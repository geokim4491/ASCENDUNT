// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_BaseFire.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "ASCENDUNT/System/AscCollisionChannels.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Character/Hero/HeroFirstPersonComponent.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "ASCENDUNT/Weapon/AscWeaponAOEProviderInterface.h"
#include "ASCENDUNT/AbilitySystem/Utility/AscTargetUtil.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/AbilitySystem/AscenduntGameplayEffectContext.h"
#include "ASCENDUNT/AbilitySystem/Components/AscCombatContributionComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Character/HitCollision/AscHitCollisionComponent.h"

#include "ASCENDUNT/AbilitySystem/AbilityTask/AT_PlayWeaponMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

#include "GenericTeamAgentInterface.h"

#include "ASCENDUNT/Character/AscenduntHealthComponent.h"

// Projectile + Pool
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "ASCENDUNT/System/AscenduntProjectilePoolSubsystem.h"

UGA_BaseFire::UGA_BaseFire()
{
	ActivationGroup = EAscenduntAbilityActivationGroup::Independent;
	ActivationPolicy = EAscenduntAbilityActivationPolicy::WhileInputActive;

	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Weapon_Fire);
	ReloadAbilityTag = AscenduntGameplayTags::Ability_Weapon_Reload;

	FireCueTagReplicated = AscenduntGameplayTags::GameplayCue_Replicated_Weapon_Fire;
	FireCueTagPredicted = AscenduntGameplayTags::GameplayCue_Predicted_Weapon_Fire;
	TraceCueTagReplicated = AscenduntGameplayTags::GameplayCue_Replicated_Weapon_Trace;
	TraceCueTagPredicted = AscenduntGameplayTags::GameplayCue_Predicted_Weapon_Trace;
	ImpactWorldCueTagReplicated = AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactWorld;
	ImpactWorldCueTagPredicted = AscenduntGameplayTags::GameplayCue_Predicted_Weapon_ImpactWorld;

}

int32 UGA_BaseFire::GetAmmoCostPerTrigger(const UAbilitySystemComponent* ASC) const
{
	// "펠릿 수"와 탄 소모는 무관
	const float Cost = ASC ? ASC->GetNumericAttribute(UAscenduntWeaponSet::GetAmmoCostPerShotAttribute()) : 1.f;
	return FMath::Max(0, FMath::RoundToInt(Cost));
}

float UGA_BaseFire::GetShotIntervalSeconds(const UAbilitySystemComponent* ASC) const
{
	const float ROF = ASC ? ASC->GetNumericAttribute(UAscenduntWeaponSet::GetRateOfFireAttribute()) : 10.f; // shots/sec
	return 1.f / FMath::Max(0.1f, ROF);
}

float UGA_BaseFire::GetBaseSpreadDegSafe(const UAbilitySystemComponent* ASC) const
{
	return ASC ? ASC->GetNumericAttribute(UAscenduntWeaponSet::GetBaseSpreadDegAttribute()) : 0.f;
}

int32 UGA_BaseFire::GetProjectilesPerTrigger(const UAbilitySystemComponent* ASC) const
{
	const float Shots = ASC
		? ASC->GetNumericAttribute(UAscenduntWeaponSet::GetShotsPerTriggerAttribute())
		: 1.f;

	return FMath::Max(1, FMath::RoundToInt(Shots));
}

bool UGA_BaseFire::BuildViewRayAndMuzzle(const FGameplayAbilityActorInfo* ActorInfo, AAscenduntWeapon* Weapon, FVector& OutViewStart, FVector& OutViewDir, FVector& OutMuzzleLoc) const
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar) return false;

	FVector ViewLoc;
	FRotator ViewRot;

	if (bUseControllerView && ActorInfo->PlayerController.IsValid())
	{
		ActorInfo->PlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);
	}
	else
	{
		ViewLoc = Avatar->GetActorLocation();
		ViewRot = Avatar->GetActorRotation();
	}

	OutViewStart = ViewLoc;
	OutViewDir = ViewRot.Vector().GetSafeNormal();

	// ---- Gameplay Muzzle location ----
	// 투사체 스폰/판정 기준은 항상 3P WeaponMesh 기준을 유지한다.
	// 로컬 1P VFX 시작점은 PostFireOnce / Trace GameplayCue에서 별도로 계산한다.
	if (Weapon)
	{
		OutMuzzleLoc = Weapon->GetMuzzleTransform().GetLocation();
	}
	else
	{
		OutMuzzleLoc = OutViewStart; // fallback
	}

	return true;
}

FVector UGA_BaseFire::ApplySpreadDeterministic(const FVector& BaseDir, float SpreadDeg, int32 Seed) const
{
	const float HalfAngleRad = FMath::DegreesToRadians(FMath::Max(0.f, SpreadDeg) * 0.5f);

	if (HalfAngleRad <= 0.f)
	{
		return BaseDir.GetSafeNormal();
	}

	FRandomStream Stream(Seed);
	return Stream.VRandCone(BaseDir.GetSafeNormal(), HalfAngleRad);
}

