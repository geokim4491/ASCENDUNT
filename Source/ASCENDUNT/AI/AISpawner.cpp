// Fill out your copyright notice in the Description page of Project Settings.

#include "AISpawner.h"
#include "AscAISpawnPoint.h"
#include "ASCENDUNT/Character/AI/AIPawnData.h"
#include "ASCENDUNT/System/AIPoolSubsystem.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAISpawner::AAISpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAISpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (bAutoActivateOnBeginPlay)
	{
		ActivateSpawner();
	}
}

bool AAISpawner::HasValidEntries(const TArray<FAscAISpawnEntry>& Entries) const
{
	for (const FAscAISpawnEntry& Entry : Entries)
	{
		if (Entry.PawnData && !Entry.PawnData->BPClass.IsNull() && Entry.Count > 0)
		{
			return true;
		}
	}

	return false;
}

bool AAISpawner::IsUsingWaveConfig() const
{
	for (const FAscAISpawnWave& Wave : Waves)
	{
		if (HasValidEntries(Wave.SpawnEntries))
		{
			return true;
		}
	}

	return false;
}

const FAscAISpawnWave* AAISpawner::GetCurrentWave() const
{
	if (!IsUsingWaveConfig() || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return nullptr;
	}

	return &Waves[CurrentWaveIndex];
}

const TArray<FAscAISpawnEntry>& AAISpawner::GetActiveSpawnEntries() const
{
	if (const FAscAISpawnWave* Wave = GetCurrentWave())
	{
		return Wave->SpawnEntries;
	}

	return SpawnEntries;
}

void AAISpawner::InitializeEntryRuntimeStates()
{
	const TArray<FAscAISpawnEntry>& ActiveEntries = GetActiveSpawnEntries();
	EntryRuntimeStates.SetNum(ActiveEntries.Num());

	for (FEntryRuntimeState& State : EntryRuntimeStates)
	{
		State.AliveCount = 0;
		State.TotalSpawnedCount = 0;
		State.NextSpawnPointIndex = 0;
	}

	SpawnedActorEntryIndices.Reset();
	NextEntrySelectionIndex = 0;
	bEntryRuntimeInitialized = true;
}

void AAISpawner::ActivateSpawner()
{
	if (!HasAuthority() || bIsSpawnerActive || bEncounterCompleted)
	{
		return;
	}

	PruneInvalidLiveMonsters();

	if (!IsUsingWaveConfig() && !HasValidSpawnEntries() && !IsUsingLegacySpawnConfig())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AISpawner] %s: Waves/SpawnEntries가 비어 있고 Legacy SpawnData도 유효하지 않아 활성화하지 않습니다."),
			*GetName());
		return;
	}

	bIsSpawnerActive = true;

	if (IsUsingLegacySpawnConfig())
	{
		while (CanSpawnGlobally())
		{
			if (!SpawnOneLegacy())
			{
				break;
			}
		}
		return;
	}

	if (IsUsingWaveConfig())
	{
		if (CurrentWaveIndex == INDEX_NONE)
		{
			// 비어 있는 Wave가 중간에 있어도 첫 유효 Wave부터 시작한다.
			for (int32 WaveIndex = 0; WaveIndex < Waves.Num(); ++WaveIndex)
			{
				if (HasValidEntries(Waves[WaveIndex].SpawnEntries))
				{
					CurrentWaveIndex = WaveIndex;
					break;
				}
			}
		}

		if (!Waves.IsValidIndex(CurrentWaveIndex))
		{
			CompleteEncounter();
			return;
		}

		// Stop 후 재활성화: 시작된 Wave는 Population을 이어서 채우고,
		// 아직 시작 전이면 해당 Wave의 StartDelay를 다시 적용한다.
		if (bCurrentWaveStarted)
		{
			if (!TryCompleteCurrentProgress())
			{
				ScheduleSpawn(0.0f);
			}
		}
		else
		{
			ScheduleWaveStart(Waves[CurrentWaveIndex].StartDelay);
		}
		return;
	}

	// Waves가 없는 기존 단일 Encounter 모드.
	if (!bEntryRuntimeInitialized || EntryRuntimeStates.Num() != SpawnEntries.Num())
	{
		InitializeEntryRuntimeStates();
	}

	if (!TryCompleteCurrentProgress())
	{
		ScheduleSpawn(0.0f);
	}
}

