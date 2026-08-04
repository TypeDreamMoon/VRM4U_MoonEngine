// VRM4U Copyright (c) 2021-2026 Haruyoshi Yamamoto. This software is released under the MIT License.

#include "VrmImportMaterialSet.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"


namespace {
	constexpr int32 TextureSlotNum = static_cast<int32>(EVrmMToonTextureSlot::Max);
	constexpr int32 ScalarSlotNum = static_cast<int32>(EVrmMToonScalarSlot::Max);
	constexpr int32 VectorSlotNum = static_cast<int32>(EVrmMToonVectorSlot::Max);

	// Moon VRM: parts fall back along "more specific -> less specific" so a set only has to fill in
	// what it actually differentiates. Eyebrow/Eyeline are face paint, highlights are eye paint.
	EVrmMaterialPart LocalGetParentPart(EVrmMaterialPart Part) {
		switch (Part) {
		case EVrmMaterialPart::Eyebrow:
		case EVrmMaterialPart::Eyeline:
			return EVrmMaterialPart::Face;
		case EVrmMaterialPart::EyeHighlight:
			return EVrmMaterialPart::Eye;
		case EVrmMaterialPart::Accessory:
			return EVrmMaterialPart::Cloth;
		default:
			return EVrmMaterialPart::Default;
		}
	}

	void LocalSetTexture(UMaterialInstanceConstant* Target, FName Name, UTexture2D* Texture, bool bImportMode) {
		if (Name.IsNone() || Texture == nullptr) {
			return;
		}
#if WITH_EDITOR
		if (bImportMode) {
			Target->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Name), Texture);
			return;
		}
#endif
		FTextureParameterValue* Value = Target->TextureParameterValues.FindByPredicate(
			[Name](const FTextureParameterValue& V) { return V.ParameterInfo.Name == Name; });
		if (Value == nullptr) {
			Value = new (Target->TextureParameterValues) FTextureParameterValue();
		}
		Value->ParameterInfo.Index = INDEX_NONE;
		Value->ParameterInfo.Name = Name;
		Value->ParameterInfo.Association = EMaterialParameterAssociation::GlobalParameter;
		Value->ParameterValue = Texture;
	}

	void LocalSetScalar(UMaterialInstanceConstant* Target, FName Name, float Scalar, bool bImportMode) {
		if (Name.IsNone()) {
			return;
		}
#if WITH_EDITOR
		if (bImportMode) {
			Target->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(Name), Scalar);
			return;
		}
#endif
		FScalarParameterValue* Value = Target->ScalarParameterValues.FindByPredicate(
			[Name](const FScalarParameterValue& V) { return V.ParameterInfo.Name == Name; });
		if (Value == nullptr) {
			Value = new (Target->ScalarParameterValues) FScalarParameterValue();
		}
		Value->ParameterInfo.Index = INDEX_NONE;
		Value->ParameterInfo.Name = Name;
		Value->ParameterInfo.Association = EMaterialParameterAssociation::GlobalParameter;
		Value->ParameterValue = Scalar;
	}

	void LocalSetVector(UMaterialInstanceConstant* Target, FName Name, const FLinearColor& Color, bool bImportMode) {
		if (Name.IsNone()) {
			return;
		}
#if WITH_EDITOR
		if (bImportMode) {
			Target->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(Name), Color);
			return;
		}
