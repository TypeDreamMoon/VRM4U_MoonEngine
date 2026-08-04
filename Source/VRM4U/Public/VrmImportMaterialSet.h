// VRM4U Copyright (c) 2021-2026 Haruyoshi Yamamoto. This software is released under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VrmImportMaterialSet.generated.h"

class UMaterialInterface;
class UMaterialInstanceConstant;
class UTexture2D;

// ---------------------------------------------------------------------------------------------
// Moon VRM
//
// The importer used to know exactly one material vocabulary: the `mtoon_*` / `gltf_*` parameter
// names baked into VRM4U's own MToon materials. A material set that speaks a different vocabulary
// (MoonToon calls its base map "Base Color Map", not "mtoon_tex_MainTex") got a parent assigned and
// nothing else, because SetTextureParameterValueEditorOnly silently drops unknown names.
//
// So a set now carries three things beyond the four legacy slots:
//   * per-part material slots  - one blend-mode variant per body part, picked by name rules
//   * a parameter mapping      - MToon source slot -> target parameter name, authored in the asset
//   * per-part constants       - values only that part wants (Face turns on the SDF facial shadow)
//
// Everything is additive. A set that leaves bUseDetailedSetup off behaves exactly as before, which
// is what the fifteen DS_* assets shipped with VRM4U rely on.
// ---------------------------------------------------------------------------------------------

/** Which part of the character a VRM material paints. Drives both parent choice and constants. */
UENUM(BlueprintType)
enum class EVrmMaterialPart : uint8
{
	Default			UMETA(DisplayName = "Default"),
	Face			UMETA(DisplayName = "Face"),
	Eyebrow			UMETA(DisplayName = "Eyebrow"),
	Eyeline			UMETA(DisplayName = "Eyeline"),
	Eye				UMETA(DisplayName = "Eye"),
	EyeHighlight	UMETA(DisplayName = "Eye Highlight"),
	Hair			UMETA(DisplayName = "Hair"),
	Skin			UMETA(DisplayName = "Skin / Body"),
	Cloth			UMETA(DisplayName = "Cloth"),
	Accessory		UMETA(DisplayName = "Accessory"),
	Outline			UMETA(DisplayName = "Outline"),

	Max				UMETA(Hidden),
};

/** MToon texture properties, in the order VRM stores them. */
UENUM(BlueprintType)
enum class EVrmMToonTextureSlot : uint8
{
	MainTex					UMETA(DisplayName = "_MainTex"),
	ShadeTexture			UMETA(DisplayName = "_ShadeTexture"),
	BumpMap					UMETA(DisplayName = "_BumpMap"),
	ReceiveShadowTexture	UMETA(DisplayName = "_ReceiveShadowTexture"),
	ShadingGradeTexture		UMETA(DisplayName = "_ShadingGradeTexture"),
	RimTexture				UMETA(DisplayName = "_RimTexture"),
	SphereAdd				UMETA(DisplayName = "_SphereAdd"),
	EmissionMap				UMETA(DisplayName = "_EmissionMap"),
	OutlineWidthTexture		UMETA(DisplayName = "_OutlineWidthTexture"),
	UvAnimMaskTexture		UMETA(DisplayName = "_UvAnimMaskTexture"),

	None					UMETA(DisplayName = "(none)"),
	Max						UMETA(Hidden),
};

/** MToon float properties. */
UENUM(BlueprintType)
enum class EVrmMToonScalarSlot : uint8
{
	Cutoff					UMETA(DisplayName = "_Cutoff"),
	BumpScale				UMETA(DisplayName = "_BumpScale"),
	ReceiveShadowRate		UMETA(DisplayName = "_ReceiveShadowRate"),
	ShadeShift				UMETA(DisplayName = "_ShadeShift"),
	ShadeToony				UMETA(DisplayName = "_ShadeToony"),
	LightColorAttenuation	UMETA(DisplayName = "_LightColorAttenuation"),
	IndirectLightIntensity	UMETA(DisplayName = "_IndirectLightIntensity"),
	RimLightingMix			UMETA(DisplayName = "_RimLightingMix"),
	RimFresnelPower			UMETA(DisplayName = "_RimFresnelPower"),
	RimLift					UMETA(DisplayName = "_RimLift"),
	OutlineWidth			UMETA(DisplayName = "_OutlineWidth"),
	OutlineScaledMaxDistance UMETA(DisplayName = "_OutlineScaledMaxDistance"),
	OutlineLightingMix		UMETA(DisplayName = "_OutlineLightingMix"),
	UvAnimScrollX			UMETA(DisplayName = "_UvAnimScrollX"),
	UvAnimScrollY			UMETA(DisplayName = "_UvAnimScrollY"),
	UvAnimRotation			UMETA(DisplayName = "_UvAnimRotation"),
	BlendMode				UMETA(DisplayName = "_BlendMode"),
	OutlineWidthMode		UMETA(DisplayName = "_OutlineWidthMode"),
	OutlineColorMode		UMETA(DisplayName = "_OutlineColorMode"),
	CullMode				UMETA(DisplayName = "_CullMode"),
	OutlineCullMode			UMETA(DisplayName = "_OutlineCullMode"),
	ZWrite					UMETA(DisplayName = "_ZWrite"),