void AAISpawner::StopSpawner()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsSpawnerActive = false;
	bSpawnScheduled = false;
	bWaveStartScheduled = false;

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(SpawnTimerHandle);
		TimerManager.ClearTimer(WaveStartTimerHandle);
	}
}

bool AAISpawner::HasValidSpawnEntries() const
{
	return HasValidEntries(SpawnEntries);
}

bool AAISpawner::IsUsingLegacySpawnConfig() const
{
	return !IsUsingWaveConfig()
		&& !HasValidSpawnEntries()
		&& SpawnData
		&& !SpawnData->BPClass.IsNull()
		&& SpawnCount > 0;
}

int32 AAISpawner::GetConfiguredEntryTargetCount() const
{
	int32 TargetCount = 0;

	for (const FAscAISpawnEntry& Entry : GetActiveSpawnEntries())
	{
		if (Entry.PawnData && !Entry.PawnData->BPClass.IsNull() && Entry.Count > 0)
		{
			TargetCount += Entry.Count;
		}
	}

	return TargetCount;
}

int32 AAISpawner::GetEncounterTargetKillCount() const
{
	// Legacy 모드는 SpawnCount를 계속 유지하는 구조라 자연 종료를 위한 총 처치 수가 없다.
	if (IsUsingLegacySpawnConfig())
	{
		return INDEX_NONE;
	}

	auto ResolvePopulationTarget = [](
		const TArray<FAscAISpawnEntry>& Entries,
		bool bInfinite,
		int32 ConfiguredMaxTotalSpawnCount) -> int32
	{
		int32 InitialTargetCount = 0;
		bool bHasRepeatableEntry = false;

		for (const FAscAISpawnEntry& Entry : Entries)
		{
			if (!Entry.PawnData || Entry.PawnData->BPClass.IsNull() || Entry.Count <= 0)
			{
				continue;
			}

			InitialTargetCount += Entry.Count;
			bHasRepeatableEntry |= !Entry.bSpawnOnlyOnce;
		}

		if (InitialTargetCount <= 0)
		{
			return 0;
		}

		// Infinite여도 모든 Entry가 SpawnOnlyOnce라면 실제 생성 가능한 수는 유한하다.
		if (bInfinite)
		{
			return bHasRepeatableEntry ? INDEX_NONE : InitialTargetCount;
		}

		const int32 ResolvedMaxTotalSpawnCount = ConfiguredMaxTotalSpawnCount > 0
			? ConfiguredMaxTotalSpawnCount
			: InitialTargetCount;

		// 반복 가능한 Entry가 하나라도 있으면 전역 MaxTotal까지 생성 가능하다.
		// 전부 SpawnOnlyOnce라면 Entry Count 합계를 넘어서 생성할 수 없다.
		return bHasRepeatableEntry
			? ResolvedMaxTotalSpawnCount
			: FMath::Min(InitialTargetCount, ResolvedMaxTotalSpawnCount);
	};

	if (IsUsingWaveConfig())
	{
		int64 EncounterTargetCount = 0;

		for (const FAscAISpawnWave& Wave : Waves)
		{
			if (!HasValidEntries(Wave.SpawnEntries))
			{
				continue;
			}

			const int32 WaveTargetCount = ResolvePopulationTarget(
				Wave.SpawnEntries,
				Wave.bInfiniteSpawn,
				Wave.MaxTotalSpawnCount);

			if (WaveTargetCount == INDEX_NONE)
			{
				return INDEX_NONE;
			}

			EncounterTargetCount += WaveTargetCount;
			if (EncounterTargetCount >= MAX_int32)
			{
				return MAX_int32;
			}
		}

		return static_cast<int32>(EncounterTargetCount);
	}

	if (HasValidSpawnEntries())
	{
		return ResolvePopulationTarget(SpawnEntries, bInfiniteSpawn, MaxTotalSpawnCount);
	}

	return INDEX_NONE;
}

