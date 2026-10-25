// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AISpawner.generated.h"

class AAscAISpawnPoint;
class AAISpawner;
class UAIPawnData;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnAscMonsterDiedNative,
	AActor* /* DeadActor */);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnAscEncounterCompletedNative,
	AAISpawner* /* Spawner */);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FAscAISpawnerWaveEvent,
	AAISpawner*, Spawner,
	int32, WaveIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAscAISpawnerEncounterEvent,
	AAISpawner*, Spawner);

/** 한 AI 타입의 목표 Population과 Spawn 위치를 정의한다. */
USTRUCT(BlueprintType)
struct FAscAISpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner")
	TObjectPtr<UAIPawnData> PawnData = nullptr;

	/** 이 AI 타입을 동시에 몇 마리까지 유지할지 정의한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** true면 최초 Count만 생성하고, 해당 AI가 죽어도 다시 보충하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner")
	bool bSpawnOnlyOnce = false;

	/**
	 * 이 Entry 전용 Spawn Point.
	 * 비어 있으면 AAISpawner의 기본 SpawnPoints를 사용한다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawner")
	TArray<TObjectPtr<AAscAISpawnPoint>> SpawnPoints;
};

/** 하나의 PvE Wave를 구성하는 Spawn/Population 설정. */
USTRUCT(BlueprintType)
struct FAscAISpawnWave
{
	GENERATED_BODY()

	/** 이 Wave에서 유지/생성할 AI 구성. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	TArray<FAscAISpawnEntry> SpawnEntries;

	/** 이전 Wave 완료(첫 Wave는 Spawner Activate) 후 이 Wave가 시작되기까지의 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float StartDelay = 0.0f;

	/** 이 Wave에서 AI 한 마리 생성 후 다음 AI까지의 간격. 0이면 다음 Tick에 생성한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float SpawnInterval = 0.2f;

	/** 이 Wave에서 사망으로 생긴 결원을 보충하기 전 대기 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float RespawnDelay = 0.0f;

	/** 이 Wave에서 동시에 살아 있을 수 있는 전체 AI 수. 0이면 유효 Entry Count 합계를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave|Population", meta = (ClampMin = "0"))
	int32 MaxAliveCount = 0;

	/** true면 이 Wave의 MaxTotalSpawnCount를 무시한다. 일반 Entry가 있으면 외부에서 Stop하기 전까지 끝나지 않을 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave|Population")
	bool bInfiniteSpawn = false;

	/** 이 Wave에서 누적 생성 가능한 최대 AI 수. 0이면 최초 목표 Population 합계만큼만 생성한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave|Population", meta = (ClampMin = "0", EditCondition = "!bInfiniteSpawn", EditConditionHides))
	int32 MaxTotalSpawnCount = 0;
};

UENUM(BlueprintType)
enum class EAscSpawnPointSelectionPolicy : uint8
{
	Random UMETA(DisplayName = "Random"),
	RoundRobin UMETA(DisplayName = "Round Robin")
};

/**
 * PvE Encounter용 AI Spawner.
 *
 * - Waves가 있으면 Wave 0부터 순서대로 진행한다.
 * - Wave는 자신의 SpawnEntries/Population 한도를 독립적으로 가진다.
 * - Wave에서 더 생성할 AI가 없고 해당 Wave AI가 모두 사망하면 다음 Wave로 진행한다.
 * - 마지막 Wave가 끝나면 Encounter Completed 이벤트를 발생시킨다.
 * - Waves가 비어 있으면 기존 SpawnEntries/Population 설정을 단일 Encounter로 사용한다.
 * - Entry::Count는 해당 AI 타입의 목표 동시 생존 수다.
 * - SpawnOnlyOnce Entry는 최초 Count만 생성하고 이후 보충하지 않는다.
 * - Entry SpawnPoints -> Spawner 기본 SpawnPoints -> Spawner Transform 순으로 Spawn 위치를 선택한다.
 * - AI 인스턴스 생성/재사용은 UAIPoolSubsystem에 위임한다.
 */
UCLASS()
class ASCENDUNT_API AAISpawner : public AActor
{
	GENERATED_BODY()

public:
	AAISpawner();

	// KillCountCondition 등 기존 서버 로직이 사용한다.
	FOnAscMonsterDiedNative OnMonsterDiedNative;

	// Mission Condition 등 C++ 서버 로직에서 Encounter 완료를 구독할 때 사용한다.
	FOnAscEncounterCompletedNative OnEncounterCompletedNative;

