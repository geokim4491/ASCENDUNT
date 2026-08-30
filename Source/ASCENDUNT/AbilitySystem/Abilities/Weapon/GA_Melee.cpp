// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_Melee.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "ASCENDUNT/System/AscCollisionChannels.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "GenericTeamAgentInterface.h"
#include "ASCENDUNT/AbilitySystem/AbilityTask/AT_PlayWeaponMontageAndWait.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

static TAutoConsoleVariable<int32> CVarAscDebugMelee(
	TEXT("asc.DebugMelee"),
	0,
	TEXT("0=off, 1=draw melee traces"),
	ECVF_Cheat
);

namespace AscMeleeInternal
{
	static bool IsFriendly(const AActor* A, const AActor* B)
	{
		const IGenericTeamAgentInterface* TA = Cast<IGenericTeamAgentInterface>(A);
		const IGenericTeamAgentInterface* TB = Cast<IGenericTeamAgentInterface>(B);
		return (TA && TB) && (TA->GetGenericTeamId() == TB->GetGenericTeamId());
	}

	static bool IsFullHealth(const AActor* Actor)
	{
		const UAscenduntHealthComponent* HC = UAscenduntHealthComponent::FindHealthComponent(Actor);
		if (!HC || !HC->HasValidHealthSet())
		{
			return false;
		}
		const float H = HC->GetHealth();
		const float M = HC->GetMaxHealth();
		return (M > 0.f) && (H >= M - KINDA_SMALL_NUMBER);
	}

	static void AddIgnoreWeaponAndOwner(FCollisionQueryParams& Params, ACharacter* Char, AAscenduntWeapon* Weapon)
	{
		if (Char) Params.AddIgnoredActor(Char);
		if (Weapon) Params.AddIgnoredActor(Weapon);
	}
}

UGA_Melee::UGA_Melee()
{
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 활성화된 GA_Melee가 InputPressed를 서버에서도 받을 수 있게 한다.
	bReplicateInputDirectly = true;

	TraceChannel = AscCollision::WeaponTrace;

	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Weapon_Melee);
	AbilityTags.RemoveTag(AscenduntGameplayTags::Ability_Weapon_Fire);
}

bool UGA_Melee::GetMeleeStep(const AAscenduntWeapon* Weapon, int32 StepIndex, const FAscenduntMeleeStep*& OutStep) const
{
	OutStep = nullptr;
	if (!Weapon) return false;

	const FAscenduntWeaponModeStats& Stats = Weapon->GetCurrentModeStats();
	if (Stats.MeleeSteps.Num() <= 0) return false;

	const int32 Idx = FMath::Clamp(StepIndex, 0, Stats.MeleeSteps.Num() - 1);
	OutStep = &Stats.MeleeSteps[Idx];
	return true;
}

float UGA_Melee::GetMeleePlayRate(const AAscenduntWeapon* Weapon, const FAscenduntMeleeStep* Step) const
{
	float AttributePlayRate = 1.f;
	if (const UAscenduntAbilitySystemComponent* AscASC = GetAscenduntASCFromActorInfo())
	{
		AttributePlayRate = AscASC->GetNumericAttribute(UAscenduntWeaponSet::GetMeleePlayRateAttribute());
		if (AttributePlayRate <= 0.f)
		{
			AttributePlayRate = 1.f;
		}
	}

	const float ModePlayRate = Weapon ? FMath::Max(0.01f, Weapon->GetCurrentModeStats().MeleePlayRate) : 1.f;
	const float StepPlayRate = Step ? FMath::Max(0.01f, Step->MontagePlayRate) : 1.f;

	return FMath::Max(0.01f, AttributePlayRate * ModePlayRate * StepPlayRate);
}

float UGA_Melee::GetMeleeComboGateSeconds(const AAscenduntWeapon* Weapon, const FAscenduntMeleeStep* Step) const
{
	if (!Step)
	{
		return 0.1f;
	}

	const float PlayRate = GetMeleePlayRate(Weapon, Step);
	const float CloseTime = FMath::Max(Step->ComboInputCloseTime, Step->ComboInputOpenTime);
	return FMath::Max(0.05f, CloseTime / PlayRate);
}

bool UGA_Melee::HasNextComboStep(const AAscenduntWeapon* Weapon) const
{
	if (!Weapon)
	{
		return false;
	}

	const FAscenduntWeaponModeStats& Stats = Weapon->GetCurrentModeStats();
	return Stats.MeleeSteps.IsValidIndex(CurrentStepIndex + 1);
}

