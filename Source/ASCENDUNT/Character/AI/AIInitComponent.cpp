// Fill out your copyright notice in the Description page of Project Settings.


#include "AIInitComponent.h"

#include "ASCENDUNT/AI/BaseAIController.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"
#include "BaseAICharacter.h"

UAIInitComponent::UAIInitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAIInitComponent::NotifyPossessed()
{
	Evaluate();

	// PossessedBy가 들어온 이후엔 Controller가 보통 존재하므로,
	// 이미 Ready면 여기서도 재시도
	if (bGameplayReady)
	{
		TryStartBTFromReady();
	}
}

void UAIInitComponent::NotifyControllerRep()
{
	Evaluate();
}

void UAIInitComponent::Shutdown()
{
	if (ABaseAICharacter* Pawn = Cast<ABaseAICharacter>(GetOwner()))
	{
		// 순서 중요.
		// HealthComponent는 HealthSet 인스턴스를 직접 포인터로 들고 있다.
		// AbilitySet 회수가 그 AttributeSet을 ASC에서 제거하므로,
		// 반드시 unbind를 먼저 하고 회수해야 한다.
		if (UAscenduntHealthComponent* HC = Pawn->GetAscenduntHealthComponent())
		{
			HC->UninitializeFromAbilitySystem();
		}

		Pawn->RevokePawnDataAbilities();
	}

	OwnerPawn = nullptr;
	CachedAIC = nullptr;
	CachedASC = nullptr;

	bActorInfoSet = false;
	bHealthCompInitialized = false;
	CurrentState = FGameplayTag::EmptyTag;

	bGameplayReady = false;
	bBTStarted = false;
}

void UAIInitComponent::Evaluate()
{
	if (bGameplayReady)
	{
		return;
	}

	if (!OwnerPawn.IsValid())
	{
		OwnerPawn = Cast<ABaseAICharacter>(GetOwner());
		if (!OwnerPawn.IsValid()) return;
	}

	// 캐시 업데이트 (Possess/Rep 타이밍 흡수)
	{
		ABaseAIController* AIC = Cast<ABaseAIController>(OwnerPawn->GetController());
		if (CachedAIC.Get() != AIC)
		{
			CachedAIC = AIC;
		}

		UAscenduntAbilitySystemComponent* ASC = OwnerPawn->GetAscenduntAbilitySystemComponent();
		if (CachedASC.Get() != ASC)
		{
			CachedASC = ASC;
			bActorInfoSet = false;
			bHealthCompInitialized = false;
		}
	}

	if (!CurrentState.IsValid())
	{
		EnterSpawned();
	}

	if (CurrentState == AscenduntGameplayTags::InitState_Spawned)
	{
		if (CanEnterDataAvailable()) EnterDataAvailable();
		else return;
	}

	if (CurrentState == AscenduntGameplayTags::InitState_DataAvailable)
	{
		if (CanEnterDataInitialized()) EnterDataInitialized();
		else return;
	}

	if (CurrentState == AscenduntGameplayTags::InitState_DataInitialized)
	{
		if (CanEnterGameplayReady()) EnterGameplayReady();
		else return;
	}
}

bool UAIInitComponent::CanEnterDataAvailable() const
{
	if (!OwnerPawn.IsValid()) return false;
	if (!CachedASC.IsValid()) return false;

	// 컨트롤러와 PawnData는 서버 로직(AbilitySet 부여, BT 시작)에만 필요하다.
	// ABaseAIController는 bReplicates=false라 클라이언트에는 존재하지 않고,
	// PawnData도 복제되지 않는다. 클라이언트에서 이 둘을 요구하면 InitState가
	// Spawned에서 영원히 멈추고, HealthComponent가 ASC에 연결되지 못해
	// 클라이언트의 GetHealth()가 항상 0이 된다.
	if (OwnerPawn->HasAuthority())
	{
		// 서버는 Possessed가 사실상 전제지만, 안전하게 체크
		// (특수 스폰/컨트롤러 교체 등)
		if (!OwnerPawn->GetController()) return false;

		// PawnData 없이는 AbilitySet을 부여할 수 없고, 그러면 HealthSet이 없어
		// 다음 단계(DataInitialized)에서 조용히 멈춘다. 명시적으로 알린다.
		if (!OwnerPawn->GetPawnData())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[AIInit] %s 에 PawnData가 없어 초기화를 진행할 수 없습니다. "
					 "레벨 배치 AI라면 BP 기본 PawnData를, 스폰 AI라면 스포너의 SpawnData를 확인하세요."),
				*GetNameSafe(OwnerPawn.Get()));
			return false;
		}
	}

	return true;
}

