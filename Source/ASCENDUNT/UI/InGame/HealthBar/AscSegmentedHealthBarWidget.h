#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/UI/InGame/HealthBar/AscHealthBarStyleData.h"
#include "AscSegmentedHealthBarWidget.generated.h"

class UAscHealthSegmentWidget;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class USizeBox;
class UTexture2D;

struct FAscHealthSegmentRuntime
{
    TWeakObjectPtr<UAscHealthSegmentWidget> Widget;
    EAscHealthPoolType PoolType = EAscHealthPoolType::BasicHealth;
    float PoolStartValue = 0.0f;
    float CellCapacity = 25.0f;
    float VisualStartX = 0.0f;
};

/**
 * Shared segmented health bar used by the local HUD and world-space target bars.
 * Each cell represents HealthPerSegment health and owns its own ProgressBar fill.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API UAscSegmentedHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UAscSegmentedHealthBarWidget(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health Bar")
    void ApplyHealthSnapshot(
        const FAscenduntHealthSnapshot& InSnapshot,
        bool bWasDamaging);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health Bar")
    void SetVisualMode(EAscSegmentedHealthBarVisualMode InVisualMode);

    /** Applies one complete centralized style and rebuilds the current layout. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health Bar|Style")
    void ApplyStyle(const FAscHealthBarStyle& InStyle);

    /** Applies a context from the shared style asset, or the tuned C++ fallback when the asset is null. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health Bar|Style")
    void ApplyStyleContext(
        const UAscHealthBarStyleSetData* StyleSet,
        EAscHealthBarStyleContext Context);

    /**
     * Multiplies only the internal render resolution/layout. The owning world-space
     * WidgetComponent applies the inverse component scale, so the visible world size stays unchanged.
     * HUD instances keep the default value of 1.
     */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health Bar|Rendering")
    void SetPresentationScale(float InPresentationScale);

    /** Clears the previous-snapshot comparison, preventing a pawn replacement from playing damage feedback. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health Bar")
    void ResetHealthDisplay();

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Health Bar")
    float GetDisplayedHealthPercent() const { return DisplayedHealthPercent; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Health Bar")
    EAscSegmentedHealthBarVisualMode GetVisualMode() const { return VisualMode; }

    /** Exact horizontal space used by the complete segmented bar. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Health Bar|Layout")
    float GetHealthBarWidth() const { return FMath::Max(1.0f, ActiveStyle.HealthBarWidth); }

    /** Total logical widget height, including room for the damage image. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Health Bar|Layout")
    float GetHealthBarHeight() const { return FMath::Max(1.0f, ActiveStyle.HealthBarHeight); }

    /** Actual render-target dimensions after world-space supersampling is applied. */
    UFUNCTION(BlueprintPure, Category = "Ascendunt|Health Bar|Rendering")
    float GetRenderedHealthBarWidth() const { return GetHealthBarWidth() * GetPresentationScale(); }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Health Bar|Rendering")
    float GetRenderedHealthBarHeight() const { return GetHealthBarHeight() * GetPresentationScale(); }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Health Bar|Rendering")
    float GetPresentationScale() const { return FMath::Max(1.0f, PresentationScale); }

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Health Bar",
        meta = (DisplayName = "On Segmented Health Changed"))
    void BP_OnSegmentedHealthChanged(
        const FAscenduntHealthSnapshot& InSnapshot,
        float InDisplayedHealthPercent);

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Health Bar",
        meta = (DisplayName = "On Damage Pulse Started"))
    void BP_OnDamagePulseStarted(float LocalX);

private:
    void EnsureNativeFallbackWidget();
    void ApplyEditorConfiguration();
    void UpdateDynamicOverHealthCapacity();
    bool DoesLayoutNeedRebuild() const;
    void RebuildSegments();
    void RefreshSegments();
    void CalculateResolvedLayoutMetrics();

    void AddFriendlyPool(EAscHealthPoolType PoolType, float Capacity);
    void AddEnemyCombinedPool(float Capacity);
    void AddPoolSegments(EAscHealthPoolType PoolType, float Capacity);

    int32 GetSegmentCount(float Capacity) const;
    int32 GetTotalSegmentCountForCurrentLayout() const;
    int32 GetFriendlyNonEmptyPoolCount() const;
    float GetPoolCurrentValue(EAscHealthPoolType PoolType) const;
    float GetDisplayCapacityTotal() const;
    float GetFriendlyPoolCapacity(EAscHealthPoolType PoolType) const;
    FLinearColor ResolveFillColor(EAscHealthPoolType PoolType) const;

    void PlayDamagePulse();
    void StopDamagePulse();
    float CalculateCurrentBoundaryX() const;
    void UpdateDamagePulseVisual(float NormalizedTime);

private:
    /** Optional WBP name: SB_HealthBar. Use it as the root SizeBox to preview the configured width/height. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> SB_HealthBar;

    /** Optional WBP name: HB_Segments. Runtime segment widgets are inserted here. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> HB_Segments;

    /** Optional WBP name: CP_DamageEffects. Must overlay HB_Segments. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCanvasPanel> CP_DamageEffects;

    /** Optional WBP name: Img_DamagePulse. Must be a direct child of CP_DamageEffects. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Img_DamagePulse;

    /** Runtime copy of the selected centralized style. UObject references remain GC-visible. */
    UPROPERTY(Transient)
    FAscHealthBarStyle ActiveStyle;

    TArray<FAscHealthSegmentRuntime> RuntimeSegments;

    FAscenduntHealthSnapshot CurrentSnapshot;

    EAscSegmentedHealthBarVisualMode VisualMode = EAscSegmentedHealthBarVisualMode::FriendlyDetailed;
    EAscSegmentedHealthBarVisualMode CachedLayoutMode = EAscSegmentedHealthBarVisualMode::FriendlyDetailed;

    float DynamicOverHealthCapacity = 0.0f;
    float CachedThornCapacity = -1.0f;
    float CachedBasicCapacity = -1.0f;
    float CachedArmorCapacity = -1.0f;
    float CachedOverHealthCapacity = -1.0f;
    float CachedEnemyTotalCapacity = -1.0f;
    float CachedConfiguredWidth = -1.0f;
    float CachedBarWidth = 0.0f;

    /** Runtime-only. HUD remains 1; world-space target bars set this from their component. */
    float PresentationScale = 1.0f;
    float ResolvedSegmentWidth = 1.0f;
    float ResolvedSegmentGap = 1.0f;
    float ResolvedPoolGap = 3.0f;
    float DisplayedHealthPercent = 0.0f;

    float DamagePulseElapsed = 0.0f;
    bool bDamagePulseActive = false;
    bool bHasSnapshot = false;
};