#endif
		FVectorParameterValue* Value = Target->VectorParameterValues.FindByPredicate(
			[Name](const FVectorParameterValue& V) { return V.ParameterInfo.Name == Name; });
		if (Value == nullptr) {
			Value = new (Target->VectorParameterValues) FVectorParameterValue();
		}
		Value->ParameterInfo.Index = INDEX_NONE;
		Value->ParameterInfo.Name = Name;
		Value->ParameterInfo.Association = EMaterialParameterAssociation::GlobalParameter;
		Value->ParameterValue = Color;
	}

	bool LocalEvaluateCondition(const FVrmStaticSwitchMapping& Mapping, const FVrmMaterialSourceParams& Source) {
		switch (Mapping.Condition) {
		case EVrmSwitchCondition::Always:
			return true;
		case EVrmSwitchCondition::WhenTextureValid:
			return Source.HasTexture(Mapping.ConditionTexture);
		case EVrmSwitchCondition::WhenTextureMissing:
			return !Source.HasTexture(Mapping.ConditionTexture);
		case EVrmSwitchCondition::WhenScalarNonZero:
			return !FMath::IsNearlyZero(Source.GetScalar(Mapping.ConditionScalar));
		case EVrmSwitchCondition::WhenTranslucent:
			return Source.bTranslucent;
		case EVrmSwitchCondition::WhenTwoSided:
			return Source.bTwoSided;
		default:
			return false;
		}
	}

	/** Collects the switches into Out, later ones overriding earlier, so parts can beat the shared table. */
	void LocalGatherSwitches(const TArray<FVrmStaticSwitchMapping>& Mappings, const FVrmMaterialSourceParams& Source, TMap<FName, bool>& Out) {
		for (const FVrmStaticSwitchMapping& Mapping : Mappings) {
			if (Mapping.TargetParameter.IsNone()) {
				continue;
			}
			const bool bPass = LocalEvaluateCondition(Mapping, Source);
			if (bPass) {
				Out.Add(Mapping.TargetParameter, Mapping.bValue);
			} else if (Mapping.bWriteWhenConditionFails) {
				Out.Add(Mapping.TargetParameter, !Mapping.bValue);
			}
		}
	}
}// namespace


// ---------------------------------------------------------------------------------------------
// FVrmImportMaterialVariant

UMaterialInterface* FVrmImportMaterialVariant::Resolve(bool bTranslucent, bool bTwoSided) const {
	// Exact slot first, then drop two-sided (the parent can still be forced two-sided), then drop
	// translucency, so a set that only fills Opaque still produces something for every material.
	UMaterialInterface* const Table[2][2] = {
		{ Opaque,      OpaqueTwoSided },
		{ Translucent, TranslucentTwoSided },
	};

	const int32 B = bTranslucent ? 1 : 0;
	const int32 S = bTwoSided ? 1 : 0;

	if (Table[B][S]) return Table[B][S];
	if (Table[B][1 - S]) return Table[B][1 - S];
	if (Table[1 - B][S]) return Table[1 - B][S];
	return Table[1 - B][1 - S];
}

bool FVrmImportMaterialVariant::IsEmpty() const {
	return Opaque == nullptr && OpaqueTwoSided == nullptr && Translucent == nullptr && TranslucentTwoSided == nullptr;
}


// ---------------------------------------------------------------------------------------------
// FVrmMaterialSourceParams

FVrmMaterialSourceParams::FVrmMaterialSourceParams() {
	Textures.SetNum(TextureSlotNum);
	Scalars.SetNumZeroed(ScalarSlotNum);
	Vectors.Init(FLinearColor::White, VectorSlotNum);
}

void FVrmMaterialSourceParams::SetTexture(EVrmMToonTextureSlot Slot, UTexture2D* Texture) {
	if (Textures.IsValidIndex(static_cast<int32>(Slot))) {
		Textures[static_cast<int32>(Slot)] = Texture;
	}
}

void FVrmMaterialSourceParams::SetScalar(EVrmMToonScalarSlot Slot, float Value) {
	if (Scalars.IsValidIndex(static_cast<int32>(Slot))) {
		Scalars[static_cast<int32>(Slot)] = Value;
	}
}

void FVrmMaterialSourceParams::SetVector(EVrmMToonVectorSlot Slot, const FLinearColor& Value) {
	if (Vectors.IsValidIndex(static_cast<int32>(Slot))) {
		Vectors[static_cast<int32>(Slot)] = Value;
	}
}