bool UGA_Melee::BuildMeleeAttackContext(float DamageCoef, int32 AttackIdx, FAscenduntWeaponAttackContext& OutCtx, bool bDoCommit, bool bCheckFireGate)
{
	UAbilitySystemComponent* ASC = nullptr;
	UAscenduntAbilitySystemComponent* AscASC = nullptr;
	AAscenduntWeapon* Weapon = nullptr;

	if (!GetASCAndWeapon(ASC, AscASC, Weapon))
	{
		return false;
	}

	const FAscenduntMeleeStep* Step = nullptr;
	GetMeleeStep(Weapon, CurrentStepIndex, Step);

	// 근접은 탄 체크/소모 없음
	const int32 AmmoCost = 0;

	UWorld* World = CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()
		? CurrentActorInfo->AvatarActor->GetWorld()
		: nullptr;
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const bool bAuth = CurrentActorInfo->IsNetAuthority();
	const float Interval = GetMeleeComboGateSeconds(Weapon, Step);

	// 첫 공격 활성화만 무기 시간 게이트를 확인한다.
	// 콤보 Step 전환은 같은 Ability 안에서 일어나는 상태 전환이므로 여기서 막으면 안 된다.
	if (bCheckFireGate)
	{
		if (Weapon && !Weapon->CanFireByTime(bAuth, Now))
		{
			return false;
		}
		if (Weapon && CurrentActorInfo->IsLocallyControlled() && !bAuth)
		{
			if (!Weapon->CanFireByTime(false, Now))
			{
				return false;
			}
		}
	}

	if (bDoCommit)
	{
		if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		{
			return false;
		}
	}

	OutCtx.ASC = ASC;
	OutCtx.AscASC = AscASC;
	OutCtx.Weapon = Weapon;
	OutCtx.AmmoCost = AmmoCost;
	OutCtx.Now = Now;
	OutCtx.Interval = Interval;
	OutCtx.bAuthority = bAuth;
	OutCtx.DamageCoef = DamageCoef;
	OutCtx.ShotIndex = AttackIdx;

	return true;
}

void UGA_Melee::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ResetComboState();

	// 먼저 SwingCtx를 구성한다.
	StartComboStep(/*StepIndex*/0, /*bInitialStep*/true);

	// StartComboStep 실패로 Ability가 종료됐으면 바인딩하지 않는다.
	if (!IsActive())
	{
		return;
	}

	// SwingCtx가 준비된 후 HitWindow를 연결한다.
	BindWeaponHitWindow();
}

void UGA_Melee::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	// 서버 GA에도 "다시 공격 입력"을 전달한다.
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() && !ActorInfo->IsNetAuthority())
	{
		ActorInfo->AbilitySystemComponent->ServerSetInputPressed(Handle);
	}

	// ComboInputOpenTime 이전 입력은 "다음 공격 입력"으로 인정하지 않는다.
	// 다른 입력 전체를 막는 게 아니라, 이 Melee Ability 안의 공격 입력만 무시한다.
	if (!bComboInputOpen || bFinishingMelee)
	{
		return;
	}

	TryAdvanceComboStepFromInput();
}

void UGA_Melee::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	bInputReleased = true;

	// 콤보 공격은 Release로 즉시 종료하지 않는다.
	// ComboInputCloseTime이 지나면 CurrentStepIndex/AttackIndex를 초기화하며 종료한다.
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() && !ActorInfo->IsNetAuthority())
	{
		ActorInfo->AbilitySystemComponent->ServerSetInputReleased(Handle);
	}
}

void UGA_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ClearComboTimers();
	StopServerHitTick();

	if (bStopMontagesOnEnd || bWasCancelled)
	{
		StopCurrentMeleeMontagesNoBlend();
	}
	else
	{
		// 정상 ComboCloseTime 종료는 현재 몽타주를 끊지 않는다.
		// 콤보 인덱스/Ability 상태만 초기화하고 애니메이션의 회수 구간은 자연 재생시킨다.
		if (HeroMeleeMontageTask)
		{
			HeroMeleeMontageTask->EndTask();
			HeroMeleeMontageTask = nullptr;
		}
		if (WeaponMeleeMontageTask)
		{
			WeaponMeleeMontageTask->EndTask();
			WeaponMeleeMontageTask = nullptr;
		}
	}

	UnbindWeaponHitWindow();
	ResetComboState();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Melee::ResetComboState()
{
	bComboInputOpen = false;
	bFinishingMelee = false;
	bInputReleased = false;
	bStopMontagesOnEnd = false;
	bServerHitWindowActive = false;
	ActiveHitWindowNotifyStepIndex = INDEX_NONE;
	ActiveHitWindowAbilityStepIndex = INDEX_NONE;
	AttackIndex = 0;
	CurrentStepIndex = 0;
	SwingCtx = FAscenduntWeaponAttackContext();
	HitActorsThisWindow.Reset();
}