bool UGA_BaseFire::FireOnce_Trigger(float DamageCoef, int32 ShotIndex, FAscenduntFireOnceContext& OutCtx, bool bDoCommit)
{
	UAbilitySystemComponent* ASC = nullptr;
	UAscenduntAbilitySystemComponent* AscASC = nullptr;
	AAscenduntWeapon* Weapon = nullptr;

	// ========= 발사 가능 여부 체크 =========
	if (!GetASCAndWeapon(ASC, AscASC, Weapon))
	{
		return false;
	}

	const int32 AmmoCost = GetAmmoCostPerTrigger(ASC);

	// 클라/서버 공통으로 탄 체크 먼저 -> "탄 0인데 클라 예측 발사" 방지
	if (Weapon && !Weapon->CanFire(AmmoCost))
	{
		return false;
	}

	// 발사 간격 체크 (클라 예측도 막아서 "헛발사 VFX" 방지) => GAS의 CommitAbility에서 쿨타운 체크하는 걸로 리팩토링???????????????
	UWorld* World = CurrentActorInfo->AvatarActor->GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float Interval = GetShotIntervalSeconds(ASC);
	const bool bAuth = CurrentActorInfo->IsNetAuthority();

	if (Weapon && !Weapon->CanFireByTime(/*bAuthority*/bAuth, Now))
	{
		return false;
	}
	// ========= 발사 처리 =========
	//
	// Commit은 “발사 1회 단위”로 처리 (Auto는 루프마다 호출)
	// Charge는 파생에서 “Release 시점에만 Commit”하고 싶으면, 파생에서 Commit 후 이 함수를 호출해도 됨.
	if (bDoCommit)
	{
		if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
			return false;
	}

	OutCtx.ASC = ASC;
	OutCtx.AscASC = AscASC;
	OutCtx.Weapon = Weapon;
	OutCtx.AmmoCost = AmmoCost;
	OutCtx.Now = Now;
	OutCtx.Interval = Interval;
	OutCtx.bAuthority = bAuth;
	OutCtx.DamageCoef = DamageCoef;
	OutCtx.ShotIndex = ShotIndex;

	if (const IAscWeaponAOEProviderInterface* AOEProvider = Cast<IAscWeaponAOEProviderInterface>(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr))
	{
		float Radius = 0.f;
		float DamageCoefficient = 1.f;
		bool bDebugDrawRadius = false;
		float DebugDrawDuration = 0.f;
		if (AOEProvider->GetWeaponAOEParameters(Radius, DamageCoefficient, bDebugDrawRadius, DebugDrawDuration) && Radius > KINDA_SMALL_NUMBER)
		{
			OutCtx.bAOEImpact = true;
			OutCtx.AOERadius = Radius;
			OutCtx.AOEDamageCoefficient = FMath::Max(0.f, DamageCoefficient);
			OutCtx.bDebugDrawAOERadius = bDebugDrawRadius;
			OutCtx.AOEDebugDrawDuration = FMath::Max(0.f, DebugDrawDuration);
		}
	}

	return true;
}

