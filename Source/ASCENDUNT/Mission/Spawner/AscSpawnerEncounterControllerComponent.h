#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASCENDUNT/Mission/Objective/AscObjectiveTypes.h"
#include "AscSpawnerEncounterControllerComponent.generated.h"

class AAISpawner;
class AAscObjectiveBase;

/**
 * Objective의 성공/실패 조건과 무관하게 AI Encounter의 수명주기만 제어한다.
 *
 * - Objective가 Active가 되면 지정한 Spawner를 Activate한다.
 * - Objective가 Active 상태를 벗어나면 지정한 Spawner를 Stop한다.
 * - UAscObjectiveConditionComponent가 아니므로 Objective 완료/실패 판정에는 참여하지 않는다.
 *
 * Infinite Spawn, 방어전 중 지속 증원, 보스전 잡몹 증원처럼
 * "적 전멸 != Objective 완료"인 Encounter에 사용한다.
 */
UCLASS(Blueprintable, ClassGroup=(Ascendunt), meta=(BlueprintSpawnableComponent))
class ASCENDUNT_API UAscSpawnerEncounterControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscSpawnerEncounterControllerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 현재 설정으로 관리할 Spawner 목록을 다시 수집한다. */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Spawner")
	void RefreshManagedSpawners();

	/** 관리 중인 Spawner를 수동으로 활성화한다. Objective 완료 조건에는 영향을 주지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Spawner")
	void ActivateManagedSpawners();

	/** 관리 중인 Spawner의 추가 Spawn/Wave 진행을 중단한다. 이미 살아 있는 AI는 유지된다. */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Mission|Spawner")
	void StopManagedSpawners();

protected:
	/** 이 Objective 동안 제어할 Spawner들. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Ascendunt|Mission|Spawner")
	TArray<TObjectPtr<AAISpawner>> ManagedSpawners;

	/** ManagedSpawners가 비어 있으면 레벨의 모든 AAISpawner를 대상으로 할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Spawner")
	bool bManageAllSpawnersIfNoneAssigned = false;

	/** Objective가 Active가 될 때 Spawner를 자동으로 활성화한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Spawner")
	bool bActivateSpawnersOnObjectiveStart = true;

	/** Objective가 완료/실패/비활성화되어 Active 상태를 벗어날 때 Spawner를 중단한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Mission|Spawner")
	bool bStopSpawnersOnObjectiveEnd = true;

private:
	UFUNCTION()
	void HandleObjectiveStateChanged(
		AAscObjectiveBase* Objective,
		EAscObjectiveState OldState,
		EAscObjectiveState NewState);

	AAscObjectiveBase* GetOwningObjective() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAISpawner>> ResolvedSpawners;
};
