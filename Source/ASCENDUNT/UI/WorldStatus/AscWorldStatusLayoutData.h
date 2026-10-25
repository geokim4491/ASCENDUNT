#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AscWorldStatusLayoutData.generated.h"

class UUserWidget;

/**
 * Stable identifiers for world-space status widgets.
 *
 * Gameplay Tags are intentionally not used here. These are presentation slots owned by
 * one layout asset, not extensible gameplay state. Each entry attaches directly to a
 * skeletal-mesh socket configured in the data asset.
 */
UENUM(BlueprintType)
enum class EAscWorldStatusWidgetType : uint8
{
	TargetHealthBar = 0 UMETA(DisplayName = "Target Health Bar"),
	// Value 1 is intentionally unused to preserve existing serialized layout asset values.
	CriticalWarning = 2 UMETA(DisplayName = "Critical Warning"),
	DownedIndicator = 3 UMETA(DisplayName = "Downed Indicator")
};

/** Placement and rendering policy for one world-space status widget. */
USTRUCT(BlueprintType)
struct ASCENDUNT_API FAscWorldStatusWidgetLayout
{
	GENERATED_BODY()

	/** Widget class used when the presenter does not supply a feature-specific class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget")
	TSubclassOf<UUserWidget> WidgetClass;

	/**
	 * Optional skeletal-mesh component object name. NAME_None uses ACharacter::GetMesh().
	 * This exists for characters that expose more than one skeletal mesh.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName SkeletalMeshComponentName = NAME_None;

	/** Socket on the selected skeletal mesh. Missing sockets fall back to the mesh root. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName SocketName = NAME_None;

	/** Additional offset after socket attachment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** WidgetComponent pivot. Logical width/height remain owned by the WBP or its style DA. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout")
	FVector2D Pivot = FVector2D(0.5f, 0.5f);

	/** Final world-space multiplier after render-resolution compensation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.001"))
	float WorldScale = 1.0f;

	/**
	 * Render-target supersampling. The component applies the inverse scale so visible size
	 * does not change. Width and height are deliberately not duplicated in this asset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float RenderResolutionScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rendering")
	bool bFaceLocalCamera = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rendering")
	FRotator CameraFacingRotationOffset = FRotator::ZeroRotator;
};

/** Character/archetype-specific socket layout for all world-space status widgets. */
UCLASS(BlueprintType)
class ASCENDUNT_API UAscWorldStatusLayoutData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Ascendunt|World Status")
	bool FindWidgetLayout(EAscWorldStatusWidgetType WidgetType, FAscWorldStatusWidgetLayout& OutLayout) const;

	const FAscWorldStatusWidgetLayout* FindWidgetLayoutPtr(EAscWorldStatusWidgetType WidgetType) const;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|World Status")
	TMap<EAscWorldStatusWidgetType, FAscWorldStatusWidgetLayout> WidgetLayouts;
};