void UGA_BaseFire::PostFireOnce(const FAscenduntFireOnceContext& Ctx, const FVector& Start, const FVector& EndOrImpact, bool bHasAnyHit, const FHitResult* Hit)
{
	// --- Fire Montage ---
	if (Ctx.Weapon)
	{
		const FAscenduntWeaponModeStats& S = Ctx.Weapon->GetCurrentModeStats();

		const float Desired = FMath::Max(0.01f, Ctx.Interval);

		// 발사 몽타주 재생 속도.
		//
		// 기본은 "몽타주 길이 / 발사 간격" 이다. 연사 중에 몽타주가 정확히 한 발
		// 간격을 채우도록 늘리거나 줄이는 계산이고, 몽타주 길이가 발사 간격과
		// 비슷한 무기에서는 이게 맞다.
		//
		// 다만 발사 간격이 몽타주보다 훨씬 긴 무기(곡사포 등)는 이 계산이
		// 슬로모션을 만든다. 그런 무기는 ModeStats 에서 고정 배속을 지정한다.
		// 세 몽타주(3P / 무기 / 1P)에 같은 값을 써야 메시끼리 어긋나지 않는다.
		const auto ResolveFireMontageRate = [&S, Desired](const UAnimMontage* Montage) -> float
		{
			if (S.FireMontagePlayRateOverride > 0.f)
			{
				return S.FireMontagePlayRateOverride;
			}

			return Montage ? float(Montage->GetPlayLength() / Desired) : 1.f;
		};

		ABaseAICharacter* AIAvatar = Cast<ABaseAICharacter>(GetAvatarActorFromActorInfo());
		const bool bServerAI = AIAvatar && Ctx.bAuthority;

		// 캐릭터 몽타주: (예측) 로컬 + (권한) 서버에서 실행
		// AI Dedicated Server는 Pose를 계산하지 않는다. AI는 Multicast로 클라이언트 연출만 재생한다.
		if (S.HeroFireMontage && bServerAI)
		{
			AIAvatar->Multicast_PlayPresentationMontage(
				S.HeroFireMontage,
				ResolveFireMontageRate(S.HeroFireMontage),
				NAME_None);
		}
		else if (S.HeroFireMontage && (CurrentActorInfo->IsLocallyControlled() || Ctx.bAuthority))
		{
			if (HeroMontageTask)
			{
				HeroMontageTask->EndTask();
				HeroMontageTask = nullptr;
			}

			HeroMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, S.HeroFireMontage, ResolveFireMontageRate(S.HeroFireMontage)
				, NAME_None, /*bStopWhenAbilityEnds*/false);

			if (HeroMontageTask)
			{
				HeroMontageTask->ReadyForActivation();
			}
		}

		// 무기 몽타주: 로컬은 예측으로 즉시, 서버는 NetMulticast로 타 클라에게 보이게
		if (S.WeaponFireMontage && bServerAI)
		{
			// AI 무기 역시 서버 AnimInstance를 돌리지 않고 클라이언트에 연출만 전파한다.
			Ctx.Weapon->Multicast_PlayWeaponMontage(
				S.WeaponFireMontage,
				ResolveFireMontageRate(S.WeaponFireMontage));
		}
		else if (S.WeaponFireMontage && (CurrentActorInfo->IsLocallyControlled() || Ctx.bAuthority))
		{
			if (WeaponMontageTask)
			{
				WeaponMontageTask->EndTask();
				WeaponMontageTask = nullptr;
			}

			const bool bReplicateToOthers = Ctx.bAuthority; // 서버만 뿌림(오너는 클라 예측으로 이미 봄)
			WeaponMontageTask = UAT_PlayWeaponMontageAndWait::PlayWeaponMontageAndWait(
				this, NAME_None, Ctx.Weapon, S.WeaponFireMontage, ResolveFireMontageRate(S.WeaponFireMontage),
				bReplicateToOthers, /*bStopWhenAbilityEnds*/false);

			if (WeaponMontageTask)
			{
				WeaponMontageTask->ReadyForActivation();
			}
		}

		// 1인칭 몽타주: 로컬 오너만 보므로 복제하지 않는다. GAS 태스크 대신 직접 재생한다.
		// 위의 3P 재생은 그대로 둔다. 3P 메시가 1인칭에서도 그림자를 만들기 때문에
		// 여기서 3P 를 멈추면 그림자만 가만히 서 있게 된다.
		if (S.HeroFireMontage1P && CurrentActorInfo->IsLocallyControlled())
		{
			if (UHeroFirstPersonComponent* FirstPerson =
				UHeroFirstPersonComponent::FindFirstPersonComponent(GetAvatarActorFromActorInfo()))
			{
				// 배율 계산식은 3P(HeroFireMontage)와 반드시 같아야 두 메시가 어긋나지 않는다.
				FirstPerson->PlayMontage1P(S.HeroFireMontage1P,
					ResolveFireMontageRate(S.HeroFireMontage1P));
			}
		}

		// 1인칭 자세 전환: 달리는 중이면 SprintPose 가 적용 중인데, 쏘는 순간 총이 올라와야 한다.
		// ReadyHoldAfterFire 동안 ReadyPose 를 강제 유지시키고 반동도 함께 넣는다.
		// 1P 몽타주 유무와 무관하게 필요하므로 위 블록 밖에서 호출한다.
		if (CurrentActorInfo->IsLocallyControlled())
		{
			if (UHeroFirstPersonComponent* FirstPerson =
				UHeroFirstPersonComponent::FindFirstPersonComponent(GetAvatarActorFromActorInfo()))
			{
				FirstPerson->NotifyFired1P();
			}
		}
	}

	// --- FireCue (trigger당 1회) ---
	const FHitResult* HitResult = (bHasAnyHit && Hit) ? Hit : nullptr;
	if (CurrentActorInfo->IsLocallyControlled())
	{
		// 로컬 예측 Muzzle VFX는 1P Weapon이 보이는 경우 그 Muzzle을 기준으로 한다.
		// 서버 확정 Start는 아래 Replicated Cue에 그대로 사용하므로 실제 발사 판정과 분리된다.
		const FVector LocalVFXStart = Ctx.Weapon
			? Ctx.Weapon->GetMuzzleVFXTransform().GetLocation()
			: Start;
		PlayFireCue(Ctx.AscASC, LocalVFXStart, EndOrImpact, HitResult, EAscenduntCueRoute::LocalOnly, false);
	}

	if (Ctx.bAuthority)
	{
		// 탄 소모는 trigger당 1회
		if (Ctx.Weapon)
		{
			Ctx.Weapon->ConsumeAmmo_Server(Ctx.AmmoCost);
		}

		PlayFireCue(Ctx.AscASC, Start, EndOrImpact, HitResult, EAscenduntCueRoute::ReplicatedToRelevant, true);
	}

	if (Ctx.Weapon)
	{
		if (Ctx.bAuthority)
		{
			Ctx.Weapon->MarkFired(Ctx.bAuthority, Ctx.Now, Ctx.Interval);
		}
		else if (CurrentActorInfo->IsLocallyControlled())
		{
			Ctx.Weapon->MarkFired(false, Ctx.Now, Ctx.Interval);
		}
	}

	if (Ctx.bAuthority && Ctx.Weapon->GetCurrentAmmoInMag() <= 0 && !Ctx.Weapon->IsInfiniteMag())
	{
		TryAutoReload(Ctx.ASC, Ctx.Weapon, /*bEndThisAbilityBeforeReload=*/true);
	}
}

