#include "ASCENDUNT/Mission/Conditions/AscSpawnerEncounterConditionComponent.h"

#include "ASCENDUNT/AI/AISpawner.h"
#include "EngineUtils.h"
#include "Engine/World.h"

float UAscSpawnerEncounterConditionComponent::GetCurrentProgressValue_Implementation() const
{
	return static_cast<float>(CurrentKillCount);
}

float UAscSpawnerEncounterConditionComponent::GetTargetProgressValue_Implementation() const
{
	return static_cast<float>(FMath::Max(1, ResolvedTargetKillCount));
}

void UAscSpawnerEncounterConditionComponent::OnActivated()
{
	BindSpawners();
	RefreshAlreadyCompletedSpawners();
	NotifyProgressChanged();

	// 먼저 Delegate를 Bind한 뒤 Activate해야 즉시 완료되는 Encounter도 놓치지 않는다.
	if (bActivateSpawnersOnConditionStart)
	{
		for (AAISpawner* Spawner : BoundSpawners)
		{
			if (Spawner && !Spawner->IsEncounterCompleted())
			{
				Spawner->ActivateSpawner();
			}
		}
	}

	RefreshAlreadyCompletedSpawners();
	NotifyProgressChanged();
	EvaluateCompletion();
}

void UAscSpawnerEncounterConditionComponent::OnDeactivated()
{
	if (bStopSpawnersOnConditionEnd)
	{
		for (AAISpawner* Spawner : BoundSpawners)
		{
			if (Spawner)
			{
				Spawner->StopSpawner();
			}
		}
	}

	UnbindSpawners();
}

void UAscSpawnerEncounterConditionComponent::OnReset()
{
	CompletedSpawners.Reset();
	BoundSpawners.Reset();
	CurrentKillCount = 0;
	ResolvedTargetKillCount = 1;
	bHasUnboundedKillTarget = false;
}

void UAscSpawnerEncounterConditionComponent::HandleMonsterDied(AActor* DeadActor)
{
	if (!IsConditionActive() || !DeadActor)
	{
		return;
	}

	++CurrentKillCount;
	NotifyProgressChanged();
}

void UAscSpawnerEncounterConditionComponent::HandleEncounterCompleted(AAISpawner* Spawner)
{
	if (!IsConditionActive() || !Spawner || !BoundSpawners.Contains(Spawner))
	{
		return;
	}

	CompletedSpawners.AddUnique(Spawner);
	NotifyProgressChanged();
	EvaluateCompletion();
}

void UAscSpawnerEncounterConditionComponent::BindSpawners()
{
	UnbindSpawners();
	CompletedSpawners.Reset();

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
				BoundSpawners.AddUnique(*It);
			}
		}
	}

	ResolvedTargetKillCount = 0;
	bHasUnboundedKillTarget = false;

	for (AAISpawner* Spawner : BoundSpawners)
	{
		if (!Spawner)
		{
			continue;
		}

		const int32 SpawnerTargetKillCount = Spawner->GetEncounterTargetKillCount();
		if (SpawnerTargetKillCount == INDEX_NONE)
		{
			bHasUnboundedKillTarget = true;
		}
		else
		{
			ResolvedTargetKillCount += SpawnerTargetKillCount;
		}

		Spawner->OnMonsterDiedNative.AddUObject(
			this,
			&UAscSpawnerEncounterConditionComponent::HandleMonsterDied);

		Spawner->OnEncounterCompletedNative.AddUObject(
			this,
			&UAscSpawnerEncounterConditionComponent::HandleEncounterCompleted);
	}

	ResolvedTargetKillCount = FMath::Max(1, ResolvedTargetKillCount);

	if (bHasUnboundedKillTarget)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SpawnerEncounterCondition] %s: Infinite/Legacy Spawner가 포함되어 총 처치 목표를 정확히 계산할 수 없습니다. Count UI에는 유한 Spawner 목표만 표시됩니다."),
			*GetName());
	}
}

void UAscSpawnerEncounterConditionComponent::UnbindSpawners()
{
	for (AAISpawner* Spawner : BoundSpawners)
	{
		if (Spawner)
		{
			Spawner->OnMonsterDiedNative.RemoveAll(this);
			Spawner->OnEncounterCompletedNative.RemoveAll(this);
		}
	}

	BoundSpawners.Reset();
}

void UAscSpawnerEncounterConditionComponent::RefreshAlreadyCompletedSpawners()
{
	for (AAISpawner* Spawner : BoundSpawners)
	{
		if (Spawner && Spawner->IsEncounterCompleted())
		{
			CompletedSpawners.AddUnique(Spawner);
		}
	}
}

void UAscSpawnerEncounterConditionComponent::EvaluateCompletion()
{
	if (!IsConditionActive() || BoundSpawners.IsEmpty())
	{
		return;
	}

	const bool bSatisfied = bRequireAllSpawners
		? CompletedSpawners.Num() >= BoundSpawners.Num()
		: CompletedSpawners.Num() > 0;

	if (bSatisfied)
	{
		Satisfy();
	}
}