UTexture2D* FVrmMaterialSourceParams::GetTexture(EVrmMToonTextureSlot Slot) const {
	const int32 Index = static_cast<int32>(Slot);
	return Textures.IsValidIndex(Index) ? Textures[Index].Get() : nullptr;
}

float FVrmMaterialSourceParams::GetScalar(EVrmMToonScalarSlot Slot) const {
	const int32 Index = static_cast<int32>(Slot);
	return Scalars.IsValidIndex(Index) ? Scalars[Index] : 0.f;
}

FLinearColor FVrmMaterialSourceParams::GetVector(EVrmMToonVectorSlot Slot) const {
	const int32 Index = static_cast<int32>(Slot);
	return Vectors.IsValidIndex(Index) ? Vectors[Index] : FLinearColor::White;
}


// ---------------------------------------------------------------------------------------------
// UVrmImportMaterialSet

EVrmMaterialPart UVrmImportMaterialSet::ClassifyPart(const FString& VrmMaterialName) const {
	EVrmMaterialPart Best = EVrmMaterialPart::Default;
	int32 BestPriority = MIN_int32;
	int32 BestLength = -1;

	for (const FVrmMaterialPartRule& Rule : PartRules) {
		if (Rule.Pattern.IsEmpty() || !VrmMaterialName.Contains(Rule.Pattern, ESearchCase::IgnoreCase)) {
			continue;
		}
		// Longer pattern wins at equal priority: "EyeHighlight" beats "Eye" without hand-tuned numbers.
		const int32 Length = Rule.Pattern.Len();
		if (Rule.Priority > BestPriority || (Rule.Priority == BestPriority && Length > BestLength)) {
			Best = Rule.Part;
			BestPriority = Rule.Priority;
			BestLength = Length;
		}
	}
	return Best;
}

UMaterialInterface* UVrmImportMaterialSet::ResolveMaterial(EVrmMaterialPart Part, bool bTranslucent, bool bTwoSided) const {
	UMaterialInterface* const LegacyTable[2][2] = {
		{ Opaque,      OpaqueTwoSided },
		{ Translucent, TranslucentTwoSided },
	};

	if (!bUseDetailedSetup) {
		// Exactly what the importer did before: the one slot, or nothing. A stock DS_* set with a
		// hole in it is meant to skip that material, not silently borrow a neighbouring parent.
		return LegacyTable[bTranslucent ? 1 : 0][bTwoSided ? 1 : 0];
	}

	for (EVrmMaterialPart Current = Part;;) {
		if (const FVrmImportMaterialPartSetup* Setup = Parts.Find(Current)) {
			if (UMaterialInterface* Material = Setup->Materials.Resolve(bTranslucent, bTwoSided)) {
				return Material;
			}
		}
		if (Current == EVrmMaterialPart::Default) {
			break;
		}
		Current = LocalGetParentPart(Current);
	}

	// Last resort: the four legacy slots, tolerantly. A detailed set that only bothered to fill
	// Parts[Face] still needs a parent for everything else.
	FVrmImportMaterialVariant Legacy;
	Legacy.Opaque = Opaque;
	Legacy.OpaqueTwoSided = OpaqueTwoSided;
	Legacy.Translucent = Translucent;
	Legacy.TranslucentTwoSided = TranslucentTwoSided;
	return Legacy.Resolve(bTranslucent, bTwoSided);
}