void UGA_BaseFire::PlayFireCue(UAscenduntAbilitySystemComponent* InstigatorASC, const FVector& Start, const FVector& EndOrImpact, const FHitResult* Hit, EAscenduntCueRoute Route, bool bIsReplicated) const
{
	FGameplayCueParameters P;
	P.Instigator = InstigatorASC ? InstigatorASC->GetAvatarActor() : nullptr;
	P.EffectCauser = Cast<AActor>(GetCurrentSourceObject());
	P.SourceObject = GetCurrentSourceObject();
	P.Location = EndOrImpact;

	FGameplayEffectContextHandle Ctx = InstigatorASC->MakeEffectContext();
	Ctx.AddSourceObject(GetCurrentSourceObject());
	Ctx.AddOrigin(Start);
	if (Hit)
	{
		Ctx.AddHitResult(*Hit, true);
	}
	P.EffectContext = Ctx;

	if (bIsReplicated)
	{
		UAscenduntAbilitySystemComponent::AscRouteCue(
			InstigatorASC, InstigatorASC, FireCueTagReplicated, P, Route);
	}
	else
	{
		UAscenduntAbilitySystemComponent::AscRouteCue(
			InstigatorASC, InstigatorASC, FireCueTagPredicted, P, Route);
	}

}

void UGA_BaseFire::PlayTraceCue(UAscenduntAbilitySystemComponent* InstigatorASC, const FVector& Start, const FVector& End, EAscenduntCueRoute Route, bool bIsReplicated) const
{
	if (!InstigatorASC)
	{
		return;
	}

	FGameplayCueParameters P;
	P.Instigator = InstigatorASC->GetAvatarActor();
	P.EffectCauser = Cast<AActor>(GetCurrentSourceObject());
	P.SourceObject = GetCurrentSourceObject();
	P.Location = End;

	FGameplayEffectContextHandle CueContext = InstigatorASC->MakeEffectContext();
	CueContext.AddSourceObject(GetCurrentSourceObject());
	CueContext.AddOrigin(Start);
	P.EffectContext = CueContext;

	const FGameplayTag& CueTag = bIsReplicated ? TraceCueTagReplicated : TraceCueTagPredicted;
	if (!CueTag.IsValid())
	{
		return;
	}

	UAscenduntAbilitySystemComponent::AscRouteCue(
		InstigatorASC, InstigatorASC, CueTag, P, Route);
}

void UGA_BaseFire::PlayImpactWorldCue(UAscenduntAbilitySystemComponent* InstigatorASC, const FHitResult& Hit, EAscenduntCueRoute Route, bool bIsReplicated, bool bAOEImpact) const
{
	FGameplayCueParameters P;
	P.Instigator = InstigatorASC ? InstigatorASC->GetAvatarActor() : nullptr;
	P.EffectCauser = Cast<AActor>(GetCurrentSourceObject());
	P.SourceObject = GetCurrentSourceObject();
	P.Location = Hit.ImpactPoint;
	P.Normal = Hit.ImpactNormal;
	P.PhysicalMaterial = Hit.PhysMaterial.Get();
	if (bAOEImpact)
	{
		P.AggregatedSourceTags.AddTag(AscenduntGameplayTags::Effect_Weapon_AOEImpact);
	}

	FGameplayEffectContextHandle Ctx = InstigatorASC->MakeEffectContext();
	Ctx.AddSourceObject(GetCurrentSourceObject());
	Ctx.AddOrigin(Hit.TraceStart);
	Ctx.AddHitResult(Hit, true);
	P.EffectContext = Ctx;

	if (bIsReplicated)
	{
		UAscenduntAbilitySystemComponent::AscRouteCue(
			InstigatorASC, InstigatorASC, ImpactWorldCueTagReplicated, P, Route);
	}
	else
	{
		UAscenduntAbilitySystemComponent::AscRouteCue(
			InstigatorASC, InstigatorASC, ImpactWorldCueTagPredicted, P, Route);
	}
}