int32 AAISpawner::GetResolvedMaxTotalSpawnCount() const
{
	if (IsUsingLegacySpawnConfig())
	{
		return MAX_int32;
	}

	if (const FAscAISpawnWave* Wave = GetCurrentWave())
	{
		if (Wave->bInfiniteSpawn)
		{
			return MAX_int32;
		}

		return Wave->MaxTotalSpawnCount > 0
			? Wave->MaxTotalSpawnCount
			: GetConfiguredEntryTargetCount();
	}

	if (bInfiniteSpawn)
	{
		return MAX_int32;
	}

	return MaxTotalSpawnCount > 0
		? MaxTotalSpawnCount
		: GetConfiguredEntryTargetCount();
}

int32 AAISpawner::GetResolvedMaxAliveCount() const
{
	if (IsUsingLegacySpawnConfig())
	{
		return FMath::Max(1, SpawnCount);
	}

	if (const FAscAISpawnWave* Wave = GetCurrentWave())
	{
		return Wave->MaxAliveCount > 0
			? Wave->MaxAliveCount
			: FMath::Max(0, GetConfiguredEntryTargetCount());
	}

	return MaxAliveCount > 0
		? MaxAliveCount
		: FMath::Max(0, GetConfiguredEntryTargetCount());
}


float AAISpawner::GetResolvedSpawnInterval() const
{
	if (const FAscAISpawnWave* Wave = GetCurrentWave())
	{
		return FMath::Max(0.0f, Wave->SpawnInterval);
	}

	return FMath::Max(0.0f, SpawnInterval);
}

float AAISpawner::GetResolvedRespawnDelay() const
{
	if (const FAscAISpawnWave* Wave = GetCurrentWave())
	{
		return FMath::Max(0.0f, Wave->RespawnDelay);
	}

	return FMath::Max(0.0f, RespawnDelay);
}

int32 AAISpawner::GetCurrentPopulationSpawnedCount() const
{
	return GetCurrentWave() ? CurrentWaveSpawnedCount : TotalSpawnedCount;
}

bool AAISpawner::CanSpawnGlobally() const
{
	if (!bIsSpawnerActive || bEncounterCompleted)
	{
		return false;
	}

	if (!IsUsingLegacySpawnConfig() && !HasValidEntries(GetActiveSpawnEntries()))
	{
		return false;
	}

	if (!IsUsingLegacySpawnConfig()
		&& GetCurrentPopulationSpawnedCount() >= GetResolvedMaxTotalSpawnCount())
	{
		return false;
	}

	const int32 ResolvedMaxAlive = GetResolvedMaxAliveCount();
	if (ResolvedMaxAlive > 0 && LiveMonsters.Num() >= ResolvedMaxAlive)
	{
		return false;
	}

	return true;
}

bool AAISpawner::CanSpawnEntry(int32 EntryIndex) const
{
	const TArray<FAscAISpawnEntry>& ActiveEntries = GetActiveSpawnEntries();
	if (!ActiveEntries.IsValidIndex(EntryIndex) || !EntryRuntimeStates.IsValidIndex(EntryIndex))
	{
		return false;
	}

	const FAscAISpawnEntry& Entry = ActiveEntries[EntryIndex];
	const FEntryRuntimeState& State = EntryRuntimeStates[EntryIndex];

	if (!Entry.PawnData || Entry.PawnData->BPClass.IsNull() || Entry.Count <= 0)
	{
		return false;
	}

	if (Entry.bSpawnOnlyOnce)
	{
		return State.TotalSpawnedCount < Entry.Count;
	}

	return State.AliveCount < Entry.Count;
}