void UVrmImportMaterialSet::ApplyParameters(UMaterialInstanceConstant* Target, const FVrmMaterialSourceParams& Source, EVrmMaterialPart Part, bool bImportMode) const {
	if (Target == nullptr || !HasParameterMapping()) {
		return;
	}

	// Textures.
	for (const FVrmTextureParamMapping& Mapping : ParameterMapping.Textures) {
		UTexture2D* Texture = Source.GetTexture(Mapping.Source);
		if (Texture == nullptr) {
			Texture = Source.GetTexture(Mapping.Fallback);
		}
		LocalSetTexture(Target, Mapping.TargetParameter, Texture, bImportMode);
	}

	// Scalars.
	for (const FVrmScalarParamMapping& Mapping : ParameterMapping.Scalars) {
		float Value = Source.GetScalar(Mapping.Source) * Mapping.Scale + Mapping.Bias;
		if (Mapping.bClamp01) {
			Value = FMath::Clamp(Value, 0.f, 1.f);
		}
		LocalSetScalar(Target, Mapping.TargetParameter, Value, bImportMode);
	}

	// Vectors.
	for (const FVrmVectorParamMapping& Mapping : ParameterMapping.Vectors) {
		FLinearColor Value = Source.GetVector(Mapping.Source);
		if (Mapping.bForceOpaqueAlpha) {
			Value.A = 1.f;
		}
		LocalSetVector(Target, Mapping.TargetParameter, Value, bImportMode);
	}

	// Part constants, after the shared mapping so a part can overwrite it.
	const FVrmImportMaterialPartSetup* PartSetup = nullptr;
	for (EVrmMaterialPart Current = Part;;) {
		if ((PartSetup = Parts.Find(Current)) != nullptr) {
			break;
		}
		if (Current == EVrmMaterialPart::Default) {
			break;
		}
		Current = LocalGetParentPart(Current);
	}

	if (PartSetup) {
		for (const FVrmTextureConstant& Constant : PartSetup->TextureConstants) {
			LocalSetTexture(Target, Constant.TargetParameter, Constant.Value.Get(), bImportMode);
		}
		for (const FVrmScalarConstant& Constant : PartSetup->ScalarConstants) {
			LocalSetScalar(Target, Constant.TargetParameter, Constant.Value, bImportMode);
		}
		for (const FVrmVectorConstant& Constant : PartSetup->VectorConstants) {
			LocalSetVector(Target, Constant.TargetParameter, Constant.Value, bImportMode);
		}
	}

	// Static switches. There is no per-parameter editor setter for these, so gather every change
	// and push one FStaticParameterSet: UpdateStaticPermutation recompiles, and it is not cheap.
#if WITH_EDITOR
	TMap<FName, bool> Switches;
	LocalGatherSwitches(ParameterMapping.StaticSwitches, Source, Switches);
	if (PartSetup) {
		LocalGatherSwitches(PartSetup->StaticSwitches, Source, Switches);
	}
	if (Switches.Num() == 0) {
		return;
	}

	FStaticParameterSet StaticParameters = Target->GetStaticParameters();
	bool bChanged = false;

	for (const TPair<FName, bool>& Switch : Switches) {
		const FMaterialParameterInfo Info(Switch.Key);

		// Reads through to the parent, and the GUID has to match the expression the parent compiled
		// or the override is dropped on load.
		bool bCurrentValue = false;
		FGuid ExpressionGuid;
		if (!Target->GetStaticSwitchParameterValue(Info, bCurrentValue, ExpressionGuid)) {
			// Not a switch this material owns. Skip rather than write a dangling override.
			continue;
		}
		if (bCurrentValue == Switch.Value) {
			// Already renders this way through inheritance. Overriding anyway would cost a shader
			// permutation per material for no visible difference.
			continue;
		}

		FStaticSwitchParameter* Existing = StaticParameters.StaticSwitchParameters.FindByPredicate(
			[&Info](const FStaticSwitchParameter& P) { return P.ParameterInfo == Info; });

		if (Existing == nullptr) {
			StaticParameters.StaticSwitchParameters.Emplace(Info, Switch.Value, true, ExpressionGuid);
		} else {
			Existing->Value = Switch.Value;
			Existing->bOverride = true;
			Existing->ExpressionGUID = ExpressionGuid;
		}
		bChanged = true;
	}

	if (bChanged) {
		Target->UpdateStaticPermutation(StaticParameters);
	}
#endif
}
