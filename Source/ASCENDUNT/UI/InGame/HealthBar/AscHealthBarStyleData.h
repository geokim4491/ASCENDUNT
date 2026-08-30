#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASCENDUNT/UI/InGame/HealthBar/AscSegmentedHealthBarTypes.h"
#include "AscHealthBarStyleData.generated.h"

class UAscHealthSegmentWidget;
class UTexture2D;

/** Selects one centralized health-bar presentation profile. */
UENUM(BlueprintType)
enum class EAscHealthBarStyleContext : uint8
{
    PlayerHUD,
    FriendlyTarget,
    EnemyTarget
};

/**
 * Complete visual configuration for one segmented health-bar context.
 * Gameplay health values are intentionally excluded from this structure.
 */
USTRUCT(BlueprintType)
struct ASCENDUNT_API FAscHealthBarStyle
{
    GENERATED_BODY()

    FAscHealthBarStyle();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Segments")
    EAscSegmentedHealthBarVisualMode VisualMode = EAscSegmentedHealthBarVisualMode::FriendlyDetailed;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Segments")
    TSubclassOf<UAscHealthSegmentWidget> SegmentWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Segments", meta = (ClampMin = "1.0"))
    float HealthPerSegment = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1.0"))
    float HealthBarWidth = 180.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1.0"))
    float HealthBarHeight = 48.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.5"))
    float MinSegmentWidth = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1.0"))
    float SegmentHeight = 12.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
    float SegmentGap = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0.0"))
    float PoolGap = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Colors")
    FLinearColor BasicHealthColor = FLinearColor::White;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Colors")
    FLinearColor ArmorColor = FLinearColor(1.0f, 0.78f, 0.05f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Colors")
    FLinearColor ThornColor = FLinearColor(0.58f, 0.18f, 0.95f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Colors")
    FLinearColor OverHealthColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Colors")
    FLinearColor EnemyHealthColor = FLinearColor(0.95f, 0.03f, 0.025f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    bool bEnableDamagePulse = true;

    /** Leave null to use the brush configured directly on Img_DamagePulse in the WBP. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    TObjectPtr<UTexture2D> DamagePulseTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    FLinearColor DamagePulseColor = FLinearColor::Red;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback", meta = (ClampMin = "1.0"))
    FVector2D DamagePulseDrawSize = FVector2D(3.0f, 16.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    FVector2D DamagePulsePositionOffset = FVector2D::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    FVector2D DamagePulsePivot = FVector2D(0.5f, 0.5f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    float DamagePulseRenderAngle = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback", meta = (ClampMin = "0.01"))
    float DamagePulseDuration = 0.16f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    FVector2D DamagePulseStartScale = FVector2D(1.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback")
    FVector2D DamagePulseEndScale = FVector2D(1.0f, 3.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DamagePulseStartOpacity = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DamagePulseEndOpacity = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback", meta = (ClampMin = "0.01"))
    float DamagePulseScaleExponent = 0.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage Feedback", meta = (ClampMin = "0.01"))
    float DamagePulseFadeExponent = 1.0f;
};

/** One asset owns all player, ally-target, and enemy-target health-bar styles. */
UCLASS(BlueprintType)
class ASCENDUNT_API UAscHealthBarStyleSetData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UAscHealthBarStyleSetData(const FObjectInitializer& ObjectInitializer);

    const FAscHealthBarStyle& GetStyle(EAscHealthBarStyleContext Context) const;

    /** C++ fallback used before an asset is assigned. Values match the current tuned WBP settings. */
    static FAscHealthBarStyle MakeDefaultStyle(EAscHealthBarStyleContext Context);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar Styles")
    FAscHealthBarStyle PlayerHUD;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar Styles")
    FAscHealthBarStyle FriendlyTarget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar Styles")
    FAscHealthBarStyle EnemyTarget;
};