namespace AscFireInternal
{
	static UAscCombatContributionComponent* ResolveCombatContribution(UAbilitySystemComponent* SourceASC)
	{
		if (!SourceASC)
		{
			return nullptr;
		}

		AAscenduntPlayerState* PlayerState = Cast<AAscenduntPlayerState>(SourceASC->GetOwnerActor());
		return PlayerState ? PlayerState->GetCombatContributionComponent() : nullptr;
	}

	static bool IsCriticalWeaponHit(UAbilitySystemComponent* SourceASC, const FAscShotImpactDecision& Decision)
	{
		if (!SourceASC || Decision.Kind != EAscShotImpactKind::DamageTarget)
		{
			return false;
		}

		const UAscHitCollisionComponent* HitCollision = Cast<UAscHitCollisionComponent>(Decision.Hit.GetComponent());
		return HitCollision && HitCollision->IsHeadZone() &&
			SourceASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_AllowHeadshot);
	}

	static bool IsFriendly(const AActor* A, const AActor* B)
	{
		if (!A || !B)
		{
			return false;
		}

		const IGenericTeamAgentInterface* TA = Cast<IGenericTeamAgentInterface>(A);
		const IGenericTeamAgentInterface* TB = Cast<IGenericTeamAgentInterface>(B);
		if (!TA || !TB)
		{
			return false;
		}

		return TA->GetGenericTeamId() == TB->GetGenericTeamId();
	}

	static bool IsFullHealth(const AActor* Actor)
	{
		const UAscenduntHealthComponent* HC = UAscenduntHealthComponent::FindHealthComponent(Actor);
		if (!HC || !HC->HasValidHealthSet())
		{
			// If we can't determine health, treat as NOT full so healing can still happen.
			return false;
		}
		const float H = HC->GetHealth();
		const float M = HC->GetMaxHealth();
		return (M > 0.f) && (H >= M - KINDA_SMALL_NUMBER);
	}

	static FVector CalcHitPointOnRay(const FVector& Start, const FVector& End, const FHitResult& Hit)
	{
		if (Hit.bBlockingHit)
		{
			return Hit.ImpactPoint;
		}
		// For non-blocking hits (Overlap/Touch), ImpactPoint/Location can be unreliable.
		if (Hit.Time > 0.f && Hit.Time < 1.f)
		{
			return FMath::Lerp(Start, End, Hit.Time);
		}
		if (!Hit.Location.IsNearlyZero())
		{
			return Hit.Location;
		}
		return Hit.ImpactPoint;
	}
}

bool UGA_BaseFire::MakeShotImpactDecision(const FAscenduntFireOnceContext& Ctx, const FVector& Start, const FVector& End, FAscShotImpactDecision& OutDecision) const
{
	OutDecision = FAscShotImpactDecision();

	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AscWeaponTrace), /*bTraceComplex=*/false);
	Params.bReturnPhysicalMaterial = true;

	if (Avatar) Params.AddIgnoredActor(Avatar);
	if (AActor* WeaponActor = Cast<AActor>(GetCurrentSourceObject())) Params.AddIgnoredActor(WeaponActor);

	TArray<FHitResult> Hits;
	World->LineTraceMultiByChannel(Hits, Start, End, TraceChannel, Params);
	if (Hits.Num() == 0)
	{
		return false;
	}

	Hits.Sort([](const FHitResult& A, const FHitResult& B)
		{
			return A.Distance < B.Distance;
		});

	const AActor* SourceAvatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;

	const FAscenduntWeaponModeStats* ModeStats = (Ctx.Weapon ? &Ctx.Weapon->GetCurrentModeStats() : nullptr);
	const bool bCanHeal = (ModeStats ? ModeStats->bCanHeal : false) && HealEffectClass != nullptr;

	for (const FHitResult& H : Hits)
	{
		AActor* HitActor = H.GetActor();
		if (!HitActor)
		{
			continue;
		}

		// 1) ASC actor? -> choose first valid target by rules
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor))
		{
			const bool bFriendly = AscFireInternal::IsFriendly(SourceAvatar, HitActor);
			if (bFriendly)
			{
				// Damage-only shots: pass through friendlies
				if (!bCanHeal)
				{
					continue;
				}

				// Heal shots: heal first ally that is NOT full health, otherwise pass through
				if (AscFireInternal::IsFullHealth(HitActor))
				{
					continue;
				}

				OutDecision.Kind = EAscShotImpactKind::HealTarget;
				OutDecision.Hit = H;
				OutDecision.TargetASC = TargetASC;
				return true;
			}
			else
			{
				OutDecision.Kind = EAscShotImpactKind::DamageTarget;
				OutDecision.Hit = H;
				OutDecision.TargetASC = TargetASC;
				return true;
			}
		}

		// 2) Non-ASC blocking hit -> world stop
		if (H.bBlockingHit)
		{
			OutDecision.Kind = EAscShotImpactKind::WorldBlock;
			OutDecision.Hit = H;
			return true;
		}
	}

	return false;
}