void UGA_Melee::ClearComboTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ComboInputOpenTimer);
		World->GetTimerManager().ClearTimer(ComboInputCloseTimer);
	}
}

void UGA_Melee::StartComboStep(int32 StepIndex, bool bInitialStep)
{
	if (bFinishingMelee || !CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		FinishMeleeAbility(true);
		return;
	}

	// 콤보 전환 시 이전 Step의 HitWindow/타이머를 먼저 정리한다.
	// CurrentStepIndex를 새 Step으로 바꾸기 전에 끝내야 hero-specific hook이 이전 Step을 올바르게 받는다.
	const int32 PreviousStepIndex = CurrentStepIndex;
	if (bServerHitWindowActive)
	{
		OnMeleeHitWindowEnded(PreviousStepIndex);
	}

	ClearComboTimers();
	bComboInputOpen = false;
	bServerHitWindowActive = false;
	ActiveHitWindowNotifyStepIndex = INDEX_NONE;
	ActiveHitWindowAbilityStepIndex = INDEX_NONE;
	HitActorsThisWindow.Reset();
	StopServerHitTick();

	// 콤보 전환은 블렌드 없이 즉시 이전 몽타주를 끊는다.
	StopCurrentMeleeMontagesNoBlend();

	CurrentStepIndex = FMath::Max(0, StepIndex);

	FAscenduntWeaponAttackContext Ctx;
	if (!BuildMeleeAttackContext(/*DamageCoef*/1.f, AttackIndex, Ctx, /*bDoCommit*/bInitialStep, /*bCheckFireGate*/bInitialStep))
	{
		FinishMeleeAbility(true);
		return;
	}

	const FAscenduntMeleeStep* Step = nullptr;
	if (!GetMeleeStep(Ctx.Weapon, CurrentStepIndex, Step) || !Step)
	{
		FinishMeleeAbility(true);
		return;
	}

	SwingCtx = Ctx;

	const float PlayRate = GetMeleePlayRate(Ctx.Weapon, Step);
	PlayMeleeStepMontages(*Step, Ctx.Weapon, PlayRate, Ctx.bAuthority);

	// 첫 공격에만 시간 게이트를 건다. 후속 콤보 Step은 FireGate로 막지 않는다.
	if (bInitialStep && Ctx.Weapon)
	{
		Ctx.Weapon->MarkFired(Ctx.bAuthority, Ctx.Now, Ctx.Interval);
		if (CurrentActorInfo->IsLocallyControlled() && !Ctx.bAuthority)
		{
			Ctx.Weapon->MarkFired(false, Ctx.Now, Ctx.Interval);
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishMeleeAbility(true);
		return;
	}

	const float OpenDelay = FMath::Max(0.f, Step->ComboInputOpenTime / PlayRate);
	const float CloseDelay = FMath::Max(OpenDelay, Step->ComboInputCloseTime / PlayRate);

	World->GetTimerManager().SetTimer(ComboInputOpenTimer, this, &ThisClass::OpenComboInput, OpenDelay, false);
	World->GetTimerManager().SetTimer(ComboInputCloseTimer, this, &ThisClass::CloseComboInput, CloseDelay, false);

	++AttackIndex;
}

void UGA_Melee::PlayMeleeStepMontages(const FAscenduntMeleeStep& Step, AAscenduntWeapon* Weapon, float PlayRate, bool bReplicateWeaponMontage)
{
	if (Step.HeroMontage)
	{
		HeroMeleeMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			Step.HeroMontage,
			PlayRate,
			NAME_None,
			/*bStopWhenAbilityEnds*/false);

		if (HeroMeleeMontageTask)
		{
			HeroMeleeMontageTask->ReadyForActivation();
		}
	}

	// 1P Arm Melee Montage
	if (Step.HeroMontage1P &&
		CurrentActorInfo &&
		CurrentActorInfo->IsLocallyControlled())
	{
		if (UHeroFirstPersonComponent* FirstPerson =
			UHeroFirstPersonComponent::FindFirstPersonComponent(
				GetAvatarActorFromActorInfo()))
		{
			FirstPerson->PlayMontage1P(
				Step.HeroMontage1P,
				PlayRate);
		}
	}

	if (Step.WeaponMontage && Weapon)
	{
		WeaponMeleeMontageTask = UAT_PlayWeaponMontageAndWait::PlayWeaponMontageAndWait(
			this,
			NAME_None,
			Weapon,
			Step.WeaponMontage,
			PlayRate,
			/*bReplicateToOthers*/bReplicateWeaponMontage,
			/*bStopWhenAbilityEnds*/false);

		if (WeaponMeleeMontageTask)
		{
			WeaponMeleeMontageTask->ReadyForActivation();
		}
	}
}