	/** _Color.a. MToon multiplies opacity by it, so it is far more useful here than in the vector. */
	ColorAlpha				UMETA(DisplayName = "_Color.a"),

	None					UMETA(DisplayName = "(none)"),
	Max						UMETA(Hidden),
};

/** MToon vector properties. */
UENUM(BlueprintType)
enum class EVrmMToonVectorSlot : uint8
{
	Color			UMETA(DisplayName = "_Color"),
	ShadeColor		UMETA(DisplayName = "_ShadeColor"),
	RimColor		UMETA(DisplayName = "_RimColor"),
	EmissionColor	UMETA(DisplayName = "_EmissionColor"),
	OutlineColor	UMETA(DisplayName = "_OutlineColor"),

	None			UMETA(DisplayName = "(none)"),
	Max				UMETA(Hidden),
};

/** When a static switch mapping should fire. */
UENUM(BlueprintType)
enum class EVrmSwitchCondition : uint8
{
	Always				UMETA(DisplayName = "Always"),
	WhenTextureValid	UMETA(DisplayName = "When source texture exists"),
	WhenTextureMissing	UMETA(DisplayName = "When source texture is missing"),
	WhenScalarNonZero	UMETA(DisplayName = "When source scalar != 0"),
	WhenTranslucent		UMETA(DisplayName = "When material is translucent"),
	WhenTwoSided		UMETA(DisplayName = "When material is two sided"),

	Max					UMETA(Hidden),
};


/** The four parents one part can be built from, chosen by blend mode and culling. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmImportMaterialVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UMaterialInterface> Opaque;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UMaterialInterface> OpaqueTwoSided;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UMaterialInterface> Translucent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UMaterialInterface> TranslucentTwoSided;

	/** Picks the closest filled slot; falls back along two-sided then blend mode. Null if all empty. */
	UMaterialInterface* Resolve(bool bTranslucent, bool bTwoSided) const;

	bool IsEmpty() const;
};


/** One `VRM material name contains Pattern -> this part` rule. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmMaterialPartRule
{
	GENERATED_BODY()

	/** Case-insensitive substring tested against the VRM material name, e.g. "EyeHighlight". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FString Pattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	EVrmMaterialPart Part = EVrmMaterialPart::Default;

	/** Higher wins. Ties break on the longer pattern, so "EyeHighlight" beats "Eye" for free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	int32 Priority = 0;
};


/** MToon texture slot -> target texture parameter. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmTextureParamMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EVrmMToonTextureSlot Source = EVrmMToonTextureSlot::MainTex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FName TargetParameter;

	/** Used when Source is absent on this material. MToon leaves _ShadeTexture empty a lot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EVrmMToonTextureSlot Fallback = EVrmMToonTextureSlot::None;
};


/** MToon float slot -> target scalar parameter, with an affine remap. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmScalarParamMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EVrmMToonScalarSlot Source = EVrmMToonScalarSlot::Cutoff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FName TargetParameter;

	/** Written value is Source * Scale + Bias. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	float Scale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	float Bias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	bool bClamp01 = false;
};


/** MToon vector slot -> target vector parameter. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmVectorParamMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EVrmMToonVectorSlot Source = EVrmMToonVectorSlot::Color;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FName TargetParameter;

	/** MToon stashes unrelated data in alpha (_RimColor.a is a strength). Force it to 1 for colours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	bool bForceOpaqueAlpha = false;
};


/** A static switch to drive from the source material's contents. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmStaticSwitchMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	FName TargetParameter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EVrmSwitchCondition Condition = EVrmSwitchCondition::Always;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EVrmMToonTextureSlot ConditionTexture = EVrmMToonTextureSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	EVrmMToonScalarSlot ConditionScalar = EVrmMToonScalarSlot::None;

	/** Value written when the condition passes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	bool bValue = true;

	/** Also write !bValue when it fails, instead of leaving the parent's default alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	bool bWriteWhenConditionFails = false;
};


USTRUCT(BlueprintType)
struct VRM4U_API FVrmScalarConstant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constant")
	FName TargetParameter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constant")
	float Value = 0.f;
};

USTRUCT(BlueprintType)
struct VRM4U_API FVrmVectorConstant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constant")
	FName TargetParameter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constant")
	FLinearColor Value = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct VRM4U_API FVrmTextureConstant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constant")
	FName TargetParameter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constant")
	TObjectPtr<UTexture2D> Value;
};


/** The whole MToon -> target vocabulary, authored in the data asset. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmMaterialParameterMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	TArray<FVrmTextureParamMapping> Textures;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	TArray<FVrmScalarParamMapping> Scalars;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	TArray<FVrmVectorParamMapping> Vectors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mapping")
	TArray<FVrmStaticSwitchMapping> StaticSwitches;

	bool IsEmpty() const
	{
		return Textures.Num() == 0 && Scalars.Num() == 0 && Vectors.Num() == 0 && StaticSwitches.Num() == 0;
	}
};


/** Parents plus part-only overrides for one body part. */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmImportMaterialPartSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FVrmImportMaterialVariant Materials;

	/** Applied after the shared mapping, so a part can force a feature on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	TArray<FVrmStaticSwitchMapping> StaticSwitches;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	TArray<FVrmScalarConstant> ScalarConstants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	TArray<FVrmVectorConstant> VectorConstants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	TArray<FVrmTextureConstant> TextureConstants;
};


/**
 * One VRM material flattened into engine-side values, so the data asset never has to see assimp.
 * The loader fills this; UVrmImportMaterialSet reads it.
 */
