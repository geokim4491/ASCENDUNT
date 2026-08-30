#include "ASCENDUNT/Mission/Conditions/AscInteractionConditionComponent.h"

#include "ASCENDUNT/Interaction/AscInteractable.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

float UAscInteractionConditionComponent::GetCurrentProgressValue_Implementation() const
{
    return static_cast<float>(CurrentInteractionCount);
}

float UAscInteractionConditionComponent::GetTargetProgressValue_Implementation() const
{
    return static_cast<float>(GetRequiredInteractionCount());
}

void UAscInteractionConditionComponent::OnActivated()
{
    BindInteractionEvent();
    NotifyProgressChanged();

    const int32 RequiredCount = GetRequiredInteractionCount();
    if (RequiredCount <= 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Mission] Interaction condition '%s' in objective '%s' has no interaction targets."),
            *GetName(),
            *GetNameSafe(GetOwner()));
        return;
    }

    TSet<const AActor*> SeenTargets;
    for (const AActor* Target : InteractionTargets)
    {
        if (!IsValid(Target)
            || !Target->GetClass()->ImplementsInterface(UAscInteractable::StaticClass()))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Mission] Interaction condition '%s' in objective '%s' contains an invalid/non-interactable target."),
                *GetName(),
                *GetNameSafe(GetOwner()));
            continue;
        }

        if (SeenTargets.Contains(Target))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Mission] Interaction condition '%s' in objective '%s' contains duplicate target '%s'."),
                *GetName(),
                *GetNameSafe(GetOwner()),
                *GetNameSafe(Target));
        }
        SeenTargets.Add(Target);
    }

    if (CurrentInteractionCount >= RequiredCount)
    {
        Satisfy();
    }
}

void UAscInteractionConditionComponent::OnDeactivated()
{
    UnbindInteractionEvent();
}

void UAscInteractionConditionComponent::OnReset()
{
    CurrentInteractionCount = 0;
    CompletedTargets.Reset();
}

void UAscInteractionConditionComponent::HandleInteractionCompleted(
    UWorld* InteractionWorld,
    AActor* Target,
    AHeroCharacter* Interactor)
{
    if (!IsConditionActive() || InteractionWorld != GetWorld() || !Target)
    {
        return;
    }

    if (!InteractionTargets.Contains(Target) || CompletedTargets.Contains(Target))
    {
        return;
    }

    // The array is editor-filtered, but keep runtime validation for references
    // assigned from C++ or stale assets.
    if (!Target->GetClass()->ImplementsInterface(UAscInteractable::StaticClass()))
    {
        return;
    }

    CompletedTargets.Add(Target);
    CurrentInteractionCount = CompletedTargets.Num();
    NotifyProgressChanged();

    if (CurrentInteractionCount >= GetRequiredInteractionCount())
    {
        Satisfy();
    }
}

int32 UAscInteractionConditionComponent::GetRequiredInteractionCount() const
{
    // The design intentionally uses the configured array length as the goal:
    // one configured interactable == one required completion.
    return InteractionTargets.Num();
}

void UAscInteractionConditionComponent::BindInteractionEvent()
{
    UnbindInteractionEvent();
    FAscInteractionEvents::OnInteractionCompleted().AddUObject(
        this,
        &UAscInteractionConditionComponent::HandleInteractionCompleted);
}

void UAscInteractionConditionComponent::UnbindInteractionEvent()
{
    FAscInteractionEvents::OnInteractionCompleted().RemoveAll(this);
}