void UGA_Melee::StopCurrentMeleeMontagesNoBlend()
{
	if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		CurrentActorInfo->AbilitySystemComponent->CurrentMontageStop(0.f);
	}

	AAscenduntWeapon* Weapon = CachedWeapon.Get();
	if (!Weapon && SwingCtx.Weapon)
	{
		Weapon = SwingCtx.Weapon;
	}

	if (Weapon)
	{
		if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
		{
			Weapon->Multicast_StopWeaponMontage(0.f);
		}

		if (USkeletalMeshComponent* Mesh = Weapon->GetWeaponMesh())
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance())
			{
				Anim->Montage_Stop(0.f);
			}
		}
	}

	if (HeroMeleeMontageTask)
	{
		HeroMeleeMontageTask->EndTask();
		HeroMeleeMontageTask = nullptr;
	}

	if (WeaponMeleeMontageTask)
	{
		WeaponMeleeMontageTask->EndTask();
		WeaponMeleeMontageTask = nullptr;
	}
}

void UGA_Melee::OpenComboInput()
{
	if (bFinishingMelee)
	{
		return;
	}

	AAscenduntWeapon* Weapon = CachedWeapon.Get();
	bComboInputOpen = HasNextComboStep(Weapon);
}

void UGA_Melee::CloseComboInput()
{
	// CloseTime이 지나면 더 이상 다음 공격 입력을 받지 않고 콤보 상태를 초기화한다.
	bComboInputOpen = false;
	FinishMeleeAbility(false);
}

void UGA_Melee::TryAdvanceComboStepFromInput()
{
	AAscenduntWeapon* Weapon = CachedWeapon.Get();
	if (!Weapon || !HasNextComboStep(Weapon))
	{
		return;
	}

	// 공격 입력이 유효한 순간 바로 다음 Step으로 넘어간다.
	// CloseTime까지 입력을 예약해두지 않는다.
	const int32 NextStepIndex = CurrentStepIndex + 1;
	StartComboStep(NextStepIndex, /*bInitialStep*/false);
}

void UGA_Melee::FinishMeleeAbility(bool bWasCancelled)
{
	if (bFinishingMelee)
	{
		return;
	}

	bFinishingMelee = true;
	bStopMontagesOnEnd = bWasCancelled;
	bComboInputOpen = false;
	CurrentStepIndex = 0;
	AttackIndex = 0;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

void UGA_Melee::BindWeaponHitWindow()
{
	if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = nullptr;
	UAscenduntAbilitySystemComponent* AscASC = nullptr;
	AAscenduntWeapon* Weapon = nullptr;

	if (!GetASCAndWeapon(ASC, AscASC, Weapon) || !Weapon)
	{
		return;
	}

	CachedWeapon = Weapon;

	// 서버 인스턴스에서만 바인딩(판정은 서버가 수행)
	if (CurrentActorInfo->IsNetAuthority())
	{
		HitWindowHandle = Weapon->OnMeleeHitWindow.AddUObject(this, &ThisClass::OnWeaponHitWindow);

		// 레이스 방지: 이미 HitWindow가 열려있으면 즉시 시작
		bool bActive = false;
		int32 StepIdx = 0;
		Weapon->GetServerMeleeWindowState(bActive, StepIdx);
		if (bActive)
		{
			OnWeaponHitWindow(true, StepIdx);
		}
	}
}

void UGA_Melee::UnbindWeaponHitWindow()
{
	AAscenduntWeapon* Weapon = CachedWeapon.Get();
	if (Weapon && HitWindowHandle.IsValid())
	{
		Weapon->OnMeleeHitWindow.Remove(HitWindowHandle);
	}
	HitWindowHandle.Reset();
	CachedWeapon.Reset();
}

void UGA_Melee::OnWeaponHitWindow(bool bBegin, int32 NotifyStepIndex)
{
	// 서버에서만 의미 있음
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

#if !(UE_BUILD_SHIPPING)
	if (NotifyStepIndex != CurrentStepIndex)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MeleeHitWindow StepIndex mismatch. Notify=%d, Ability=%d. Ability index will be used for Begin, but End will be paired by NotifyStepIndex."),
			NotifyStepIndex,
			CurrentStepIndex);
	}