bool UGA_BaseFire::FireDelivery_Hitscan(const FAscenduntFireOnceContext& Ctx)
{
	const float MaxRange = Ctx.Weapon ? Ctx.Weapon->GetMaxRange() : 10000.f;

	FVector TraceStart, ViewDir, FireCueStart;
	if (!BuildViewRayAndMuzzle(CurrentActorInfo, Ctx.Weapon, TraceStart, ViewDir, FireCueStart))
	{
		return false;
	}

	const int32 PelletCount = FMath::Max(1, GetProjectilesPerTrigger(Ctx.ASC));
	const float SpreadDeg = GetBaseSpreadDegSafe(Ctx.ASC);

	if (Ctx.bAuthority)
	{
		if (UAscCombatContributionComponent* Contribution = AscFireInternal::ResolveCombatContribution(Ctx.ASC))
		{
			Contribution->RecordWeaponShotsFired(PelletCount);
		}
	}
	const bool bHasTraceVFX = Ctx.Weapon && Ctx.Weapon->GetCurrentModeStats().FireTraceVFXNiagara != nullptr;

	FVector FireCueEnd = FireCueStart + ViewDir * MaxRange;
	FHitResult FireCueHit;
	bool bHasAnyImpact = false;

	for (int32 i = 0; i < PelletCount; ++i)
	{
		const int32 Seed = (int32)CurrentActivationInfo.GetActivationPredictionKey().Current + (Ctx.ShotIndex * 101) + i;
		const FVector Dir = ApplySpreadDeterministic(ViewDir, SpreadDeg, Seed);
		const FVector End = TraceStart + Dir * MaxRange;

		FAscShotImpactDecision Decision;
		const bool bHasDecision = MakeShotImpactDecision(Ctx, TraceStart, End, Decision);

		const FVector EndOrImpact = bHasDecision ? AscFireInternal::CalcHitPointOnRay(TraceStart, End, Decision.Hit) : End;

		// Hitscan Trail은 Pellet 단위로 실행한다. FireCue는 아래 PostFireOnce에서 Trigger당 1회만 실행한다.
		if (bHasTraceVFX && CurrentActorInfo->IsLocallyControlled())
		{
			PlayTraceCue(Ctx.AscASC, FireCueStart, EndOrImpact, EAscenduntCueRoute::LocalOnly, false);
		}

		if (bHasTraceVFX && Ctx.bAuthority)
		{
			PlayTraceCue(Ctx.AscASC, FireCueStart, EndOrImpact, EAscenduntCueRoute::ReplicatedToRelevant, true);
		}

		// FireCue context에는 첫 Pellet의 결과를 유지한다. (Trail 재생에는 사용하지 않음)
		if (i == 0)
		{
			FireCueEnd = EndOrImpact;
			if (bHasDecision)
			{
				bHasAnyImpact = true;
				FireCueHit = Decision.Hit;
			}
		}
		else if (!bHasAnyImpact && bHasDecision)
		{
			// Optional: if first pellet missed but later one hit, use that hit for fire cue context
			bHasAnyImpact = true;
			FireCueHit = Decision.Hit;
		}

		// 로컬 predicted: 월드 임팩트 cue(펠릿별)
		if (CurrentActorInfo->IsLocallyControlled() && bHasDecision && Decision.Kind == EAscShotImpactKind::WorldBlock)
		{
			PlayImpactWorldCue(Ctx.AscASC, Decision.Hit, EAscenduntCueRoute::LocalOnly, false, Ctx.bAOEImpact);
		}

		// 서버 authoritative
		if (Ctx.bAuthority && bHasDecision)
		{
			if (Decision.Kind == EAscShotImpactKind::WorldBlock)
			{
				PlayImpactWorldCue(Ctx.AscASC, Decision.Hit, EAscenduntCueRoute::ReplicatedToRelevant, true, Ctx.bAOEImpact);
				ApplyAOEDamage_Server(Ctx, Decision.Hit.ImpactPoint, nullptr);
			}
			else
			{
				if (UAscCombatContributionComponent* Contribution = AscFireInternal::ResolveCombatContribution(Ctx.ASC))
				{
					Contribution->RecordWeaponHit(AscFireInternal::IsCriticalWeaponHit(Ctx.ASC, Decision));
				}

				ApplyImpactDecision_Server(Ctx, Decision, TraceStart);
				if (Decision.Kind == EAscShotImpactKind::DamageTarget)
				{
					AActor* DirectTarget = Decision.TargetASC.IsValid() ? Decision.TargetASC->GetAvatarActor_Direct() : nullptr;
					ApplyAOEDamage_Server(Ctx, Decision.Hit.ImpactPoint, DirectTarget);
				}
			}
		}
	}

	PostFireOnce(Ctx, FireCueStart, FireCueEnd, bHasAnyImpact, bHasAnyImpact ? &FireCueHit : nullptr);
	return true;
}

