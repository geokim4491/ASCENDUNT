#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "AscAbilitySlotWidget.generated.h"

class UAscenduntAbilitySystemComponent;
class UImage;
class UProgressBar;
class UTextBlock;
class UWidget;
class UCanvasPanel;

/**
 * Generic skill icon presenter.
 *
 * WBP layout contract (all optional):
 *  - Image_AbilityIcon: automatically filled from the ability DataAsset
 *  - Txt_Stock: current charge count (2 -> 1 -> 0)
 *  - CooldownOverlay: yellow overlay container placed above the skill icon
 *  - Progress_CooldownOverlay: overlay fill, percent decreases 1 -> 0
 *  - Txt_CooldownRemaining: remaining seconds shown over the icon
 *
 * Configure the ProgressBar style/color in WBP. For a radial wipe, replace the visual in BP
 * with a material and use BP_OnAbilityUIStateChanged's OverlayPercent.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API UAscAbilitySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability UI")
    void SetAbilityInputTag(FGameplayTag InInputTag);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Ability UI")
    void RefreshAbilityUI();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UAscenduntAbilitySystemComponent* ResolveASC() const;

    UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Ability UI")
    void BP_OnAbilityUIStateChanged(bool bUsesStock, int32 CurrentStock, int32 MaxStock,
        bool bShowCooldownOverlay, float TimeRemaining, float Duration, float OverlayPercent);

protected:
    /** InputTag assigned to this ability in the AbilitySet. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ascendunt|Ability UI", meta = (ExposeOnSpawn = "true"))
    FGameplayTag AbilityInputTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Slot")
    FText KeyText;

    UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    TObjectPtr<UCanvasPanel> SlotRoot;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    TObjectPtr<UImage> Image_AbilityIcon;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    TObjectPtr<UTextBlock> Txt_Stock;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    TObjectPtr<UTextBlock> Txt_Key;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    TObjectPtr<UWidget> CooldownOverlay;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    TObjectPtr<UProgressBar> Progress_CooldownOverlay;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    TObjectPtr<UTextBlock> Txt_CooldownRemaining;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Ability UI")
    FLinearColor CooldownOverlayColor = FLinearColor(1.0f, 0.85f, 0.0f, 0.65f);
};
