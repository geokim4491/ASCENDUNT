#include "ASCENDUNT/Mission/Spawner/AscSpawnerEncounterControllerComponent.h"

#include "ASCENDUNT/AI/AISpawner.h"
#include "ASCENDUNT/Mission/Objective/AscObjectiveBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

UAscSpawnerEncounterControllerComponent::UAscSpawnerEncounterControllerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UAscSpawnerEncounterControllerComponent::BeginPlay()
{
	Super::BeginPlay();

	AAscObjectiveBase* Objective = GetOwningObjective();
	if (!Objective)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SpawnerEncounterController] %s must be attached to AAscObjectiveBase."),
			*GetName());
		return;
	}

	// Spawner 제어는 서버 권위에서만 수행한다.
	if (!Objective->HasAuthority())
	{
		return;
	}

	Objective->OnObjectiveStateChanged.RemoveDynamic(
		this,
		&UAscSpawnerEncounterControllerComponent::HandleObjectiveStateChanged);
	Objective->OnObjectiveStateChanged.AddDynamic(
		this,
		&UAscSpawnerEncounterControllerComponent::HandleObjectiveStateChanged);

	RefreshManagedSpawners();

	// 런타임에 Active Objective에 Component가 붙는 경우도 안전하게 처리한다.
	if (Objective->IsActive() && bActivateSpawnersOnObjectiveStart)
	{
		ActivateManagedSpawners();
	}
}

void UAscSpawnerEncounterControllerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AAscObjectiveBase* Objective = GetOwningObjective())
	{
		if (Objective->HasAuthority())
		{
			Objective->OnObjectiveStateChanged.RemoveDynamic(
				this,
				&UAscSpawnerEncounterControllerComponent::HandleObjectiveStateChanged);

			if (bStopSpawnersOnObjectiveEnd)
			{
				StopManagedSpawners();
			}
		}
	}

	ResolvedSpawners.Reset();
	Super::EndPlay(EndPlayReason);
}

void UAscSpawnerEncounterControllerComponent::RefreshManagedSpawners()
{
	AAscObjectiveBase* Objective = GetOwningObjective();
	if (!Objective || !Objective->HasAuthority())
	{
		return;
	}

	ResolvedSpawners.Reset();

	for (AAISpawner* Spawner : ManagedSpawners)
	{
		if (IsValid(Spawner))
		{
			ResolvedSpawners.AddUnique(Spawner);
		}
	}

	if (ResolvedSpawners.IsEmpty() && bManageAllSpawnersIfNoneAssigned)
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AAISpawner> It(World); It; ++It)
			{
				ResolvedSpawners.AddUnique(*It);
			}
		}
	}
}

void UAscSpawnerEncounterControllerComponent::ActivateManagedSpawners()
{
	AAscObjectiveBase* Objective = GetOwningObjective();
	if (!Objective || !Objective->HasAuthority())
	{
		return;
	}

	// 레벨 배치 참조 변경이나 늦게 생성된 Spawner를 반영한다.
	RefreshManagedSpawners();

	for (AAISpawner* Spawner : ResolvedSpawners)
	{
		if (IsValid(Spawner) && !Spawner->IsEncounterCompleted())
		{
			Spawner->ActivateSpawner();
		}
	}
}

void UAscSpawnerEncounterControllerComponent::StopManagedSpawners()
{
	AAscObjectiveBase* Objective = GetOwningObjective();
	if (!Objective || !Objective->HasAuthority())
	{
		return;
	}

	for (AAISpawner* Spawner : ResolvedSpawners)
	{
		if (IsValid(Spawner))
		{
			Spawner->StopSpawner();
		}
	}
}

void UAscSpawnerEncounterControllerComponent::HandleObjectiveStateChanged(
	AAscObjectiveBase* Objective,
	EAscObjectiveState OldState,
	EAscObjectiveState NewState)
{
	if (!Objective || Objective != GetOwningObjective() || !Objective->HasAuthority())
	{
		return;
	}

	if (NewState == EAscObjectiveState::Active)
	{
		if (bActivateSpawnersOnObjectiveStart)
		{
			ActivateManagedSpawners();
		}
		return;
	}

	// Complete / Failed / Deactivate / Reset 모두 Active -> Non-Active 전이로 통일한다.
	if (OldState == EAscObjectiveState::Active && bStopSpawnersOnObjectiveEnd)
	{
		StopManagedSpawners();
	}
}

AAscObjectiveBase* UAscSpawnerEncounterControllerComponent::GetOwningObjective() const
{
	return Cast<AAscObjectiveBase>(GetOwner());
}
