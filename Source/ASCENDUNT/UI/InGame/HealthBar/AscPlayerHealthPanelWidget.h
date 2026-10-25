#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "AscPlayerHealthPanelWidget.generated.h"

class APawn;
class UAscenduntHealthComponent;
class UAscHealthBarStyleSetData;
class UAscSegmentedHealthBarWidget;
class UTextBlock;

/**
 * Owns the local player's health presentation and delegate binding.
 * The root InGameHUD only forwards Pawn replacement notifications.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API UAscPlayerHealthPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Rebinds the panel to the health component found on NewPawn. */
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Player Health")
    void SetObservedPawn(APawn* NewPawn);

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Player Health")
    void ClearObservedPawn();

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleHealthChanged(
        UAscenduntHealthComponent* HealthComponent,
        AActor* OwningActor,
        AActor* Instigator,
        bool bWasDamaging,
        float RawValue,
        const FAscenduntHealthSnapshot& HealthSnapshot);

    /** Optional WBP name: Txt_Health. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Player Health")
    TObjectPtr<UTextBlock> Txt_Health;

    /** Optional WBP name: PlayerHealthBar. */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ascendunt|Player Health")
    TObjectPtr<UAscSegmentedHealthBarWidget> PlayerHealthBar;

    /** Assign the same centralized DA used by target health bars. Null uses tuned C++ defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Player Health|Style")
    TObjectPtr<UAscHealthBarStyleSetData> HealthBarStyleSet;

private:
    void BindHealthComponent(UAscenduntHealthComponent* NewHealthComponent);
    void UnbindHealthComponent();
    void RefreshHealth();
    void ApplyHealthSnapshotToPanel(
        const FAscenduntHealthSnapshot& HealthSnapshot,
        bool bWasDamaging);
    void ApplyPlayerStyle();

    TWeakObjectPtr<APawn> ObservedPawn;
    TWeakObjectPtr<UAscenduntHealthComponent> ObservedHealthComponent;
};
