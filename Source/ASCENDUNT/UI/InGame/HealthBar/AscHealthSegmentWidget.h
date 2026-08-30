#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCENDUNT/UI/InGame/HealthBar/AscSegmentedHealthBarTypes.h"
#include "AscHealthSegmentWidget.generated.h"

class UImage;
class UProgressBar;

/** One 25-HP cell. The background is an Image and the changing fill is a ProgressBar. */
UCLASS(Blueprintable)
class ASCENDUNT_API UAscHealthSegmentWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health Bar")
    void SetSegmentVisual(
        EAscHealthPoolType InPoolType,
        float InFillPercent,
        const FLinearColor& InFillColor,
        bool bInEnemyStyle);

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeOnInitialized() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Health Bar",
        meta = (DisplayName = "On Segment Visual Changed"))
    void BP_OnSegmentVisualChanged(
        EAscHealthPoolType InPoolType,
        float InFillPercent,
        FLinearColor InFillColor,
        bool bInEnemyStyle);

private:
    void EnsureNativeFallbackWidget();
    void ApplyFlatNoOutlineStyle();

private:
    /** Optional WBP name: Img_Background. Assign the segment background art here. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Img_Background;

    /** Optional WBP name: PB_Fill. This must remain a ProgressBar for partial cell depletion. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_Fill;

    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Health Bar|Style")
    FLinearColor FriendlyBackgroundTint = FLinearColor(0.035f, 0.035f, 0.045f, 0.95f);

    UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Health Bar|Style")
    FLinearColor EnemyBackgroundTint = FLinearColor(0.08f, 0.015f, 0.015f, 0.95f);
};
