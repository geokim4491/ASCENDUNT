// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Damage.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/Character/AI/AIPawnData.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"

ABaseAIController::ABaseAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// BT/BB
	BehaviorComp = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorComp"));
	BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));

	// Perception
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(*PerceptionComp);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));

	ConfigurePerception();

	// 서버 전용 로직이 대부분이라, 일반적으로 AIController replication은 불필요
	bReplicates = false;
}

bool ABaseAIController::IsServerAuth() const
{
	// AIController는 보통 서버에만 있으니 이 체크로 충분
	return HasAuthority();
}

void ABaseAIController::ConfigurePerception()
{
	if (!PerceptionComp) return;

	// ---- Sight ----
	if (SightConfig)
	{
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionHalfAngleDeg;
		SightConfig->SetMaxAge(SightMaxAge);

		// 팀 기반 감지: Attitude는 GetTeamAttitudeTowards로 결정됨
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

		PerceptionComp->ConfigureSense(*SightConfig);
		PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
	}

	// ---- Damage ----
	if (DamageConfig)
	{
		DamageConfig->SetMaxAge(5.f);
		PerceptionComp->ConfigureSense(*DamageConfig);
	}

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ABaseAIController::HandleTargetPerceptionUpdated);
}

ETeamAttitude::Type ABaseAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<IGenericTeamAgentInterface>(&Other);
	if (!OtherTeamAgent)
	{
		// 팀 인터페이스가 없다면 Neutral 처리
		return ETeamAttitude::Neutral;
	}

	const FGenericTeamId OtherId = OtherTeamAgent->GetGenericTeamId();
	if (OtherId == TeamId)
	{
		return ETeamAttitude::Friendly;
	}

	// 255 같은 “무소속/Unknown”을 Neutral로 처리하고 싶으면 여기서 정책화
	return ETeamAttitude::Hostile;
}

void ABaseAIController::SetTeamId(uint8 NewTeamId)
{
	TeamId = FGenericTeamId(NewTeamId);

	// Perception affiliation은 Attitude 콜백 기반이므로 즉시 반영됨.
	// 필요시 Perception 리셋 가능:
	// if (PerceptionComp) PerceptionComp->RequestStimuliListenerUpdate();
}

void ABaseAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SyncTeamIdFromPawnIfPossible(InPawn);

	// 회전 모드를 비전투로 초기화한다.
	//
	// SetTargetActor 는 값이 바뀔 때만 이 처리를 하는데(Old == NewTarget 이면 조기 반환),
	// 스폰 직후에는 null -> null 이라 한 번도 불리지 않는다. 그래서 BP 값이 그대로 남는데,
	// bUseControllerRotationYaw 만 켜진 상태로 Focus 가 없으면
	//   UpdateControlRotation: ControlRotation = 폰의 현재 회전
	//   FaceRotation:          폰 회전 = ControlRotation
	// 이 되어 서로를 참조하며 폰이 영원히 회전하지 않는다.
	//
	// AI 시야 방향은 AController::GetActorEyesViewPoint 를 통해 ControlRotation 을 쓰므로,
	// 회전이 멈추면 시야 원뿔이 스폰 방향에 고정되어 옆에서 접근하는 적을 감지하지 못한다.
	SetCombatRotationMode(false);

	// --- 추가할 로직: PawnData로부터 시야 설정 동기화 ---
	if (ABaseAICharacter* AIChar = Cast<ABaseAICharacter>(InPawn))
	{
		if (const UAIPawnData* PawnData = AIChar->GetPawnData())
		{
			// 이미 생성된 SightConfig의 수치를 PawnData에 적힌 값으로 변경
			if (SightConfig)
			{
				SightConfig->SightRadius = PawnData->SightRadius;
				SightConfig->LoseSightRadius = PawnData->LoseSightRadius;
				SightConfig->PeripheralVisionAngleDegrees = PawnData->FieldOfView;

				// 변경된 설정을 PerceptionComponent에 다시 적용
				PerceptionComp->ConfigureSense(*SightConfig);
				PerceptionComp->RequestStimuliListenerUpdate();
			}
		}
	}
	// BT는 여기서 자동 실행할 수도 있고, InitComponent의 GameplayReady 이벤트에서 시작할 수도 있음.
	// 더 좋은 구조(추천): InitComponent가 "GameplayReady"되면 Controller가 BT 시작.
}

