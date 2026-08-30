#include "ASCENDUNT/Mission/Conditions/AscKillCountConditionComponent.h"

#include "ASCENDUNT/AI/AISpawner.h"
#include "EngineUtils.h"
#include "Engine/World.h"

float UAscKillCountConditionComponent::GetCurrentProgressValue_Implementation() const
{
    return static_cast<float>(CurrentKillCount);
}

float UAscKillCountConditionComponent::GetTargetProgressValue_Implementation() const
{
    return static_cast<float>(FMath::Max(1, TargetKillCount));
}

void UAscKillCountConditionComponent::OnActivated()
{
    BindSpawners();
    NotifyProgressChanged();

    if (CurrentKillCount >= FMath::Max(1, TargetKillCount))
    {
        Satisfy();
    }
}

void UAscKillCountConditionComponent::OnDeactivated()
{
    UnbindSpawners();
}

void UAscKillCountConditionComponent::OnReset()
{
    CurrentKillCount = 0;
    BoundSpawners.Reset();
}

void UAscKillCountConditionComponent::HandleMonsterDied(AActor* DeadActor)
{
    if (!IsConditionActive())
    {
        return;
    }

    ++CurrentKillCount;
    NotifyProgressChanged();

    if (CurrentKillCount >= FMath::Max(1, TargetKillCount))
    {
        Satisfy();
    }
}

void UAscKillCountConditionComponent::BindSpawners()
{
    UnbindSpawners();

    for (AAISpawner* Spawner : ObservedSpawners)
    {
        if (Spawner)
        {
            BoundSpawners.AddUnique(Spawner);
        }
    }

    if (BoundSpawners.IsEmpty() && bObserveAllSpawnersIfNoneAssigned)
    {
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<AAISpawner> It(World); It; ++It)
            {
                BoundSpawners.Add(*It);
            }
        }
    }

    for (AAISpawner* Spawner : BoundSpawners)
    {
        if (Spawner)
        {
            Spawner->OnMonsterDiedNative.AddUObject(this, &UAscKillCountConditionComponent::HandleMonsterDied);
        }
    }
}

void UAscKillCountConditionComponent::UnbindSpawners()
{
    for (AAISpawner* Spawner : BoundSpawners)
    {
        if (Spawner)
        {
            Spawner->OnMonsterDiedNative.RemoveAll(this);
        }
    }

    BoundSpawners.Reset();
}
