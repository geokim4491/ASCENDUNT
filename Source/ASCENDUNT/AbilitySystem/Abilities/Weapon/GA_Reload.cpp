#include "GA_Reload.h"
// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"

#include "ASCENDUNT/AscenduntGameplayTags.h"

#include "ASCENDUNT/AbilitySystem/AbilityTask/AT_PlayWeaponMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UGA_Reload::UGA_Reload()
{
	// Base가 LocalPredicted / InstancedPerActor라 그대로 사용 가능
	ActivationPolicy = EAscenduntAbilityActivationPolicy::OnInputTriggered;

	// 원하면 여기서 그룹으로도 막을 수 있음(선택)
	// ActivationGroup = EAscenduntAbilityActivationGroup::Exclusive_Blocking;

	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Weapon_Reload);
}

bool UGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	if (!Weapon)
	{
		return false;
	}

	// 무한 탄창이면 “재장전 의미 없음” → 입력은 그냥 무시
	if (Weapon->IsInfiniteMag())
	{
		return false;
	}

	// 이미 재장전 중이면 중복 발동 방지
	if (Weapon->IsReloading())
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return ASC && Weapon->GetOwner() == ActorInfo->AvatarActor.Get() && Weapon->GetCachedAscASC() == ASC && Weapon->GetMagSize() > 0
	       && Weapon->GetCurrentAmmoInMag() < Weapon->GetMagSize();
}

void UGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo();
	AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));

	if (!AscASC || !Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 무한탄창은 그냥 종료(요구사항)
	if (Weapon->IsInfiniteMag())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
		return;
	}

	// WeaponSet에서 MagSize / ReloadTime 읽기 (SetByCaller로 주입된 값)
	const float MagSizeF = AscASC->GetNumericAttribute(UAscenduntWeaponSet::GetMagSizeAttribute());
	const int32 MagSize = FMath::Max(0, FMath::RoundToInt(MagSizeF));

	if (MagSize <= 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
		return;
	}

	// 이미 가득 찼으면 종료
	if (Weapon->GetCurrentAmmoInMag() >= MagSize)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
		return;
	}

	CachedWeapon = Weapon;
	CachedMagSize = MagSize;

	const float ReloadTime = FMath::Max(0.01f, AscASC->GetNumericAttribute(UAscenduntWeaponSet::GetReloadTimeAttribute()));

	// 재장전 시작 플래그: 오너클라 즉시 + 서버 확정(복제)
	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		Weapon->BeginPredictedReload(ActivationInfo.GetActivationPredictionKey());
	}

	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		Weapon->SetReloading_Server(true);
	}

	// 재장전 몽타주
	const FAscenduntWeaponModeStats& S = CachedWeapon->GetCurrentModeStats();
	ABaseAICharacter* AIAvatar = Cast<ABaseAICharacter>(GetAvatarActorFromActorInfo());
	const bool bServerAI = AIAvatar && ActorInfo->IsNetAuthority();

	// 캐릭터 몽타주: (예측) 로컬 + (권한) 서버에서 실행
	// AI Dedicated Server는 Pose를 계산하지 않으므로 클라이언트에만 연출을 전파한다.
	if (S.HeroReloadMontage && bServerAI)
	{
		AIAvatar->Multicast_PlayPresentationMontage(S.HeroReloadMontage, float(S.HeroReloadMontage->GetPlayLength() / ReloadTime), NAME_None);
	}
	else if (S.HeroReloadMontage && (CurrentActorInfo->IsLocallyControlled() || ActorInfo->IsNetAuthority()))
	{
		if (HeroMontageTask)
		{
			HeroMontageTask->EndTask();
			HeroMontageTask = nullptr;
		}

		HeroMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, S.HeroReloadMontage,
		    float(S.HeroReloadMontage->GetPlayLength() / ReloadTime), NAME_None, /*bStopWhenAbilityEnds*/ false);

		if (HeroMontageTask)
		{
			HeroMontageTask->ReadyForActivation();
		}
	}

	// 무기 몽타주: 로컬은 예측으로 즉시, 서버는 NetMulticast로 타 클라에게 보이게
	if (S.WeaponReloadMontage && bServerAI)
	{
		CachedWeapon->Multicast_PlayWeaponMontage(S.WeaponReloadMontage, float(S.WeaponReloadMontage->GetPlayLength() / ReloadTime));
	}
	else if (S.WeaponReloadMontage && (CurrentActorInfo->IsLocallyControlled() || ActorInfo->IsNetAuthority()))
	{
		if (WeaponMontageTask)
		{
			WeaponMontageTask->EndTask();
			WeaponMontageTask = nullptr;
		}

		const bool bReplicateToOthers = ActorInfo->IsNetAuthority(); // 서버만 뿌림(오너는 클라 예측으로 이미 봄)
		WeaponMontageTask = UAT_PlayWeaponMontageAndWait::PlayWeaponMontageAndWait(this, NAME_None, CachedWeapon, S.WeaponReloadMontage,
		    float(S.WeaponReloadMontage->GetPlayLength() / ReloadTime), bReplicateToOthers, /*bStopWhenAbilityEnds*/ false);

		if (WeaponMontageTask)
		{
			WeaponMontageTask->ReadyForActivation();
		}
	}

	// 1인칭 몽타주: 로컬 오너만 보므로 복제하지 않는다. GAS 태스크 대신 직접 재생한다.
	// 위의 3P 재생은 그대로 둔다. 3P 메시가 1인칭에서도 그림자를 만들기 때문에
	// 여기서 3P 를 멈추면 그림자만 가만히 서 있게 된다.
	//
	// 재장전 종료는 아래의 WaitDelay(ReloadTime)가 소유하므로, 이 몽타주는 표현일 뿐
	// 게임플레이 타이밍에 관여하지 않는다.
	if (S.HeroReloadMontage1P && CurrentActorInfo->IsLocallyControlled())
	{
		if (UHeroFirstPersonComponent* FirstPerson = UHeroFirstPersonComponent::FindFirstPersonComponent(GetAvatarActorFromActorInfo()))
		{
			// 배율 계산식은 3P(HeroReloadMontage)와 반드시 같아야 두 메시가 어긋나지 않는다.
			FirstPerson->PlayMontage1P(S.HeroReloadMontage1P, float(S.HeroReloadMontage1P->GetPlayLength() / ReloadTime));
		}
	}

	// 재장전 사운드 재생
	if (CurrentActorInfo->IsLocallyControlled())
	{
		FGameplayCueParameters P;
		P.Instigator = AscASC ? AscASC->GetAvatarActor() : nullptr;
		P.EffectCauser = Cast<AActor>(GetCurrentSourceObject());
		P.SourceObject = GetCurrentSourceObject();

		FGameplayEffectContextHandle Ctx = AscASC->MakeEffectContext();
		Ctx.AddSourceObject(GetCurrentSourceObject());
		P.EffectContext = Ctx;

		AscASC->InvokeGameplayCueEvent(AscenduntGameplayTags::GameplayCue_Local_Weapon_Reload, EGameplayCueEvent::Executed, P);
	}

	// 대기 후 완료
	if (ReloadTime <= KINDA_SMALL_NUMBER)
	{
		OnReloadDelayFinished();
	}
	else // 나중에 애님 노티파이로 변경할거임
	{
		UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
		Task->OnFinish.AddDynamic(this, &UGA_Reload::OnReloadDelayFinished);
		Task->ReadyForActivation();
	}
}