void ABaseAIController::OnUnPossess()
{
	StopBehaviorTreeLogic(TEXT("UnPossess"));
	ClearTargetActor();

	Super::OnUnPossess();
}

void ABaseAIController::SyncTeamIdFromPawnIfPossible(APawn* InPawn)
{
	// Pawn(캐릭터)에서 구현한 IGenericTeamAgentInterface를 동기화
	if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(InPawn))
	{
		TeamId = PawnTeam->GetGenericTeamId();
	}
}

bool ABaseAIController::StartBT(APawn* InPawn)
{
	// AIController는 서버에만 존재하는 게 정석이므로 서버 가드
	if (!HasAuthority())
	{
		return false;
	}

	// PossessedBy() 경로에서는 GetPawn()이 아직 null이므로 호출자가 넘긴 폰을 우선한다.
	const APawn* TargetPawn = InPawn;
	if (!TargetPawn)
	{
		TargetPawn = GetPawn();
	}

	// PawnData의 BT를 우선한다. 몬스터마다 다른 BT를 쓰는 게 데이터 주도 설계의 전제고,
	// DefaultBehaviorTree는 PawnData가 BT를 지정하지 않은 경우의 폴백이다.
	UBehaviorTree* BTToRun = DefaultBehaviorTree;
	if (const ABaseAICharacter* AIChar = Cast<const ABaseAICharacter>(TargetPawn))
	{
		if (const UAIPawnData* PD = AIChar->GetPawnData())
		{
			if (PD->BehaviorTreeAsset)
			{
				BTToRun = PD->BehaviorTreeAsset;
			}
		}
	}

	if (!BTToRun)
	{
		// 조용히 실패하면 "AI가 가만히 서 있는데 로그는 깨끗한" 상황이 된다.
		UE_LOG(LogTemp, Warning,
			TEXT("[AI] %s: 실행할 BehaviorTree가 없습니다. "
				 "PawnData의 BehaviorTreeAsset 또는 컨트롤러의 DefaultBehaviorTree를 확인하세요."),
			*GetNameSafe(TargetPawn));
		return false;
	}

	return StartBehaviorTreeAsset(BTToRun);
}

void ABaseAIController::SuspendLogic()
{
	if (!IsServerAuth())
	{
		return;
	}

	// PathFollowing을 먼저 멈춘다. 폰의 CharacterMovement 틱이 꺼지기 전에 정리되어야 한다.
	StopMovement();

	StopBehaviorTreeLogic(TEXT("SuspendLogic"));
	ClearTargetActor();

	if (PerceptionComp)
	{
		PerceptionComp->ForgetAll();
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		PerceptionComp->SetSenseEnabled(UAISense_Damage::StaticClass(), false);
	}

	SetActorTickEnabled(false);
}

void ABaseAIController::ResumeLogic()
{
	if (!IsServerAuth())
	{
		return;
	}

	SetActorTickEnabled(true);

	// 풀에서 재사용된 폰은 죽을 때의 전투 회전 모드를 그대로 들고 나온다.
	// SuspendLogic 시점에 ClearTargetActor 로 타깃이 비워졌으므로, 되살아난 뒤
	// SetTargetActor 는 null -> null 조기 반환에 걸려 복구해주지 않는다.
	SetCombatRotationMode(false);

	if (PerceptionComp)
	{
		PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
		PerceptionComp->SetSenseEnabled(UAISense_Damage::StaticClass(), true);
		PerceptionComp->RequestStimuliListenerUpdate();
	}
}

void ABaseAIController::StopBT(const FString& Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	// Brain 기반 정지 권장
	if (BrainComponent)
	{
		BrainComponent->StopLogic(Reason);
	}

	if (UBehaviorTreeComponent* BTC = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTC->StopTree(EBTStopMode::Safe);
	}
}

bool ABaseAIController::StartBehaviorTreeAsset(UBehaviorTree* BTAsset)
{
	if (!IsServerAuth() || !BTAsset)
	{
		return false;
	}

	// Blackboard init
	if (BTAsset->BlackboardAsset)
	{
		UBlackboardComponent* BBComp = BlackboardComp.Get();
		if (!UseBlackboard(BTAsset->BlackboardAsset, BBComp))
		{
			return false;
		}
	}

	// Run BT
	const bool bOk = RunBehaviorTree(BTAsset);
	return bOk;
}