int32 AAISpawner::FindNextEntryToSpawn() const
{
	const TArray<FAscAISpawnEntry>& ActiveEntries = GetActiveSpawnEntries();
	if (!CanSpawnGlobally() || ActiveEntries.Num() == 0)
	{
		return INDEX_NONE;
	}

	const int32 StartIndex = NextEntrySelectionIndex % ActiveEntries.Num();
	for (int32 Offset = 0; Offset < ActiveEntries.Num(); ++Offset)
	{
		const int32 EntryIndex = (StartIndex + Offset) % ActiveEntries.Num();
		if (CanSpawnEntry(EntryIndex))
		{
			return EntryIndex;
		}
	}

	return INDEX_NONE;
}

AAscAISpawnPoint* AAISpawner::SelectSpawnPoint(
	const TArray<TObjectPtr<AAscAISpawnPoint>>& CandidatePoints,
	int32& InOutRoundRobinIndex) const
{
	TArray<AAscAISpawnPoint*> ValidPoints;
	ValidPoints.Reserve(CandidatePoints.Num());

	for (AAscAISpawnPoint* SpawnPoint : CandidatePoints)
	{
		if (IsValid(SpawnPoint))
		{
			ValidPoints.Add(SpawnPoint);
		}
	}

	if (ValidPoints.Num() == 0)
	{
		return nullptr;
	}

	switch (SpawnPointSelectionPolicy)
	{
	case EAscSpawnPointSelectionPolicy::RoundRobin:
		InOutRoundRobinIndex %= ValidPoints.Num();
		{
			AAscAISpawnPoint* SelectedPoint = ValidPoints[InOutRoundRobinIndex];
			InOutRoundRobinIndex = (InOutRoundRobinIndex + 1) % ValidPoints.Num();
			return SelectedPoint;
		}

	case EAscSpawnPointSelectionPolicy::Random:
	default:
		return ValidPoints[FMath::RandRange(0, ValidPoints.Num() - 1)];
	}
}

FTransform AAISpawner::MakeSpawnTransform(int32 EntryIndex)
{
	const TArray<FAscAISpawnEntry>& ActiveEntries = GetActiveSpawnEntries();

	if (ActiveEntries.IsValidIndex(EntryIndex) && EntryRuntimeStates.IsValidIndex(EntryIndex))
	{
		FEntryRuntimeState& RuntimeState = EntryRuntimeStates[EntryIndex];
		const FAscAISpawnEntry& Entry = ActiveEntries[EntryIndex];

		if (AAscAISpawnPoint* EntryPoint = SelectSpawnPoint(Entry.SpawnPoints, RuntimeState.NextSpawnPointIndex))
		{
			return EntryPoint->MakeSpawnTransform();
		}

		if (AAscAISpawnPoint* DefaultPoint = SelectSpawnPoint(SpawnPoints, RuntimeState.NextSpawnPointIndex))
		{
			return DefaultPoint->MakeSpawnTransform();
		}
	}
	else
	{
		if (AAscAISpawnPoint* DefaultPoint = SelectSpawnPoint(SpawnPoints, LegacyNextSpawnPointIndex))
		{
			return DefaultPoint->MakeSpawnTransform();
		}
	}

	FTransform SpawnTransform = GetActorTransform();
	if (SpawnRadius > 0.0f)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = SpawnRadius * FMath::Sqrt(FMath::FRand());
		const FVector Offset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
		SpawnTransform.AddToTranslation(Offset);
	}

	return SpawnTransform;
}

