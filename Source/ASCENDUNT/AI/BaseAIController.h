// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Perception/AIPerceptionTypes.h"

#include "CoreMinimal.h"
#include "AIController.h"
#include "BaseAIController.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Damage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAITargetChanged, AActor*, OldTarget, AActor*, NewTarget);


/**
 * 
 */
UCLASS()
class ASCENDUNT_API ABaseAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	ABaseAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ===== Team =====
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Team")
	void SetTeamId(uint8 NewTeamId);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Team")
	uint8 GetTeamId() const { return TeamId.GetId(); }

	// ===== BT / BB =====
	// InPawn: PossessedBy() 안에서 호출되는 경로가 있는데, 그 시점에는
	// AController::SetPawn()이 아직 실행되지 않아 GetPawn()이 null이다.
	// 그런 경우 호출자가 폰을 직접 넘겨야 PawnData의 BT를 찾을 수 있다.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|BT")
	bool StartBT(APawn* InPawn = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|BT")
	void StopBT(const FString& Reason = TEXT("StopBT"));

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|BT")
	bool StartBehaviorTreeAsset(UBehaviorTree* BTAsset);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|BT")
	void StopBehaviorTreeLogic(const FString& Reason = TEXT("StopBehaviorTreeLogic"));

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|BT")
	bool IsBehaviorTreeRunning() const;

	// ===== Logic Suspend / Resume =====
	// 폰이 비활성화되는 동안(오브젝트 풀 대기 등) 컨트롤러가 possess를 유지한 채
	// 생각만 멈추게 한다. 컨트롤러를 UnPossess 하면 재사용 시 새로 스폰되어 누수가 난다.
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Logic")
	void SuspendLogic();

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Logic")
	void ResumeLogic();

	// ===== Target (Blackboard) =====
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Target")
	void SetTargetActor(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Target")
	void ClearTargetActor();

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Target")
	AActor* GetTargetActor() const;

	/**
	 * Perception은 계속 실행하되, 감지 결과가 TargetActor를 자동으로 덮어쓸지 결정한다.
	 * AI Stress Test처럼 Target을 외부에서 결정적으로 주입해야 하는 경우 false로 둔다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Target")
	void SetAutoTargetFromPerceptionEnabled(bool bEnabled);

	/** Rejects Downed/dying/dead actors so AI never acquires or keeps them as combat targets. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|AI|Target")
	bool IsValidCombatTarget(const AActor* Candidate) const;

	/**
	 * 전투/비전투 회전 모드를 전환한다.
	 *
	 * 전투: bUseControllerRotationYaw = true  (Focus 방향을 본다. 무빙 사격의 전제)
	 * 비전투: bOrientRotationToMovement = true (이동 방향을 본다. 순찰용)
	 *
	 * SetTargetActor 가 타깃 유무에 따라 자동으로 호출한다.
	 * 후퇴처럼 일시적으로 등을 돌려야 하는 행동은 이 함수를 직접 불러 끄고,
	 * 끝나면 반드시 되돌릴 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI|Target")
	void SetCombatRotationMode(bool bInCombat);

	UPROPERTY(BlueprintAssignable, Category = "Ascendunt|AI|Target")
	FAITargetChanged OnTargetChanged;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

protected:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI")
	TObjectPtr<UBehaviorTreeComponent> BehaviorComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI")
	TObjectPtr<UBlackboardComponent> BlackboardComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI")
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|AI|BT")
	TObjectPtr<UBehaviorTree> DefaultBehaviorTree;

	// Sense configs
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

	// Team
	UPROPERTY(EditAnywhere, Category = "Ascendunt|AI|Team")
	FGenericTeamId TeamId = FGenericTeamId(2);

	// Blackboard keys
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|AI|Blackboard")
	FName TargetActorKeyName = TEXT("TargetActor");

	// Optional: auto target by perception
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|AI|Perception")
	bool bAutoSetTargetFromPerception = true;

	// Sight tuning
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|AI|Perception|Sight")
	float SightRadius = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|AI|Perception|Sight")
	float LoseSightRadius = 3500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|AI|Perception|Sight")
	float PeripheralVisionHalfAngleDeg = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|AI|Perception|Sight")
	float SightMaxAge = 2.0f;

protected:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	void ConfigurePerception();
	void SyncTeamIdFromPawnIfPossible(APawn* InPawn);

	// Authority guards
	bool IsServerAuth() const;
	
	
};