void ABaseAIController::StopBehaviorTreeLogic(const FString& Reason)
{
	if (!IsServerAuth())
	{
		return;
	}

	// AAIController::BrainComponent 기반 정지(권장)
	if (BrainComponent)
	{
		BrainComponent->StopLogic(Reason);
	}

	// BehaviorTreeComponent가 있으면 추가로 StopTree도 안전
	if (BehaviorComp)
	{
		BehaviorComp->StopTree(EBTStopMode::Safe);
	}
}

bool ABaseAIController::IsBehaviorTreeRunning() const
{
	if (const UBrainComponent* Brain = BrainComponent)
	{
		return Brain->IsRunning();
	}
	return false;
}

AActor* ABaseAIController::GetTargetActor() const
{
	return BlackboardComp ? Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKeyName)) : nullptr;
}

void ABaseAIController::SetAutoTargetFromPerceptionEnabled(const bool bEnabled)
{
	if (!IsServerAuth())
	{
		return;
	}

	bAutoSetTargetFromPerception = bEnabled;
}

bool ABaseAIController::IsValidCombatTarget(const AActor* Candidate) const
{
	if (!Candidate || GetTeamAttitudeTowards(*Candidate) != ETeamAttitude::Hostile)
	{
		return false;
	}

	if (const UAscenduntHealthComponent* HealthComp = UAscenduntHealthComponent::FindHealthComponent(Candidate))
	{
		if (HealthComp->IsDeadOrDying())
		{
			return false;
		}
	}

	return true;
}

void ABaseAIController::SetTargetActor(AActor* NewTarget)
{
	if (!IsServerAuth() || !BlackboardComp)
	{
		return;
	}

	if (NewTarget && !IsValidCombatTarget(NewTarget))
	{
		NewTarget = nullptr;
	}

	AActor* Old = GetTargetActor();
	if (Old == NewTarget)
	{
		return;
	}

	BlackboardComp->SetValueAsObject(TargetActorKeyName, NewTarget);

	// 조준의 기준을 만든다.
	// UGA_BaseFire::BuildViewRayAndMuzzle 은 AI의 조준선으로 폰의 GetBaseAimRotation()
	// (= 이 컨트롤러의 ControlRotation)을 쓴다. Focus 가 없으면 ControlRotation 이
	// 폰 방향을 그대로 따라가서, 무빙 중에는 총알이 이동 방향으로 나간다.
	if (NewTarget)
	{
		SetFocus(NewTarget, EAIFocusPriority::Gameplay);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}

	SetCombatRotationMode(NewTarget != nullptr);

	OnTargetChanged.Broadcast(Old, NewTarget);
}

void ABaseAIController::ClearTargetActor()
{
	SetTargetActor(nullptr);
}

void ABaseAIController::SetCombatRotationMode(bool bInCombat)
{
	ACharacter* Char = Cast<ACharacter>(GetPawn());
	if (!Char)
	{
		return;
	}

	// 전투 중: 컨트롤러 회전(= Focus 방향)을 따라간다.
	//   무빙 사격의 전제다. 이동 방향과 무관하게 표적을 정면으로 본다.
	//
	// 비전투: 이동 방향을 향한다.
	//   전투 설정을 그대로 두면 Focus 가 없어 ControlRotation 이 현재 폰 회전을
	//   그대로 유지하고, 순찰 중 폰이 회전하지 않은 채 옆으로 미끄러진다.
	//
	// 두 플래그는 서로 상대를 덮어쓰므로 반드시 반대로 짝지어야 한다.
	Char->bUseControllerRotationYaw = bInCombat;

	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = !bInCombat;
	}
}

void ABaseAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!IsServerAuth() || !bAutoSetTargetFromPerception || !Actor || !BlackboardComp)
	{
		return;
	}

	// 감지 성공(시야/피해 등)
	if (Stimulus.WasSuccessfullySensed())
	{
		// Hostile이면서 살아있는 타깃만 획득한다. PvE Downed는 DeathStarted이므로 제외된다.
		if (IsValidCombatTarget(Actor))
		{
			// 이미 타겟이 있으면 “우선순위 정책”을 여기서 결정:
			// - 가까운 적
			// - 최근에 데미지 준 적
			// - 현재 타겟 유지 등
			// 지금은 단순히 덮어씀.
			SetTargetActor(Actor);
		}
		return;
	}

	// 감지 끊김: 현재 타겟이 Actor면 클리어
	if (GetTargetActor() == Actor)
	{
		ClearTargetActor();
	}
}
