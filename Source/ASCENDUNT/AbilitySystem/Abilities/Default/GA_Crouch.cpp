// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_Crouch.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"

// 테스트 용
#include "ASCENDUNT/Camera/AscenduntCameraMode.h"
#include "ASCENDUNT/Character/Hero/HeroCameraComponent.h"

UGA_Crouch::UGA_Crouch()
{
	// Hold형이니까 WhileInputActive가 가장 자연스럽다.
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;

	// 기본값은 UAscenduntGameplayAbility ctor에서:
	// InstancedPerActor / LocalPredicted / ClientOrServer 등 세팅되어 있음.

	// 선택: 능력 활성 중 태그 부여(프로젝트 태그 있으면)
	// ActivationOwnedTags.AddTag(AscenduntGameplayTags::Status_Movement_Crouching);
}

void UGA_Crouch::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/true, /*bWasCancelled*/true);
		return;
	}

	StartCrouch();

	// 테스트 용 3인칭 보기. 누르고 있는 동안이 아니라 토글이다.
	//
	// SetCameraMode() 는 어빌리티가 끝날 때 자동으로 해제되므로 홀드 동작이 된다.
	// 대신 HeroCameraComponent 에 래치를 걸어서, 다시 누를 때까지 유지되게 한다.
	// 로컬 컨트롤러에서만 토글해야 서버에서도 같이 뒤집혀 상태가 어긋나지 않는다.
	if (ThirdPersonCameraMode && ActorInfo && ActorInfo->IsLocallyControlled())
	{
		if (UHeroCameraComponent* HeroCamera = GetHeroCameraComponentFromActorInfo())
		{
			HeroCamera->ToggleThirdPersonView(ThirdPersonCameraMode);
		}
	}
}

void UGA_Crouch::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	// Release 이벤트는 클라에서만 받는다는 전제 -> 서버에 release 전달
	if (UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo())
	{
		if (CurrentActorInfo && CurrentActorInfo->IsLocallyControlled() && !CurrentActorInfo->IsNetAuthority())
		{
			AscASC->ServerSetInputReleased(CurrentSpecHandle);
		}
	}

	StopCrouch_TryAndRetry();

	// EndAbility가 서버에도 전달되도록 bReplicateEndAbility=true 권장
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/true, /*bWasCancelled*/false);
}

void UGA_Crouch::StartCrouch()
{
	ACharacter* Char = GetCharacterFromActorInfo();
	if (!Char)
	{
		return;
	}

	// 로컬 예측 + 서버 권위 둘 다에서 호출해도 안전.
	// 서버에서 최종 상태가 복제됨.
	if (!Char->bIsCrouched)
	{
		Char->Crouch(/*bClientSimulation*/false);
	}
}

void UGA_Crouch::StopCrouch_TryAndRetry()
{
	// 1) 즉시 1회 시도
	if (TryUncrouchOnce())
	{
		ClearRetryTimer();
		return;
	}

	// 2) 천장 등으로 못 일어나면, 짧게 재시도
	RetryUncrouchTick();
}

bool UGA_Crouch::TryUncrouchOnce()
{
	ACharacter* Char = GetCharacterFromActorInfo();
	if (!Char)
	{
		return true; // 캐릭터 없으면 더 할 게 없음
	}

	if (Char->bIsCrouched)
	{
		Char->UnCrouch(/*bClientSimulation*/false);
	}

	// 여전히 crouched면 실패로 간주
	return !Char->bIsCrouched;
}

void UGA_Crouch::RetryUncrouchTick()
{
	if (!CurrentActorInfo)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이미 타이머 있으면 유지
	if (World->GetTimerManager().IsTimerActive(RetryUncrouchTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RetryUncrouchTimer,
		this,
		&UGA_Crouch::RetryUncrouchTick,
		0.05f,
		true);

	// 즉시 한번 더 체크
	if (TryUncrouchOnce())
	{
		ClearRetryTimer();
	}
}

void UGA_Crouch::ClearRetryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryUncrouchTimer);
	}
}

void UGA_Crouch::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ClearRetryTimer();

	// 취소/종료 시점에 혹시 crouch가 남아있으면 마지막으로 정리 시도
	TryUncrouchOnce();

	ClearCameraMode();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