void UGA_Reload::OnReloadDelayFinished()
{
	if (!IsActive() || !CachedWeapon)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	if (!ActorInfo)
	{
		return;
	}

	// Listen host presentation flag; remote clients wait for authoritative completion.
	if (ActorInfo->IsLocallyControlled() && ActorInfo->IsNetAuthority())
	{
		CachedWeapon->FinishPredictedReload(CurrentActivationInfo.GetActivationPredictionKey().Current);
	}

	// 서버 확정: 복제될 값
	if (ActorInfo->IsNetAuthority())
	{
		CachedWeapon->SetAmmoInMag_Server(CachedMagSize);
		CachedWeapon->SetReloading_Server(false);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소/강제종료 시 플래그 복구(안전장치)
	if (CachedWeapon && ActorInfo)
	{
		if (ActorInfo->IsLocallyControlled()
		    && (ActorInfo->IsNetAuthority() || bWasCancelled || ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Rejected))
		{
			CachedWeapon->FinishPredictedReload(ActivationInfo.GetActivationPredictionKey().Current);
		}

		if (ActorInfo->IsNetAuthority())
		{
			CachedWeapon->SetReloading_Server(false);
		}
	}

	if (CachedWeapon && ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (UAscenduntAbilitySystemComponent* ASC = GetAscenduntASCFromActorInfo())
		{
			ASC->Client_WeaponReloadFinished(CachedWeapon, ActivationInfo.GetActivationPredictionKey().Current);
		}
	}

	CachedWeapon = nullptr;
	CachedMagSize = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
