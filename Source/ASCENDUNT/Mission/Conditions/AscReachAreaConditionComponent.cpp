#include "ASCENDUNT/Mission/Conditions/AscReachAreaConditionComponent.h"

#include "ASCENDUNT/Mission/World/AscObjectiveArea.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

float UAscReachAreaConditionComponent::GetCurrentProgressValue_Implementation() const
{
    return TargetArea ? static_cast<float>(TargetArea->GetPlayerCountInside()) : 0.0f;
}

float UAscReachAreaConditionComponent::GetTargetProgressValue_Implementation() const
{
    return static_cast<float>(ResolveRequiredPlayerCount());
}

void UAscReachAreaConditionComponent::OnActivated()
{
    if (!TargetArea)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Mission] ReachAreaConditionComponent has no TargetArea."));
        Fail();
        return;
    }

    TargetArea->OnPlayerEntered.RemoveDynamic(this, &UAscReachAreaConditionComponent::HandlePlayerEntered);
    TargetArea->OnPlayerExited.RemoveDynamic(this, &UAscReachAreaConditionComponent::HandlePlayerExited);
    TargetArea->OnPlayerEntered.AddDynamic(this, &UAscReachAreaConditionComponent::HandlePlayerEntered);
    TargetArea->OnPlayerExited.AddDynamic(this, &UAscReachAreaConditionComponent::HandlePlayerExited);

    EvaluateArea();
}

void UAscReachAreaConditionComponent::OnDeactivated()
{
    if (TargetArea)
    {
        TargetArea->OnPlayerEntered.RemoveDynamic(this, &UAscReachAreaConditionComponent::HandlePlayerEntered);
        TargetArea->OnPlayerExited.RemoveDynamic(this, &UAscReachAreaConditionComponent::HandlePlayerExited);
    }
}

void UAscReachAreaConditionComponent::OnReset()
{
}

void UAscReachAreaConditionComponent::HandlePlayerEntered(AAscObjectiveArea* Area, APawn* PlayerPawn)
{
    EvaluateArea();
}

void UAscReachAreaConditionComponent::HandlePlayerExited(AAscObjectiveArea* Area, APawn* PlayerPawn)
{
    NotifyProgressChanged();
}

void UAscReachAreaConditionComponent::EvaluateArea()
{
    NotifyProgressChanged();

    if (!IsConditionActive() || !TargetArea)
    {
        return;
    }

    const int32 RequiredCount = ResolveRequiredPlayerCount();
    if (RequiredCount > 0 && TargetArea->GetPlayerCountInside() >= RequiredCount)
    {
        Satisfy();
    }
}

int32 UAscReachAreaConditionComponent::ResolveRequiredPlayerCount() const
{
    if (!bRequireAllConnectedPlayers)
    {
        return FMath::Max(1, RequiredPlayerCount);
    }

    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    return GameState ? FMath::Max(1, GameState->PlayerArray.Num()) : 1;
}
