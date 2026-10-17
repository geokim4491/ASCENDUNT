// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_AIDeath.h"

#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "ASCENDUNT/Character/AI/AIPawnData.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"

UGA_AIDeath::UGA_AIDeath(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 사망 판정은 서버가 확정한다.
	// Dedicated Server에서는 AI Montage/Pose를 돌리지 않고 클라이언트 연출만 Multicast한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	bRetriggerInstancedAbility = false;

	// AbilityTriggers(Event.Death)와 활성화 소유 태그는 BP 서브클래스 설정을 쓴다.
}

void UGA_AIDeath::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bFinishScheduled = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 연출 및 사망으로 인한 게임플레이 효과
	K2_OnDeathPresentation();

	UAnimMontage* DeathMontage = nullptr;
	if (const ABaseAICharacter* AIChar = Cast<ABaseAICharacter>(GetAvatarActorFromActorInfo()))
	{
		if (const UAIPawnData* PawnData = AIChar->GetPawnData())
		{
			DeathMontage = PawnData->DeathMontage;
		}
	}

	ABaseAICharacter* AIChar = Cast<ABaseAICharacter>(GetAvatarActorFromActorInfo());
	if (!DeathMontage || !AIChar)
	{
		// 몽타주가 없으면 곧바로 사망을 완료한다.
		// 연출 확장점은 몽타주 유무와 무관하게 항상 한 번 불려야 하므로 같은 경로를 탄다.
		HandleDeathPresentationFinished();
		return;
	}

	const float SafePlayRate = FMath::Max(0.01f, DeathMontagePlayRate);

	// 서버는 Pose/Montage Timeline을 계산하지 않는다. 연출은 클라이언트에서만 재생한다.
	AIChar->Multicast_PlayDeathPresentationMontage(DeathMontage, SafePlayRate);

	// Gameplay 수명은 서버 Timer가 소유한다.
	// 기존 PlayMontageAndWait 경로에서는 OnBlendOut 시점에 FinishDeath를 시작했다.
	// 서버 Montage를 제거한 뒤에도 같은 시각적 결과를 유지하기 위해,
	// Montage 에셋의 BlendOut 설정으로 "BlendOut 시작 시점"을 직접 계산한다.
	const float MontageLength = DeathMontage->GetPlayLength();
	float FinishPosition = MontageLength;

	if (DeathMontage->bEnableAutoBlendOut)
	{
		if (DeathMontage->BlendOutTriggerTime >= 0.0f)
		{
			// 0 이상이면 SequenceEnd - BlendOutTriggerTime 에서 BlendOut 시작.
			FinishPosition = MontageLength - DeathMontage->BlendOutTriggerTime;
		}
		else
		{
			// 음수이면 BlendOutTime을 사용해 Montage 끝에서 BlendOut이 완료되므로,
			// 시작 시점은 SequenceEnd - BlendOutTime 이다.
			FinishPosition = MontageLength - DeathMontage->BlendOut.GetBlendTime();
		}
	}

	FinishPosition = FMath::Clamp(FinishPosition, 0.0f, MontageLength);
	const float DeathDuration = FinishPosition / SafePlayRate;
	if (DeathDuration <= KINDA_SMALL_NUMBER)
	{
		HandleDeathPresentationFinished();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathTimerHandle,
			this,
			&ThisClass::HandleDeathPresentationFinished,
			DeathDuration,
			false);
	}
	else
	{
		HandleDeathPresentationFinished();
	}
}

void UGA_AIDeath::HandleDeathPresentationFinished()
{
	// 연출 확장점을 먼저 부른다.
	// ScheduleFinishDeath()는 다음 틱으로 미루므로 순서상 안전하지만,
	// 폰이 사라지기 전에 연출이 나가야 한다는 의도를 코드 순서로 남겨둔다.
	K2_OnDeathMontageFinished();

	ScheduleFinishDeath();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_AIDeath::ScheduleFinishDeath()
{
	if (bFinishScheduled)
	{
		return;
	}
	bFinishScheduled = true;

	// 캐릭터가 다음 틱에 FinishDeath() 를 호출한다.
	// 여기서 직접 부르면 풀 반납까지 이어지면서 이 어빌리티 자신이 회수되는
	// 콜스택 안에 들어가게 된다.
	if (ABaseAICharacter* AIChar = Cast<ABaseAICharacter>(GetAvatarActorFromActorInfo()))
	{
		AIChar->FinishDeathDeferred();
	}
}

void UGA_AIDeath::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 취소/차단으로 끝나도 사망은 반드시 완결되어야 한다.
	// ScheduleFinishDeath 는 EndAbility 를 부르지 않으므로 재귀가 생기지 않는다.
	ScheduleFinishDeath();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathTimerHandle);
	}

	// InstancedPerActor 라 폰이 풀에서 재사용되면 이 인스턴스도 그대로 다시 쓰인다.
	// 초기화하지 않으면 두 번째 죽음에서 ScheduleFinishDeath 가 즉시 return 한다.
	bFinishScheduled = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