	/** Spawner/Encounter를 활성화한다. 이미 활성 상태면 아무것도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Spawner")
	void ActivateSpawner();

	/** 추가 Spawn/Wave 진행만 중단한다. 이미 살아 있는 AI는 그대로 유지한다. */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Spawner")
	void StopSpawner();

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Spawner")
	bool IsSpawnerActive() const { return bIsSpawnerActive; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Spawner")
	bool IsEncounterCompleted() const { return bEncounterCompleted; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Spawner")
	int32 GetLiveMonsterCount() const { return LiveMonsters.Num(); }

	/** Encounter 시작 이후 누적 생성 수. Wave가 바뀌어도 누적된다. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Spawner")
	int32 GetTotalSpawnedCount() const { return TotalSpawnedCount; }

	/**
	 * Encounter가 자연스럽게 완료될 때까지 필요한 총 처치 수를 반환한다.
	 * 유한 Wave/Encounter는 실제 Spawn 가능한 총량을 계산하며,
	 * 반복 생성 가능한 Infinite 설정 또는 Legacy 유지형 Spawner는 INDEX_NONE을 반환한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Spawner")
	int32 GetEncounterTargetKillCount() const;

	/** Wave 설정을 사용하지 않으면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Spawner|Wave")
	int32 GetCurrentWaveIndex() const { return IsUsingWaveConfig() ? CurrentWaveIndex : INDEX_NONE; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|Spawner|Wave")
	int32 GetWaveCount() const { return Waves.Num(); }

	/** 서버/Blueprint에서 Wave 시작 시점을 받을 수 있는 이벤트. */
	UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Spawner|Wave")
	FAscAISpawnerWaveEvent OnWaveStarted;

	/** 현재 Wave의 Spawn 가능 수가 모두 소진되고 해당 Wave AI가 전멸했을 때 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Spawner|Wave")
	FAscAISpawnerWaveEvent OnWaveCompleted;

	/** 단일 Encounter 또는 마지막 Wave가 완료되었을 때 한 번 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "Ascendunt|Spawner")
	FAscAISpawnerEncounterEvent OnEncounterCompleted;

protected:
	virtual void BeginPlay() override;

	/** 현재 활성 Entry 집합(Wave 또는 단일 SpawnEntries)의 런타임 상태를 초기화한다. */
	void InitializeEntryRuntimeStates();

	/** 다음 보충 대상 Entry를 찾는다. 없으면 INDEX_NONE. */
	int32 FindNextEntryToSpawn() const;

	/** 특정 Entry에서 AI 한 마리를 생성한다. 성공하면 true. */
	bool SpawnOneFromEntry(int32 EntryIndex);

	/** Legacy SpawnData 설정에서 AI 한 마리를 생성한다. */
	bool SpawnOneLegacy();

	/** 현재 Wave/Encounter의 Population/누적 한도상 AI를 한 마리 더 생성할 수 있는지 확인한다. */
	bool CanSpawnGlobally() const;

	/** 특정 Entry가 자신의 목표/OneShot 조건상 추가 생성 가능한지 확인한다. */
	bool CanSpawnEntry(int32 EntryIndex) const;

	/** Entry 전용 Point -> 기본 Point -> Spawner Transform 순으로 Transform을 선택한다. */
	FTransform MakeSpawnTransform(int32 EntryIndex);

	/** 주어진 SpawnPoint 배열에서 정책에 따라 하나를 선택한다. */
	AAscAISpawnPoint* SelectSpawnPoint(
		const TArray<TObjectPtr<AAscAISpawnPoint>>& CandidatePoints,
		int32& InOutRoundRobinIndex) const;

	/** Initial/Refill 공용 Spawn Timer 예약. 이미 Timer가 있으면 중복 예약하지 않는다. */
	void ScheduleSpawn(float DelaySeconds);

	/** Wave StartDelay용 Timer 예약. */
	void ScheduleWaveStart(float DelaySeconds);

	UFUNCTION()
	void HandleSpawnTimer();

	UFUNCTION()
	void HandleWaveStartTimer();

	UFUNCTION()
	void OnMonsterDied(AActor* DeadActor);

	/** 파괴 등으로 무효화된 AI 참조를 제거하고 Entry AliveCount도 맞춘다. */
	void PruneInvalidLiveMonsters();

	/** 현재 Wave/단일 Encounter가 종료 가능한지 검사하고 필요한 완료 처리를 한다. */
	bool TryCompleteCurrentProgress();

	void CompleteCurrentWave();
	void CompleteEncounter();

	bool IsUsingWaveConfig() const;
	const FAscAISpawnWave* GetCurrentWave() const;
	const TArray<FAscAISpawnEntry>& GetActiveSpawnEntries() const;
	bool HasValidEntries(const TArray<FAscAISpawnEntry>& Entries) const;

	/** Waves/SpawnEntries가 모두 비어 있으면 deprecated SpawnData/SpawnCount를 fallback으로 사용한다. */
	bool HasValidSpawnEntries() const;
	bool IsUsingLegacySpawnConfig() const;
	int32 GetConfiguredEntryTargetCount() const;

	int32 GetResolvedMaxAliveCount() const;
	int32 GetResolvedMaxTotalSpawnCount() const;
	float GetResolvedSpawnInterval() const;
	float GetResolvedRespawnDelay() const;
	int32 GetCurrentPopulationSpawnedCount() const;

private:
	struct FEntryRuntimeState
	{
		int32 AliveCount = 0;
		int32 TotalSpawnedCount = 0;
		int32 NextSpawnPointIndex = 0;
	};

public:
	/** BeginPlay에서 자동으로 Activate할지 여부. Mission에서 시작할 Spawner는 false로 두면 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Activation")
	bool bAutoActivateOnBeginPlay = true;

	/**
	 * Wave Encounter 설정. 하나라도 유효한 Wave가 있으면 아래 단일 SpawnEntries 대신 이 배열을 사용한다.
	 * Wave 0 -> 전멸/소진 -> Wave 1 순으로 자동 진행한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Wave")
	TArray<FAscAISpawnWave> Waves;

	/** Waves가 비어 있을 때 사용하는 단일 Encounter AI 구성. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Spawn")
	TArray<FAscAISpawnEntry> SpawnEntries;

	/** AI 한 마리 생성 후 다음 AI를 생성하기까지의 간격. 0이면 다음 Tick에 한 마리씩 생성한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Spawn", meta = (ClampMin = "0.0"))
	float SpawnInterval = 0.2f;

	/** AI 사망으로 빈 자리가 생겼을 때 보충 Spawn을 시작하기 전 대기 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Spawn", meta = (ClampMin = "0.0"))
	float RespawnDelay = 0.0f;

	/** Entry에 SpawnPoints가 지정되지 않았을 때 사용할 기본 Spawn Point들. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Ascendunt|Spawner|Spawn Point")
	TArray<TObjectPtr<AAscAISpawnPoint>> SpawnPoints;

	/** Entry 전용/기본 Spawn Point 모두에 공통으로 적용되는 선택 정책. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Spawn Point")
	EAscSpawnPointSelectionPolicy SpawnPointSelectionPolicy = EAscSpawnPointSelectionPolicy::Random;

	/** Waves가 비어 있을 때 사용하는 단일 Encounter 전체 동시 생존 상한. 0이면 유효 Entry Count 합계. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Population", meta = (ClampMin = "0"))
	int32 MaxAliveCount = 0;

	/** Waves가 비어 있을 때 true면 MaxTotalSpawnCount를 무시한다. SpawnOnlyOnce 제한은 유지된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Population")
	bool bInfiniteSpawn = false;

	/** Waves가 비어 있을 때 사용하는 단일 Encounter 누적 Spawn 상한. 0이면 최초 목표 Population 합계. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Population", meta = (ClampMin = "0", EditCondition = "!bInfiniteSpawn", EditConditionHides))
	int32 MaxTotalSpawnCount = 0;

	// ---------------------------------------------------------------------
	// Legacy fallback: 기존 레벨/Blueprint에 배치된 AAISpawner 데이터 보존용.
	// 새 Spawner는 SpawnEntries 또는 Waves를 사용한다.
	// ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use SpawnEntries or Waves instead."))
	TObjectPtr<UAIPawnData> SpawnData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Legacy", meta = (ClampMin = "1", DeprecatedProperty, DeprecationMessage = "Use SpawnEntries or Waves instead."))
	int32 SpawnCount = 1;

	/** Legacy SpawnData를 쓸 때만 적용되는 Spawner 자체 반경. SpawnPoint 사용 시 각 Point의 Radius를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner|Legacy", meta = (ClampMin = "0.0", DeprecatedProperty, DeprecationMessage = "Use AAscAISpawnPoint::SpawnRadius instead."))
	float SpawnRadius = 0.0f;

protected:
	FTimerHandle SpawnTimerHandle;
	FTimerHandle WaveStartTimerHandle;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> LiveMonsters;

	/** 현재 활성 Entry 집합의 인덱스와 1:1로 대응한다. */
	TArray<FEntryRuntimeState> EntryRuntimeStates;

	/** Spawn된 AI가 현재 Entry 집합의 어느 Entry에서 나왔는지 추적한다. */
	TMap<AActor*, int32> SpawnedActorEntryIndices;

	bool bIsSpawnerActive = false;
	bool bSpawnScheduled = false;
	bool bWaveStartScheduled = false;
	bool bEncounterCompleted = false;
	bool bCurrentWaveStarted = false;

	/** Encounter 전체 누적 Spawn 수. */
	int32 TotalSpawnedCount = 0;

	/** 현재 Wave에서만 누적된 Spawn 수. 단일 Encounter에서는 사용하지 않는다. */
	int32 CurrentWaveSpawnedCount = 0;

	int32 CurrentWaveIndex = INDEX_NONE;
	int32 LegacyNextSpawnPointIndex = 0;
	int32 NextEntrySelectionIndex = 0;
	bool bEntryRuntimeInitialized = false;
};
