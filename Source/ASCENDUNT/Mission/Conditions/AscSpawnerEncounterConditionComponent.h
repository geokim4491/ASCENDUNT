#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Mission/Conditions/AscObjectiveConditionComponent.h"
#include "AscSpawnerEncounterConditionComponent.generated.h"

class AAISpawner;

/**
 * Objective가 시작될 때 지정한 AI Spawner를 활성화하고,
 * Encounter 완료를 Objective Completion Condition으로 연결한다.
 *
 * AAISpawner는 Mission 타입을 전혀 알지 않으며,
 * Mission -> Condition -> Spawner 방향으로만 의존한다.
 */
UCLASS(Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscSpawnerEncounterConditionComponent : public UAscObjectiveConditionComponent
{
	GENERATED_BODY()

public:
	virtual float GetCurrentProgressValue_Implementation() const override;
	virtual float GetTargetProgressValue_Implementation() const override;

protected:
	virtual void OnActivated() override;
	virtual void OnDeactivated() override;
	virtual void OnReset() override;

	/** 이 Objective에서 진행/완료를 관찰할 Spawner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Spawner")
	TArray<TObjectPtr<AAISpawner>> ObservedSpawners;

	/** ObservedSpawners가 비어 있으면 레벨의 모든 AAISpawner를 대상으로 할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Spawner")
	bool bObserveAllSpawnersIfNoneAssigned = false;

	/** Condition 활성화와 동시에 관찰 Spawner의 ActivateSpawner를 호출한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Spawner")
	bool bActivateSpawnersOnConditionStart = true;

	/** Objective 완료/실패 등으로 Condition이 비활성화될 때 남은 Spawn 진행을 중지한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Spawner")
	bool bStopSpawnersOnConditionEnd = true;

	/** true면 모든 Spawner 완료가 필요하고, false면 하나만 완료되어도 조건을 만족한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Condition|Spawner")
	bool bRequireAllSpawners = true;

private:
	void HandleMonsterDied(AActor* DeadActor);
	void HandleEncounterCompleted(AAISpawner* Spawner);
	void BindSpawners();
	void UnbindSpawners();
	void RefreshAlreadyCompletedSpawners();
	void EvaluateCompletion();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAISpawner>> BoundSpawners;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAISpawner>> CompletedSpawners;

	/** 현재 Condition 활성화 이후 관찰 Spawner에서 처치된 AI 수. */
	int32 CurrentKillCount = 0;

	/** Deactivate 후에도 UI snapshot의 TargetValue가 유지되도록 Activation 시 계산한 총 처치 목표를 보존한다. */
	int32 ResolvedTargetKillCount = 1;

	/** 반복 가능한 Infinite/Legacy Spawner가 포함되어 총 처치 목표를 유한하게 계산할 수 없는지 여부. */
	bool bHasUnboundedKillTarget = false;
};
