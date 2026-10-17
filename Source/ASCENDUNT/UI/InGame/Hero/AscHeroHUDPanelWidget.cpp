#include "AscHeroHUDPanelWidget.h"

#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "GameFramework/Pawn.h"

void UAscHeroHUDPanelWidget::InitializeHeroHUDPanel(AAscenduntPlayerController* InOwningAscPC)
{
    OwningAscPC = InOwningAscPC
        ? InOwningAscPC
        : Cast<AAscenduntPlayerController>(GetOwningPlayer());
}

void UAscHeroHUDPanelWidget::NotifyOwningPawnChanged(APawn* NewPawn)
{
    ObservedPawn = NewPawn;
}

void UAscHeroHUDPanelWidget::NativeDestruct()
{
    ObservedPawn.Reset();
    OwningAscPC = nullptr;

    Super::NativeDestruct();
}