USTRUCT(BlueprintType)
struct VRM4U_API FVrmMaterialSourceParams
{
	GENERATED_BODY()

	FVrmMaterialSourceParams();

	UPROPERTY(BlueprintReadWrite, Category = "Source")
	FString MaterialName;

	UPROPERTY(BlueprintReadWrite, Category = "Source")
	bool bTranslucent = false;

	UPROPERTY(BlueprintReadWrite, Category = "Source")
	bool bTwoSided = false;

	/** True for alphaMode OPAQUE. Masked and translucent both leave this false. */
	UPROPERTY(BlueprintReadWrite, Category = "Source")
	bool bOpaque = false;

	/** One entry per EVrmMToonTextureSlot; null where the source has no texture. */
	UPROPERTY(BlueprintReadWrite, Category = "Source")
	TArray<TObjectPtr<UTexture2D>> Textures;

	/** One entry per EVrmMToonScalarSlot. */
	UPROPERTY(BlueprintReadWrite, Category = "Source")
	TArray<float> Scalars;

	/** One entry per EVrmMToonVectorSlot. */
	UPROPERTY(BlueprintReadWrite, Category = "Source")
	TArray<FLinearColor> Vectors;

	void SetTexture(EVrmMToonTextureSlot Slot, UTexture2D* Texture);
	void SetScalar(EVrmMToonScalarSlot Slot, float Value);
	void SetVector(EVrmMToonVectorSlot Slot, const FLinearColor& Value);

	UTexture2D* GetTexture(EVrmMToonTextureSlot Slot) const;
	float GetScalar(EVrmMToonScalarSlot Slot) const;
	FLinearColor GetVector(EVrmMToonVectorSlot Slot) const;

	bool HasTexture(EVrmMToonTextureSlot Slot) const { return GetTexture(Slot) != nullptr; }
};


UCLASS(BlueprintType)
class VRM4U_API UVrmImportMaterialSet : public UDataAsset{

	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	UMaterialInterface* Opaque;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	UMaterialInterface* OpaqueTwoSided;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	UMaterialInterface* Translucent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	UMaterialInterface* TranslucentTwoSided;

	// Moon VRM ------------------------------------------------------------------------------

	/** Off keeps the four slots above as the only behaviour, which is what every stock DS_* set wants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detailed")
	bool bUseDetailedSetup = false;

	/** Parents per body part. A part with no entry, or an empty slot, falls back to Default then to the four legacy slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detailed", meta = (EditCondition = "bUseDetailedSetup"))
	TMap<EVrmMaterialPart, FVrmImportMaterialPartSetup> Parts;

	/** VRM material name -> part. Evaluated highest priority first; no match means Default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detailed", meta = (EditCondition = "bUseDetailedSetup"))
	TArray<FVrmMaterialPartRule> PartRules;

	/** How MToon properties reach this set's parameter names. Empty means "leave the instance alone". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detailed", meta = (EditCondition = "bUseDetailedSetup"))
	FVrmMaterialParameterMapping ParameterMapping;

	/** Which part a VRM material name belongs to. Default when nothing matches. */
	UFUNCTION(BlueprintCallable, Category = "VRM4U")
	EVrmMaterialPart ClassifyPart(const FString& VrmMaterialName) const;

	/** Parent material for a part and blend mode, walking part -> Default -> legacy slots. */
	UFUNCTION(BlueprintCallable, Category = "VRM4U")
	UMaterialInterface* ResolveMaterial(EVrmMaterialPart Part, bool bTranslucent, bool bTwoSided) const;

	/** True when this set knows how to translate MToon values into its own parameter names. */
	bool HasParameterMapping() const { return bUseDetailedSetup && !ParameterMapping.IsEmpty(); }

	/**
	 * Writes the mapped parameters onto Target. Static switches are batched into a single
	 * UpdateStaticPermutation, so this is the expensive call in the import loop, once per material.
	 * bImportMode selects the editor-only setters over the raw parameter arrays.
	 */
	UFUNCTION(BlueprintCallable, Category = "VRM4U")
	void ApplyParameters(UMaterialInstanceConstant* Target, const FVrmMaterialSourceParams& Source, EVrmMaterialPart Part, bool bImportMode) const;

	// Moon End ------------------------------------------------------------------------------
};
