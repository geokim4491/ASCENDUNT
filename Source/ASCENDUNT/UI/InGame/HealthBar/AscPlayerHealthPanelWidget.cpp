#include "AscPlayerHealthPanelWidget.h"

#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/UI/InGame/HealthBar/AscHealthBarStyleData.h"
#include "ASCENDUNT/UI/InGame/HealthBar/AscSegmentedHealthBarWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UAscPlayerHealthPanelWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyPlayerStyle();
}

void UAscPlayerHealthPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    ApplyPlayerStyle();
}

void UAscPlayerHealthPanelWidget::NativeDestruct()
{
    ClearObservedPawn();
    Super::NativeDestruct();
}

void UAscPlayerHealthPanelWidget::SetObservedPawn(APawn* NewPawn)
{
    if (ObservedPawn.Get() == NewPawn)
    {
        RefreshHealth();
        return;
    }

    ObservedPawn = NewPawn;
    BindHealthComponent(UAscenduntHealthComponent::FindHealthComponent(NewPawn));
}

void UAscPlayerHealthPanelWidget::ClearObservedPawn()
{
    UnbindHealthComponent();
    ObservedPawn.Reset();

    if (PlayerHealthBar)
    {
        PlayerHealthBar->ResetHealthDisplay();
    }

    ApplyHealthSnapshotToPanel(FAscenduntHealthSnapshot(), false);
}

void UAscPlayerHealthPanelWidget::HandleHealthChanged(
    UAscenduntHealthComponent* HealthComponent,
    AActor* OwningActor,
    AActor* Instigator,
    bool bWasDamaging,
    float RawValue,
    const FAscenduntHealthSnapshot& HealthSnapshot)
{
    if (HealthComponent != ObservedHealthComponent.Get())
    {
        return;
    }

    ApplyHealthSnapshotToPanel(HealthSnapshot, bWasDamaging);
}

void UAscPlayerHealthPanelWidget::BindHealthComponent(
    UAscenduntHealthComponent* NewHealthComponent)
{
    if (ObservedHealthComponent.Get() == NewHealthComponent)
    {
        RefreshHealth();
        return;
    }

    UnbindHealthComponent();
    ObservedHealthComponent = NewHealthComponent;

    if (NewHealthComponent)
    {
        NewHealthComponent->OnHealthChanged.RemoveDynamic(
            this,
            &ThisClass::HandleHealthChanged);
        NewHealthComponent->OnHealthChanged.AddDynamic(
            this,
            &ThisClass::HandleHealthChanged);
    }

    if (PlayerHealthBar)
    {
        PlayerHealthBar->ResetHealthDisplay();
    }

    RefreshHealth();
}

void UAscPlayerHealthPanelWidget::UnbindHealthComponent()
{
    if (UAscenduntHealthComponent* HealthComponent = ObservedHealthComponent.Get())
    {
        HealthComponent->OnHealthChanged.RemoveDynamic(
            this,
            &ThisClass::HandleHealthChanged);
    }

    ObservedHealthComponent.Reset();
}

void UAscPlayerHealthPanelWidget::RefreshHealth()
{
    FAscenduntHealthSnapshot Snapshot;
    if (const UAscenduntHealthComponent* HealthComponent = ObservedHealthComponent.Get())
    {
        if (HealthComponent->GetHealthSnapshot(Snapshot))
        {
            ApplyHealthSnapshotToPanel(Snapshot, false);
            return;
        }
    }

    ApplyHealthSnapshotToPanel(FAscenduntHealthSnapshot(), false);
}

void UAscPlayerHealthPanelWidget::ApplyHealthSnapshotToPanel(
    const FAscenduntHealthSnapshot& HealthSnapshot,
    bool bWasDamaging)
{
    if (Txt_Health)
    {
        Txt_Health->SetText(FText::Format(
            FText::FromString(TEXT("{0} / {1}")),
            FText::AsNumber(FMath::RoundToInt(HealthSnapshot.GetCurrentTotal())),
            FText::AsNumber(FMath::RoundToInt(HealthSnapshot.GetDisplayCapacityTotal()))));
    }

    if (PlayerHealthBar)
    {
        PlayerHealthBar->ApplyHealthSnapshot(HealthSnapshot, bWasDamaging);
    }
}

void UAscPlayerHealthPanelWidget::ApplyPlayerStyle()
{
    if (PlayerHealthBar)
    {
        PlayerHealthBar->ApplyStyleContext(
            HealthBarStyleSet,
            EAscHealthBarStyleContext::PlayerHUD);
    }
}