FVector UGA_BaseFire::ComputeProjectileLaunchDir(const FAscenduntFireOnceContext& Ctx,
	const FVector& MuzzleLoc, const FVector& AimPoint) const
{
	return (AimPoint - MuzzleLoc).GetSafeNormal();
}

void UGA_BaseFire::ApplyAOEDamage_Server(const FAscenduntFireOnceContext& Ctx, const FVector& ImpactPoint, AActor* DirectTarget) const
{
	if (!Ctx.bAuthority || !Ctx.bAOEImpact || Ctx.AOERadius <= KINDA_SMALL_NUMBER || !Ctx.ASC || !Ctx.Weapon || !DamageEffectClass)
	{
		return;
	}

	AActor* SourceActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = SourceActor ? SourceActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	if (Ctx.bDebugDrawAOERadius)
	{
		Ctx.Weapon->Multicast_DrawAOEDebugSphere(ImpactPoint, Ctx.AOERadius, Ctx.AOEDebugDrawDuration);
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AscWeaponAOE), false, SourceActor);

	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByObjectType(Overlaps, ImpactPoint, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Ctx.AOERadius), QueryParams))
	{
		return;
	}

	TSet<AActor*> AppliedActors;
	const float DirectDamageBase = Ctx.DamageOverride >= 0.f
		? Ctx.DamageOverride
		: Ctx.Weapon->GetCurrentModeStats().BaseDamage * Ctx.DamageCoef;
	const float SplashDamage = FMath::Max(0.f, DirectDamageBase * Ctx.AOEDamageCoefficient);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* TargetActor = Overlap.GetActor();
		if (!TargetActor || TargetActor == SourceActor || TargetActor == DirectTarget || AppliedActors.Contains(TargetActor))
		{
			continue;
		}
		AppliedActors.Add(TargetActor);

		if (!UAscTargetUtil::IsEnemyActor(SourceActor, TargetActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
		if (!TargetASC)
		{
			continue;
		}

		FHitResult SplashHit(TargetActor, nullptr, TargetActor->GetActorLocation(), FVector::UpVector);
		SplashHit.TraceStart = ImpactPoint;
		SplashHit.TraceEnd = TargetActor->GetActorLocation();

		FGameplayEffectContextHandle EffectCtx = Ctx.ASC->MakeEffectContext();
		EffectCtx.AddSourceObject(GetCurrentSourceObject());
		EffectCtx.AddOrigin(ImpactPoint);
		EffectCtx.AddHitResult(SplashHit, true);
		if (FAscenduntGameplayEffectContext* Typed = FAscenduntGameplayEffectContext::ExtractEffectContext(EffectCtx))
		{
			Typed->SetAbilitySource(Cast<IAscenduntAbilitySourceInterface>(GetCurrentSourceObject()), GetAbilityLevel());
		}

		FGameplayEffectSpecHandle Spec = Ctx.ASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectCtx);
		if (!Spec.IsValid())
		{
			continue;
		}
		Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageOverride, SplashDamage);
		Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Instances, 1.f);
		Spec.Data->DynamicAssetTags.AddTag(AscenduntGameplayTags::Effect_Weapon_AOESplash);
		Ctx.ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