bool AAISpawner::SpawnOneFromEntry(int32 EntryIndex)
{
	const TArray<FAscAISpawnEntry>& ActiveEntries = GetActiveSpawnEntries();
	if (!CanSpawnGlobally()
		|| !CanSpawnEntry(EntryIndex)
		|| !ActiveEntries.IsValidIndex(EntryIndex)
		|| !EntryRuntimeStates.IsValidIndex(EntryIndex))
	{
		return false;
	}

	const FAscAISpawnEntry& Entry = ActiveEntries[EntryIndex];
	FEntryRuntimeState& RuntimeState = EntryRuntimeStates[EntryIndex];
	UAIPawnData* DataToSpawn = Entry.PawnData;

	UWorld* World = GetWorld();
	if (!World || !DataToSpawn)
	{
		return false;
	}

	UAIPoolSubsystem* Pool = World->GetSubsystem<UAIPoolSubsystem>();
	if (!Pool)
	{
		return false;
	}

	UClass* ClassToSpawn = DataToSpawn->BPClass.LoadSynchronous();
	if (!ClassToSpawn)
	{
		return false;
	}

	ABaseAICharacter* SpawnedAI = Pool->AcquireAI(ClassToSpawn, MakeSpawnTransform(EntryIndex));
	if (!SpawnedAI)
	{
		return false;
	}

	SpawnedAI->SetPawnData(DataToSpawn);

	if (!SpawnedAI->GetController())
	{
		SpawnedAI->SpawnDefaultController();
	}

	if (UAscenduntHealthComponent* HealthComp = UAscenduntHealthComponent::FindHealthComponent(SpawnedAI))
	{
		HealthComp->OnDeathFinished.AddUniqueDynamic(this, &AAISpawner::OnMonsterDied);
	}

	LiveMonsters.AddUnique(SpawnedAI);
	SpawnedActorEntryIndices.Add(SpawnedAI, EntryIndex);
	++RuntimeState.AliveCount;
	++RuntimeState.TotalSpawnedCount;
	++TotalSpawnedCount;

	if (GetCurrentWave())
	{
		++CurrentWaveSpawnedCount;
	}

	NextEntrySelectionIndex = (EntryIndex + 1) % ActiveEntries.Num();
	return true;
}

bool AAISpawner::SpawnOneLegacy()
{
	if (!CanSpawnGlobally() || !SpawnData || SpawnData->BPClass.IsNull())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UAIPoolSubsystem* Pool = World->GetSubsystem<UAIPoolSubsystem>();
	if (!Pool)
	{
		return false;
	}

	UClass* ClassToSpawn = SpawnData->BPClass.LoadSynchronous();
	if (!ClassToSpawn)
	{
		return false;
	}

	ABaseAICharacter* SpawnedAI = Pool->AcquireAI(ClassToSpawn, MakeSpawnTransform(INDEX_NONE));
	if (!SpawnedAI)
	{
		return false;
	}

	SpawnedAI->SetPawnData(SpawnData);

	if (!SpawnedAI->GetController())
	{
		SpawnedAI->SpawnDefaultController();
	}

	if (UAscenduntHealthComponent* HealthComp = UAscenduntHealthComponent::FindHealthComponent(SpawnedAI))
	{
		HealthComp->OnDeathFinished.AddUniqueDynamic(this, &AAISpawner::OnMonsterDied);
	}

	LiveMonsters.AddUnique(SpawnedAI);
	++TotalSpawnedCount;
	return true;
}

void AAISpawner::ScheduleSpawn(float DelaySeconds)
{
	if (!bIsSpawnerActive || bEncounterCompleted)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || bSpawnScheduled)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	bSpawnScheduled = true;

	if (DelaySeconds <= 0.0f)
	{
		SpawnTimerHandle = TimerManager.SetTimerForNextTick(this, &AAISpawner::HandleSpawnTimer);
		return;
	}

	TimerManager.SetTimer(SpawnTimerHandle, this, &AAISpawner::HandleSpawnTimer, DelaySeconds, false);
}

