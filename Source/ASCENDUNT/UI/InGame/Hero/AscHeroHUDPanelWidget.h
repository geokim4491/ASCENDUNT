#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AscHeroHUDPanelWidget.generated.h"

class AAscenduntPlayerController;
class APawn;

/**
 * Base class for the small hero-specific section hosted by the shared in-game HUD.
 *
 * This widget must not duplicate common HUD presentation such as health, ammo,
 * portrait, match clock, phase messages, or results. It only observes and presents
 * state unique to one hero.
 */
UCLASS(Abstract, Blueprintable)
class ASCENDUNT_API UAscHeroHUDPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Hero HUD")
    void InitializeHeroHUDPanel(AAscenduntPlayerController* InOwningAscPC);

    /** Called whenever the owning controller's Pawn changes, including nullptr while dead. */
    virtual void NotifyOwningPawnChanged(APawn* NewPawn);

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero HUD")
    AAscenduntPlayerController* GetOwningAscenduntPlayerController() const { return OwningAscPC; }

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero HUD")
    APawn* GetObservedPawn() const { return ObservedPawn.Get(); }

protected:
    virtual void NativeDestruct() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<AAscenduntPlayerController> OwningAscPC;

    TWeakObjectPtr<APawn> ObservedPawn;
};