#endif

	if (bBegin)
	{
		// 이전 윈도우가 남아 있으면 명시적으로 닫는다.
		// 콤보 전환 / Notify 순서 꼬임에서 HitTick이 중첩되는 것을 막는다.
		if (bServerHitWindowActive)
		{
			const int32 EndStepIndex = (ActiveHitWindowAbilityStepIndex != INDEX_NONE)
				? ActiveHitWindowAbilityStepIndex
				: CurrentStepIndex;
			OnMeleeHitWindowEnded(EndStepIndex);
			StopServerHitTick();
		}

		ActiveHitWindowNotifyStepIndex = NotifyStepIndex;
		ActiveHitWindowAbilityStepIndex = CurrentStepIndex;

		OnMeleeHitWindowStarted(CurrentStepIndex);
		bServerHitWindowActive = true;
		HitActorsThisWindow.Reset();
		StartServerHitTick();
		return;
	}

	if (!bServerHitWindowActive)
	{
		return;
	}

	// Begin과 pair가 맞지 않는 늦은 NotifyEnd는 현재 Step의 HitWindow를 닫으면 안 된다.
	if (ActiveHitWindowNotifyStepIndex != INDEX_NONE && NotifyStepIndex != ActiveHitWindowNotifyStepIndex)
	{
#if !(UE_BUILD_SHIPPING)
		UE_LOG(LogTemp, Warning,
			TEXT("Ignoring stale MeleeHitWindow End. Notify=%d, ActiveNotify=%d, ActiveAbilityStep=%d, CurrentAbilityStep=%d"),
			NotifyStepIndex,
			ActiveHitWindowNotifyStepIndex,
			ActiveHitWindowAbilityStepIndex,
			CurrentStepIndex);
#endif
		return;
	}

	const int32 EndStepIndex = (ActiveHitWindowAbilityStepIndex != INDEX_NONE)
		? ActiveHitWindowAbilityStepIndex
		: CurrentStepIndex;

	OnMeleeHitWindowEnded(EndStepIndex);
	bServerHitWindowActive = false;
	ActiveHitWindowNotifyStepIndex = INDEX_NONE;
	ActiveHitWindowAbilityStepIndex = INDEX_NONE;
	StopServerHitTick();
}

void UGA_Melee::StartServerHitTick()
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	AAscenduntWeapon* Weapon = CachedWeapon.Get();
	const FAscenduntMeleeStep* Step = nullptr;
	if (!Weapon || !GetMeleeStep(Weapon, CurrentStepIndex, Step) || !Step)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	StopServerHitTick(); // 중복 방지

	const float Interval = Step->TraceProfile.HitTickInterval;
	if (Interval <= 0.f)
	{
		ServerHitTick(); // 1회만
		return;
	}

	// 즉시 1회 + 반복
	ServerHitTick();
	World->GetTimerManager().SetTimer(ServerHitTickTimer, this, &ThisClass::ServerHitTick, FMath::Max(0.005f, Interval), true);
}

void UGA_Melee::StopServerHitTick()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServerHitTickTimer);
	}
}