void AAISpawner::ScheduleWaveStart(float DelaySeconds)
{
	if (!bIsSpawnerActive || bEncounterCompleted || !IsUsingWaveConfig() || bWaveStartScheduled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	bWaveStartScheduled = true;

	if (DelaySeconds <= 0.0f)
	{
		WaveStartTimerHandle = TimerManager.SetTimerForNextTick(this, &AAISpawner::HandleWaveStartTimer);
		return;
	}

	TimerManager.SetTimer(WaveStartTimerHandle, this, &AAISpawner::HandleWaveStartTimer, DelaySeconds, false);
}

void AAISpawner::HandleWaveStartTimer()
{
	bWaveStartScheduled = false;

	if (!HasAuthority() || !bIsSpawnerActive || bEncounterCompleted || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	// 비어 있는 Wave는 건너뛴다.
	if (!HasValidEntries(Waves[CurrentWaveIndex].SpawnEntries))
	{
		CompleteCurrentWave();
		return;
	}

	bCurrentWaveStarted = true;
	CurrentWaveSpawnedCount = 0;
	InitializeEntryRuntimeStates();

	OnWaveStarted.Broadcast(this, CurrentWaveIndex);
	ScheduleSpawn(0.0f);
}

void AAISpawner::HandleSpawnTimer()
{
	bSpawnScheduled = false;

	if (!HasAuthority() || !bIsSpawnerActive || bEncounterCompleted)
	{
		return;
	}

	PruneInvalidLiveMonsters();

	if (IsUsingWaveConfig() && !bCurrentWaveStarted)
	{
		return;
	}

	if (!CanSpawnGlobally())
	{
		TryCompleteCurrentProgress();
		return;
	}

	if (IsUsingLegacySpawnConfig())
	{
		while (CanSpawnGlobally())
		{
			if (!SpawnOneLegacy())
			{
				break;
			}
		}
		return;
	}

	const int32 EntryIndex = FindNextEntryToSpawn();
	if (EntryIndex == INDEX_NONE)
	{
		TryCompleteCurrentProgress();
		return;
	}

	if (!SpawnOneFromEntry(EntryIndex))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AISpawner] %s: Entry %d AI Spawn에 실패하여 현재 Spawn Loop를 중단합니다."),
			*GetName(), EntryIndex);
		return;
	}

	if (FindNextEntryToSpawn() != INDEX_NONE)
	{
		const float ResolvedSpawnInterval = GetResolvedSpawnInterval();
		ScheduleSpawn(ResolvedSpawnInterval <= 0.0f ? 0.0f : ResolvedSpawnInterval);
	}
	else
	{
		TryCompleteCurrentProgress();
	}
}

void AAISpawner::OnMonsterDied(AActor* DeadActor)
{
	if (!DeadActor || LiveMonsters.Remove(DeadActor) == 0)
	{
		return;
	}

	if (UAscenduntHealthComponent* HealthComp = UAscenduntHealthComponent::FindHealthComponent(DeadActor))
	{
		HealthComp->OnDeathFinished.RemoveDynamic(this, &AAISpawner::OnMonsterDied);
	}

	if (const int32* EntryIndexPtr = SpawnedActorEntryIndices.Find(DeadActor))
	{
		const int32 EntryIndex = *EntryIndexPtr;
		if (EntryRuntimeStates.IsValidIndex(EntryIndex))
		{
			EntryRuntimeStates[EntryIndex].AliveCount = FMath::Max(0, EntryRuntimeStates[EntryIndex].AliveCount - 1);
		}

		SpawnedActorEntryIndices.Remove(DeadActor);
	}

	OnMonsterDiedNative.Broadcast(DeadActor);

	// Stop 상태에서는 사망 통지만 하고 Wave/보충 진행은 멈춘 상태로 유지한다.
	if (!bIsSpawnerActive || bEncounterCompleted)
	{
		return;
	}

	if (IsUsingLegacySpawnConfig())
	{
		if (CanSpawnGlobally())
		{
			ScheduleSpawn(GetResolvedRespawnDelay());
		}
		return;
	}

	if (TryCompleteCurrentProgress())
	{
		return;
	}

	if (FindNextEntryToSpawn() != INDEX_NONE)
	{
		ScheduleSpawn(GetResolvedRespawnDelay());
	}
}