void UGA_BaseFire::SpawnProjectile_Server(const FAscenduntFireOnceContext& Ctx, const FVector& SpawnLoc, const FVector& Dir) const
{
	check(Ctx.bAuthority);
	if (!Ctx.Weapon) return;

	const FAscenduntWeaponModeStats& S = Ctx.Weapon->GetCurrentModeStats();
	if (!S.ProjectileClass) return;

	UWorld* World = Ctx.Weapon->GetWorld();
	if (!World) return;

	AActor* OwnerActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	APawn* InstigatorPawn = Cast<APawn>(OwnerActor);

	const FTransform SpawnTM(Dir.Rotation(), SpawnLoc);

	AAscenduntProjectile* Proj = nullptr;
	UAscenduntProjectilePoolSubsystem* Pool = World->GetSubsystem<UAscenduntProjectilePoolSubsystem>();

	if (S.bUseProjectilePooling && Pool)
	{
		Proj = Pool->Acquire(S.ProjectileClass, SpawnTM, OwnerActor, InstigatorPawn, S.PoolSize);

		if (!Proj)
		{
			return;
		}
	}
	else
	{
		FActorSpawnParameters Params;
		Params.Owner = OwnerActor;
		Params.Instigator = InstigatorPawn;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Proj = World->SpawnActor<AAscenduntProjectile>(S.ProjectileClass, SpawnTM, Params);
	}

	if (!Proj) return;


	const float Speed = Ctx.AscASC->GetNumericAttribute(UAscenduntWeaponSet::GetProjectileSpeedAttribute());
	const float Scale = Ctx.AscASC->GetNumericAttribute(UAscenduntWeaponSet::GetProjectileScaleAttribute());
	const bool bCanBeNullified = Ctx.AscASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_ProjectileCanBeNullified);
	const float SpeedDone = (Speed > 0.f) ? Speed : DefaultProjectileSpeed;
	const float ScaleDone = (Scale > 0.f) ? Scale : DefaultProjectileScale;

	const float BaseDamage = S.BaseDamage;
	const float BaseHeal = S.BaseHeal;


	FAscenduntProjectileInitData Init;
	Init.LaunchDir = Dir;
	Init.Speed = SpeedDone;
	Init.Scale = ScaleDone;
	Init.bCanBeNullified = bCanBeNullified;
	Init.DamageCoef = Ctx.DamageCoef;
	Init.ActiveDuration = S.ProjectileActivateDuration;
	Init.Instances = 1.f;
	Init.bAOEImpact = Ctx.bAOEImpact;
	Init.AOERadius = Ctx.AOERadius;
	Init.AOEDamageCoefficient = Ctx.AOEDamageCoefficient;
	Init.bDebugDrawAOERadius = Ctx.bDebugDrawAOERadius;
	Init.AOEDebugDrawDuration = Ctx.AOEDebugDrawDuration;

	Proj->SetDamageContext(
		Ctx.ASC,
		GetCurrentSourceObject(),
		DamageEffectClass,
		HealEffectClass,
		S.bCanHeal,
		BaseDamage,
		BaseHeal,
		S.bProjectileReturnToPoolOnWorldHit
	);

	Proj->SetActorLocationAndRotation(SpawnLoc, Dir.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	Proj->ActivateFromGA(Init);
}

bool UGA_BaseFire::FireDelivery_Projectile(const FAscenduntFireOnceContext& Ctx)
{
	const float MaxRange = Ctx.Weapon ? Ctx.Weapon->GetMaxRange() : 10000.f;

	FVector ViewStart, ViewDir, MuzzleLoc;
	if (!BuildViewRayAndMuzzle(CurrentActorInfo, Ctx.Weapon, ViewStart, ViewDir, MuzzleLoc))
	{
		return false;
	}

	const FVector ViewEnd = ViewStart + ViewDir * MaxRange;

	// Aim decision uses the same rule table as hitscan
	FAscShotImpactDecision AimDecision;
	const bool bHasAimDecision = MakeShotImpactDecision(Ctx, ViewStart, ViewEnd, AimDecision);
	const FVector AimPoint = bHasAimDecision ? AscFireInternal::CalcHitPointOnRay(ViewStart, ViewEnd, AimDecision.Hit) : ViewEnd;

	const FVector BaseDir = ComputeProjectileLaunchDir(Ctx, MuzzleLoc, AimPoint);

	const int32 Count = FMath::Max(1, GetProjectilesPerTrigger(Ctx.ASC));
	const float SpreadDeg = GetBaseSpreadDegSafe(Ctx.ASC);

	if (Ctx.bAuthority)
	{
		if (UAscCombatContributionComponent* Contribution = AscFireInternal::ResolveCombatContribution(Ctx.ASC))
		{
			Contribution->RecordWeaponShotsFired(Count);
		}
	}

	// 서버: 스폰
	if (Ctx.bAuthority)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 Seed = (int32)CurrentActivationInfo.GetActivationPredictionKey().Current + (Ctx.ShotIndex * 101) + i;
			const FVector Dir = ApplySpreadDeterministic(BaseDir, SpreadDeg, Seed);
			SpawnProjectile_Server(Ctx, MuzzleLoc, Dir);
		}
	}

	// FireCue/탄소모/마킹
	PostFireOnce(Ctx, MuzzleLoc, AimPoint, bHasAimDecision, bHasAimDecision ? &AimDecision.Hit : nullptr);
	return true;
}

bool UGA_BaseFire::TryAutoReload(UAbilitySystemComponent* ASC, AAscenduntWeapon* Weapon, bool bEndThisAbilityBeforeReload)
{
	if (!ASC || !Weapon)
	{
		return false;
	}

	if (Weapon->IsInfiniteMag())
	{
		return false;
	}

	if (!CurrentActorInfo || !(CurrentActorInfo->IsLocallyControlled() || CurrentActorInfo->IsNetAuthority()))
	{
		return false;
	}

	// 이미 Reloading 상태면 중복 방지
	if (ASC->HasMatchingGameplayTag(AscenduntGameplayTags::Status_Weapon_Reloading) || Weapon->IsReloading())
	{
		return false;
	}

	bool bActivated = false;

	if (!bActivated && ReloadAbilityTag.IsValid())
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(ReloadAbilityTag);
		bActivated = ASC->TryActivateAbilitiesByTag(Tags, /*bAllowRemoteActivation=*/true);
	}

	return bActivated;
}