void UGA_Melee::ServerHitTick()
{
	if (!bServerHitWindowActive)
	{
		return;
	}

	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	ACharacter* Char = GetCharacterFromActorInfo();
	AAscenduntWeapon* Weapon = CachedWeapon.Get();
	if (!World || !Char || !Weapon)
	{
		return;
	}

	const FAscenduntMeleeStep* Step = nullptr;
	if (!GetMeleeStep(Weapon, CurrentStepIndex, Step) || !Step)
	{
		return;
	}

	const FAscenduntWeaponModeStats& ModeStats = Weapon->GetCurrentModeStats();
	const FAscenduntMeleeTraceProfile& P = Step->TraceProfile;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AscWeaponTrace), /*bTraceComplex*/false);
	Params.bFindInitialOverlaps = true;
	Params.bIgnoreTouches = false;
	AscMeleeInternal::AddIgnoreWeaponAndOwner(Params, Char, Weapon);

	TArray<FHitResult> Hits;

	auto ApplyHits = [&](const FVector& TraceStartForContext)
		{
			const AActor* SourceAvatar = CurrentActorInfo->AvatarActor.Get();
			const bool bCanHeal = (Step->bCanHealMelee) && (HealEffectClass != nullptr);

			for (const FHitResult& H : Hits)
			{
				AActor* HitActor = H.GetActor();
				if (!HitActor || HitActor == Char)
				{
					continue;
				}

				UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
				if (!TargetASC)
				{
					continue;
				}

				if (HitActorsThisWindow.Contains(HitActor))
				{
					continue;
				}

				const bool bFriendly = AscMeleeInternal::IsFriendly(SourceAvatar, HitActor);

				FAscShotImpactDecision Decision;
				if (bFriendly)
				{
					if (!bCanHeal) continue;
					if (AscMeleeInternal::IsFullHealth(HitActor)) continue;

					Decision.Kind = EAscShotImpactKind::HealTarget;
				}
				else
				{
					Decision.Kind = EAscShotImpactKind::DamageTarget;
				}

				Decision.Hit = H;
				Decision.TargetASC = TargetASC;

				HitActorsThisWindow.Add(HitActor);

				// 스텝별 DamageCoef / DamageOverride 반영.
				// BaseDamage 자체는 ApplyImpactDecision_Server에서 현재 Weapon ModeStat을 GE Spec에 주입한다.
				FAscenduntWeaponAttackContext LocalCtx = SwingCtx;
				LocalCtx.DamageCoef *= Step->DamageCoef;
				LocalCtx.DamageOverride = Step->DamageOverride;

				ApplyImpactDecision_Server(LocalCtx, Decision, TraceStartForContext);

				if (Decision.Kind == EAscShotImpactKind::DamageTarget)
				{
					OnServerMeleeDamageHitConfirmed(HitActor, H, LocalCtx, *Step, CurrentStepIndex);
				}
			}
		};

	const bool bDebug = (CVarAscDebugMelee.GetValueOnGameThread() != 0)
		|| ModeStats.bDrawDebugMeleeHitBox
		|| P.bDrawDebugHitBox;
	const float DebugDrawDuration = ModeStats.bDrawDebugMeleeHitBox
		? ModeStats.MeleeDebugDrawDuration
		: P.DebugDrawDuration;
	const float DebugDrawThickness = ModeStats.bDrawDebugMeleeHitBox
		? ModeStats.MeleeDebugDrawThickness
		: P.DebugDrawThickness;

	// FixedForwardBox only: HitWindow 동안 캐릭터 전방에 고정 박스를 생성한다.
	const FVector Loc = Char->GetActorLocation();
	const FVector Fwd = Char->GetActorForwardVector();
	const FVector Right = Char->GetActorRightVector();
	const FVector Up = Char->GetActorUpVector();

	const FVector Center = Loc + Fwd * P.Fixed_LocalOffset.X + Right * P.Fixed_LocalOffset.Y + Up * P.Fixed_LocalOffset.Z;

	// 정지 상태에서도 sweep query가 안정적으로 overlap을 잡도록 아주 짧게 sweep한다.
	const float Eps = 2.0f;
	const FVector Start = Center - Fwd * Eps;
	const FVector End = Center + Fwd * Eps;

	Hits.Reset();
	World->SweepMultiByChannel(
		Hits,
		Start,
		End,
		Char->GetActorQuat(),
		TraceChannel,
		FCollisionShape::MakeBox(P.Fixed_BoxHalfExtent),
		Params
	);

	if (bDebug)
	{
		DrawDebugBox(World, Center, P.Fixed_BoxHalfExtent, Char->GetActorQuat(), FColor::Green, false, DebugDrawDuration, 0, DebugDrawThickness);
		DrawDebugLine(World, Start, End, FColor::Cyan, false, DebugDrawDuration, 0, DebugDrawThickness);
	}

	ApplyHits(/*TraceStart*/Loc);
}