bool UAIInitComponent::CanEnterDataInitialized() const
{
	if (!OwnerPawn.IsValid() || !CachedASC.IsValid()) return false;

	// ActorInfo 세팅 (AI는 Pawn ASC 패턴)
	if (!bActorInfoSet)
	{
		CachedASC->InitAbilityActorInfo(OwnerPawn.Get(), OwnerPawn.Get());
		bActorInfoSet = CachedASC->AbilityActorInfo.IsValid();
	}
	if (!bActorInfoSet) return false;

	// AttributeSet은 서버가 부여한 뒤 ASC 서브오브젝트로 복제되므로 폰보다 늦게 도착할 수 있다.
	// 클라이언트에서 이걸 요구하면 InitState가 멈추는데, 다시 깨워줄 이벤트가 없다.
	// HealthComponent::RefreshHealthSetFromAbilitySystem()이 지연 연결을 담당하므로
	// 클라이언트는 ActorInfo만 준비되면 다음 단계로 보낸다.
	if (!OwnerPawn->HasAuthority())
	{
		return true;
	}

	// HealthComponent가 GetSet<HealthSet>을 요구하므로 존재 보장
	const bool bHasHealthSet = (CachedASC->GetSet<UAscenduntHealthSet>() != nullptr);
	return bHasHealthSet;
}

bool UAIInitComponent::CanEnterGameplayReady() const
{
	return true;
}

void UAIInitComponent::EnterSpawned()
{
	CurrentState = AscenduntGameplayTags::InitState_Spawned;
}

void UAIInitComponent::EnterDataAvailable()
{
	// GAS 부여는 이 상태 머신이 소유한다.
	// CurrentState가 각 단계를 생애당 1회만 통과시키므로 중복 부여가 구조적으로 불가능하다.
	// (예전에는 PossessedBy와 SetPawnData 양쪽에서 부여해 AttributeSet이 2벌씩 생겼다.)
	if (OwnerPawn.IsValid())
	{
		OwnerPawn->GrantPawnDataAbilities();
	}

	CurrentState = AscenduntGameplayTags::InitState_DataAvailable;
}

void UAIInitComponent::EnterDataInitialized()
{
	CurrentState = AscenduntGameplayTags::InitState_DataInitialized;
}

void UAIInitComponent::EnterGameplayReady()
{
	if (bGameplayReady)
	{
		return;
	}

	bGameplayReady = true;
	
	// 1) HealthComponent 초기화
	if (!bHealthCompInitialized)
	{
		if (UAscenduntHealthComponent* HC = OwnerPawn->GetAscenduntHealthComponent())
		{
			HC->InitializeWithAbilitySystem(CachedASC.Get());
			bHealthCompInitialized = HC->HasValidHealthSet();
		}
	}

	// 2) BT 시작은 여기서 해도 되고, AIController OnPossess에서 해도 됨
	// (개인적으로 BT는 Controller가 소유하는 게 더 깔끔)
	// OwnerPawn->RequestStartLogic(); 같은 함수를 만들어도 됨.

	// 2) WeaponEquipmentComponent 자동 스폰 및 장착 (영웅과 동일)
	if (OwnerPawn->HasAuthority())
	{
		if (ABaseAICharacter* AIChar = Cast<ABaseAICharacter>(OwnerPawn.Get()))
		{
			if (UWeaponEquipmentComponent* EC = AIChar->GetWeaponEquipmentComponent())
			{
				// 풀에서 재사용된 폰은 무기를 그대로 들고 있다.
				// ServerSpawnDefaultWeapons를 직접 부르면 기존 무기가 인벤토리에서만
				// 사라지고 메시에는 붙어 있는 유령 무기가 된다.
				EC->ServerEnsureDefaultWeapons();

				if (EC->GetWeaponInventory().Num() > 0)
				{
					EC->ServerEquipWeapon(EC->GetWeaponInventory()[0]);
				}
			}
		}
	}

	// 서버는 HealthComponent 초기화가 끝나야 준비 완료다.
	// 클라이언트는 HealthSet이 늦게 도착할 수 있고, 그 연결은 HealthComponent가
	// 지연 처리하므로 여기서 막지 않는다.
	const bool bReady = OwnerPawn->HasAuthority() ? bHealthCompInitialized : true;
	if (bReady)
	{
		CurrentState = AscenduntGameplayTags::InitState_GameplayReady;
		
		TryStartBTFromReady();
		OnGameplayReady.Broadcast(OwnerPawn.Get());
	}
}

void UAIInitComponent::TryStartBTFromReady()
{
	// 서버에서만 BT 시작
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority())
	{
		return;
	}

	if (bBTStarted)
	{
		return;
	}

	ABaseAIController* AIC = Cast<ABaseAIController>(Pawn->GetController());
	if (!AIC)
	{
		// 아직 Controller가 없으면 다음 NotifyPossessed/NotifyControllerRep 때 다시 시도됨
		return;
	}

	// PossessedBy() 안에서 여기까지 오는 경로가 있다. 그 시점에는 컨트롤러의 GetPawn()이
	// 아직 세팅되지 않았으므로 폰을 명시적으로 넘겨야 PawnData의 BT를 찾을 수 있다.
	if (AIC->StartBT(Pawn))
	{
		bBTStarted = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AIInit] %s: BT 시작에 실패했습니다."), *GetNameSafe(Pawn));
	}
}