void AAISpawner::PruneInvalidLiveMonsters()
{
	for (int32 Index = LiveMonsters.Num() - 1; Index >= 0; --Index)
	{
		AActor* Monster = LiveMonsters[Index].Get();
		if (IsValid(Monster))
		{
			continue;
		}

		if (const int32* EntryIndexPtr = SpawnedActorEntryIndices.Find(Monster))
		{
			const int32 EntryIndex = *EntryIndexPtr;
			if (EntryRuntimeStates.IsValidIndex(EntryIndex))
			{
				EntryRuntimeStates[EntryIndex].AliveCount = FMath::Max(0, EntryRuntimeStates[EntryIndex].AliveCount - 1);
			}

			SpawnedActorEntryIndices.Remove(Monster);
		}

		LiveMonsters.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}
}

bool AAISpawner::TryCompleteCurrentProgress()
{
	if (!bIsSpawnerActive || bEncounterCompleted || IsUsingLegacySpawnConfig())
	{
		return false;
	}

	if (IsUsingWaveConfig() && !bCurrentWaveStarted)
	{
		return false;
	}

	if (LiveMonsters.Num() > 0)
	{
		return false;
	}

	// Alive가 0일 때도 더 생성할 수 있는 Entry가 남아 있다면 아직 완료가 아니다.
	if (FindNextEntryToSpawn() != INDEX_NONE)
	{
		return false;
	}

	if (IsUsingWaveConfig())
	{
		CompleteCurrentWave();
	}
	else
	{
		CompleteEncounter();
	}

	return true;
}

void AAISpawner::CompleteCurrentWave()
{
	if (!IsUsingWaveConfig() || !Waves.IsValidIndex(CurrentWaveIndex) || bEncounterCompleted)
	{
		return;
	}

	const int32 CompletedWaveIndex = CurrentWaveIndex;
	bCurrentWaveStarted = false;
	bEntryRuntimeInitialized = false;
	EntryRuntimeStates.Reset();
	SpawnedActorEntryIndices.Reset();
	CurrentWaveSpawnedCount = 0;

	OnWaveCompleted.Broadcast(this, CompletedWaveIndex);

	int32 NextWaveIndex = CompletedWaveIndex + 1;
	while (Waves.IsValidIndex(NextWaveIndex) && !HasValidEntries(Waves[NextWaveIndex].SpawnEntries))
	{
		// 빈 Wave도 완료 이벤트는 발생시켜 인덱스 기반 BP 연출이 끊기지 않게 한다.
		OnWaveStarted.Broadcast(this, NextWaveIndex);
		OnWaveCompleted.Broadcast(this, NextWaveIndex);
		++NextWaveIndex;
	}

	if (!Waves.IsValidIndex(NextWaveIndex))
	{
		CompleteEncounter();
		return;
	}

	CurrentWaveIndex = NextWaveIndex;
	ScheduleWaveStart(Waves[CurrentWaveIndex].StartDelay);
}

void AAISpawner::CompleteEncounter()
{
	if (bEncounterCompleted)
	{
		return;
	}

	bEncounterCompleted = true;
	bIsSpawnerActive = false;
	bSpawnScheduled = false;
	bWaveStartScheduled = false;

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(SpawnTimerHandle);
		TimerManager.ClearTimer(WaveStartTimerHandle);
	}

	OnEncounterCompletedNative.Broadcast(this);
	OnEncounterCompleted.Broadcast(this);
}
