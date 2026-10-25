#include "GA_BaseFire.h"
#include "ASCENDUNT/AbilitySystem/Effects/AscWeaponAmmoCost.h"
#include "GameplayPrediction.h"
// Fill out your copyright notice in the Description page of Project Settings.

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
	FireCueTag = AscenduntGameplayTags::GameplayCue_Weapon_Fire;
	TraceCueTag = AscenduntGameplayTags::GameplayCue_Weapon_Trace;
	WorldImpactCueTag = AscenduntGameplayTags::GameplayCue_Weapon_ImpactWorld;

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
	const float Shots = ASC ? ASC->GetNumericAttribute(UAscenduntWeaponSet::GetShotsPerTriggerAttribute()) : 1.f;

	return FMath::Max(1, FMath::RoundToInt(Shots));
}

bool UGA_BaseFire::BuildViewRayAndMuzzle(const FGameplayAbilityActorInfo* ActorInfo, AAscenduntWeapon* Weapon, FVector& OutViewStart, FVector& OutViewDir,
    FVector& OutMuzzleLoc) const
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return false;
	}

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
	// 로컬 1P VFX 시작점은 FinalizeShot / Trace GameplayCue에서 별도로 계산한다.
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

bool UGA_BaseFire::TryPrepareShot(float DamageCoef, int32 ShotIndex, FAscenduntWeaponAttackContext& OutCtx, bool bCommitAbility)
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

	const FAscenduntWeaponModeStats& ModeStats = Weapon->GetCurrentModeStats();
	if (ModeStats.FireType == EAscenduntWeaponFireType::Projectile && !ModeStats.ProjectileClass)
	{
		return false;
	}

	// 클라이언트 예측과 서버 검증에 각각 무기의 발사 시간 게이트를 적용한다.
	UWorld* World = CurrentActorInfo->AvatarActor->GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float Interval = GetShotIntervalSeconds(ASC);
	const bool bAuth = CurrentActorInfo->IsNetAuthority();

	if (Weapon && !Weapon->CanFireByTime(/*bAuthority*/ bAuth, Now))
	{
		return false;
	}

	// ========= 발사 처리 =========
	//
	// 플레이어 Shot은 준비 성공 후 직접 Commit한다. AI 반복 발사는 여기서 트리거당 한 번 Commit한다.
	if (bCommitAbility)
	{
		if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		{
			return false;
		}
	}

	OutCtx.AscASC = AscASC;
	OutCtx.Weapon = Weapon;
	OutCtx.AmmoCost = AmmoCost;
	OutCtx.Now = Now;
	OutCtx.Interval = Interval;
	OutCtx.bAuthority = bAuth;
	OutCtx.DamageCoef = DamageCoef;
	OutCtx.ShotIndex = ShotIndex;

	if (const IAscWeaponAOEProviderInterface* AOEProvider =
	        Cast<IAscWeaponAOEProviderInterface>(CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr))
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

void UGA_BaseFire::FinalizeShot(const FAscenduntWeaponAttackContext& Ctx, const FVector& Start, const FVector& EndOrImpact, bool bHasAnyHit,
    const FHitResult* Hit)
{
	UAscenduntAbilitySystemComponent* ASC = Ctx.AscASC.Get();
	AAscenduntWeapon* Weapon = Ctx.Weapon.Get();
	if (!ASC || !Weapon || !CurrentActorInfo)
	{
		return;
	}

	// One invocation on listen hosts; server echoes are suppressed by the originating prediction key.
	if (Ctx.bAuthority || CurrentActorInfo->IsLocallyControlled())
	{
		PlayFireCue(ASC, Start, EndOrImpact, bHasAnyHit ? Hit : nullptr, Ctx.Interval);
	}

	Weapon->MarkFired(Ctx.bAuthority, Ctx.Now, Ctx.Interval);
	if (Ctx.bAuthority && Weapon->GetCurrentAmmoInMag() <= 0 && !Weapon->IsInfiniteMag())
	{
		TryAutoReload(ASC, Weapon);
	}
}

void UGA_BaseFire::PlayFireCue(UAscenduntAbilitySystemComponent* ASC, const FVector& Start, const FVector& EndOrImpact, const FHitResult* Hit,
    float ShotInterval) const
{
	if (!ASC || !FireCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters P;
	P.Instigator = ASC->GetAvatarActor();
	P.EffectCauser = Cast<AActor>(GetCurrentSourceObject());
	P.SourceObject = GetCurrentSourceObject();
	P.Location = EndOrImpact;
	P.RawMagnitude = ShotInterval;
	P.EffectContext = ASC->MakeEffectContext();
	P.EffectContext.AddSourceObject(GetCurrentSourceObject());
	P.EffectContext.AddOrigin(Start);
	if (Hit)
	{
		P.EffectContext.AddHitResult(*Hit, true);
	}

	ASC->ExecuteGameplayCue(FireCueTag, P);
}

void UGA_BaseFire::PlayTraceCue(UAscenduntAbilitySystemComponent* ASC, const FVector& Start, const FVector& End, bool bUnusedLegacyRoute) const
{
	if (!ASC || !TraceCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters P;
	P.Instigator = ASC->GetAvatarActor();
	P.EffectCauser = Cast<AActor>(GetCurrentSourceObject());
	P.SourceObject = GetCurrentSourceObject();
	P.Location = End;
	P.EffectContext = ASC->MakeEffectContext();
	P.EffectContext.AddSourceObject(GetCurrentSourceObject());
	P.EffectContext.AddOrigin(Start);
	ASC->ExecuteGameplayCue(TraceCueTag, P);
}

void UGA_BaseFire::PlayImpactWorldCue(UAscenduntAbilitySystemComponent* ASC, const FHitResult& Hit, bool bIsReplicated, bool bAOEImpact) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || !WorldImpactCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters P;
	P.Instigator = ASC->GetAvatarActor();
	P.EffectCauser = Cast<AActor>(GetCurrentSourceObject());
	P.SourceObject = GetCurrentSourceObject();
	P.Location = Hit.ImpactPoint;
	P.Normal = Hit.ImpactNormal;
	P.PhysicalMaterial = Hit.PhysMaterial.Get();
	if (bAOEImpact)
	{
		P.AggregatedSourceTags.AddTag(AscenduntGameplayTags::Effect_Weapon_AOEImpact);
	}

	P.EffectContext = ASC->MakeEffectContext();
	P.EffectContext.AddSourceObject(GetCurrentSourceObject());
	P.EffectContext.AddOrigin(Hit.TraceStart);
	P.EffectContext.AddHitResult(Hit, true);
	// Owner did not predict this cue. A shot key here would incorrectly suppress its delivery.
	FScopedPredictionWindow UnpredictedImpact(ASC, FPredictionKey(), false);
	ASC->ExecuteGameplayCue(WorldImpactCueTag, P);
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
		return HitCollision && HitCollision->IsHeadZone() && SourceASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_AllowHeadshot);
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

bool UGA_BaseFire::MakeShotImpactDecision(const FAscenduntWeaponAttackContext& Ctx, const FVector& Start, const FVector& End,
    FAscShotImpactDecision& OutDecision) const
{
	OutDecision = FAscShotImpactDecision();

	AAscenduntWeapon* ContextWeapon = Ctx.Weapon.Get();
	if (!ContextWeapon)
	{
		return false;
	}

	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AscWeaponTrace), /*bTraceComplex=*/false);
	Params.bReturnPhysicalMaterial = true;

	if (Avatar)
	{
		Params.AddIgnoredActor(Avatar);
	}

	if (AActor* WeaponActor = Cast<AActor>(GetCurrentSourceObject()))
	{
		Params.AddIgnoredActor(WeaponActor);
	}

	TArray<FHitResult> Hits;
	World->LineTraceMultiByChannel(Hits, Start, End, TraceChannel, Params);
	if (Hits.Num() == 0)
	{
		return false;
	}

	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });

	const AActor* SourceAvatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;

	const FAscenduntWeaponModeStats* ModeStats = (ContextWeapon ? &ContextWeapon->GetCurrentModeStats() : nullptr);
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

bool UGA_BaseFire::FireHitscan(const FAscenduntWeaponAttackContext& Ctx)
{
	UAscenduntAbilitySystemComponent* ContextASC = Ctx.AscASC.Get();
	AAscenduntWeapon* ContextWeapon = Ctx.Weapon.Get();
	if (!ContextASC || !ContextWeapon)
	{
		return false;
	}

	const float MaxRange = ContextWeapon ? ContextWeapon->GetMaxRange() : 10000.f;

	FVector TraceStart, ViewDir, FireCueStart;
	if (!BuildViewRayAndMuzzle(CurrentActorInfo, ContextWeapon, TraceStart, ViewDir, FireCueStart))
	{
		return false;
	}

	const int32 PelletCount = FMath::Max(1, GetProjectilesPerTrigger(ContextASC));
	const float SpreadDeg = GetBaseSpreadDegSafe(ContextASC);

	if (Ctx.bAuthority)
	{
		if (UAscCombatContributionComponent* Contribution = AscFireInternal::ResolveCombatContribution(ContextASC))
		{
			Contribution->RecordWeaponShotsFired(PelletCount);
		}
	}

	const bool bHasTraceVFX = ContextWeapon && ContextWeapon->GetCurrentModeStats().FireTraceVFXNiagara != nullptr;

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

		// Hitscan Trail은 Pellet 단위로 실행한다. FireCue는 아래 FinalizeShot에서 Trigger당 1회만 실행한다.
		if (bHasTraceVFX && (Ctx.bAuthority || CurrentActorInfo->IsLocallyControlled()))
		{
			PlayTraceCue(ContextASC, FireCueStart, EndOrImpact, false);
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

		// 서버 authoritative
		if (Ctx.bAuthority && bHasDecision)
		{
			if (Decision.Kind == EAscShotImpactKind::WorldBlock)
			{
				PlayImpactWorldCue(ContextASC, Decision.Hit, true, Ctx.bAOEImpact);
				ApplyAOEDamage_Server(Ctx, Decision.Hit.ImpactPoint, nullptr);
			}
			else
			{
				if (UAscCombatContributionComponent* Contribution = AscFireInternal::ResolveCombatContribution(ContextASC))
				{
					Contribution->RecordWeaponHit(AscFireInternal::IsCriticalWeaponHit(ContextASC, Decision));
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

	FinalizeShot(Ctx, FireCueStart, FireCueEnd, bHasAnyImpact, bHasAnyImpact ? &FireCueHit : nullptr);
	return true;
}

FVector UGA_BaseFire::ComputeProjectileLaunchDir(const FAscenduntWeaponAttackContext& Ctx, const FVector& MuzzleLoc, const FVector& AimPoint) const
{
	return (AimPoint - MuzzleLoc).GetSafeNormal();
}

void UGA_BaseFire::ApplyAOEDamage_Server(const FAscenduntWeaponAttackContext& Ctx, const FVector& ImpactPoint, AActor* DirectTarget) const
{
	UAscenduntAbilitySystemComponent* ContextASC = Ctx.AscASC.Get();
	AAscenduntWeapon* ContextWeapon = Ctx.Weapon.Get();
	if (!ContextASC || !ContextWeapon)
	{
		return;
	}

	if (!Ctx.bAuthority || !Ctx.bAOEImpact || Ctx.AOERadius <= KINDA_SMALL_NUMBER || !ContextASC || !ContextWeapon || !DamageEffectClass)
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
		ContextWeapon->Multicast_DrawAOEDebugSphere(ImpactPoint, Ctx.AOERadius, Ctx.AOEDebugDrawDuration);
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
	const float DirectDamageBase = Ctx.DamageOverride >= 0.f ? Ctx.DamageOverride : ContextWeapon->GetCurrentModeStats().BaseDamage * Ctx.DamageCoef;
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

		FGameplayEffectContextHandle EffectCtx = ContextASC->MakeEffectContext();
		EffectCtx.AddSourceObject(GetCurrentSourceObject());
		EffectCtx.AddOrigin(ImpactPoint);
		EffectCtx.AddHitResult(SplashHit, true);
		if (FAscenduntGameplayEffectContext* Typed = FAscenduntGameplayEffectContext::ExtractEffectContext(EffectCtx))
		{
			Typed->SetAbilitySource(Cast<IAscenduntAbilitySourceInterface>(GetCurrentSourceObject()), GetAbilityLevel());
		}

		FGameplayEffectSpecHandle Spec = ContextASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectCtx);
		if (!Spec.IsValid())
		{
			continue;
		}

		Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageOverride, SplashDamage);
		Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Instances, 1.f);
		Spec.Data->AddDynamicAssetTag(AscenduntGameplayTags::Effect_Weapon_AOESplash);
		ContextASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

bool UGA_BaseFire::SpawnProjectile_Server(const FAscenduntWeaponAttackContext& Ctx, const FVector& SpawnLoc, const FVector& Dir) const
{
	UAscenduntAbilitySystemComponent* ContextASC = Ctx.AscASC.Get();
	AAscenduntWeapon* ContextWeapon = Ctx.Weapon.Get();
	if (!ContextASC || !ContextWeapon)
	{
		return false;
	}

	check(Ctx.bAuthority);
	if (!ContextWeapon)
	{
		return false;
	}

	const FAscenduntWeaponModeStats& S = ContextWeapon->GetCurrentModeStats();
	if (!S.ProjectileClass)
	{
		return false;
	}

	UWorld* World = ContextWeapon->GetWorld();
	if (!World)
	{
		return false;
	}

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
			return false;
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

	if (!Proj)
	{
		return false;
	}

	const float Speed = ContextASC->GetNumericAttribute(UAscenduntWeaponSet::GetProjectileSpeedAttribute());
	const float Scale = ContextASC->GetNumericAttribute(UAscenduntWeaponSet::GetProjectileScaleAttribute());
	const bool bCanBeNullified = ContextASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_ProjectileCanBeNullified);
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
	Init.DamageOverride = Ctx.DamageOverride;
	Init.ActiveDuration = S.ProjectileActivateDuration;
	Init.Instances = 1.f;
	Init.bAOEImpact = Ctx.bAOEImpact;
	Init.AOERadius = Ctx.AOERadius;
	Init.AOEDamageCoefficient = Ctx.AOEDamageCoefficient;
	Init.bDebugDrawAOERadius = Ctx.bDebugDrawAOERadius;
	Init.AOEDebugDrawDuration = Ctx.AOEDebugDrawDuration;

	Proj->SetDamageContext(ContextASC, GetCurrentSourceObject(), DamageEffectClass, HealEffectClass, S.bCanHeal, BaseDamage, BaseHeal,
	    S.bProjectileReturnToPoolOnWorldHit);

	Proj->SetActorLocationAndRotation(SpawnLoc, Dir.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	Proj->ActivateFromGA(Init);
	return true;
}

bool UGA_BaseFire::FireProjectile(const FAscenduntWeaponAttackContext& Ctx)
{
	UAscenduntAbilitySystemComponent* ContextASC = Ctx.AscASC.Get();
	AAscenduntWeapon* ContextWeapon = Ctx.Weapon.Get();
	if (!ContextASC || !ContextWeapon)
	{
		return false;
	}

	const float MaxRange = ContextWeapon ? ContextWeapon->GetMaxRange() : 10000.f;

	FVector ViewStart, ViewDir, MuzzleLoc;
	if (!BuildViewRayAndMuzzle(CurrentActorInfo, ContextWeapon, ViewStart, ViewDir, MuzzleLoc))
	{
		return false;
	}

	const FVector ViewEnd = ViewStart + ViewDir * MaxRange;

	// Aim decision uses the same rule table as hitscan
	FAscShotImpactDecision AimDecision;
	const bool bHasAimDecision = MakeShotImpactDecision(Ctx, ViewStart, ViewEnd, AimDecision);
	const FVector AimPoint = bHasAimDecision ? AscFireInternal::CalcHitPointOnRay(ViewStart, ViewEnd, AimDecision.Hit) : ViewEnd;

	const FVector BaseDir = ComputeProjectileLaunchDir(Ctx, MuzzleLoc, AimPoint);

	const int32 Count = FMath::Max(1, GetProjectilesPerTrigger(ContextASC));
	const float SpreadDeg = GetBaseSpreadDegSafe(ContextASC);

	// 서버: 스폰
	if (Ctx.bAuthority)
	{
		int32 SpawnedCount = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 Seed = (int32)CurrentActivationInfo.GetActivationPredictionKey().Current + (Ctx.ShotIndex * 101) + i;
			const FVector Dir = ApplySpreadDeterministic(BaseDir, SpreadDeg, Seed);
			SpawnedCount += SpawnProjectile_Server(Ctx, MuzzleLoc, Dir) ? 1 : 0;
		}

		if (UAscCombatContributionComponent* Contribution = AscFireInternal::ResolveCombatContribution(ContextASC))
		{
			Contribution->RecordWeaponShotsFired(SpawnedCount);
		}
	}

	// Fire Cue 재생 및 다음 발사 시간 기록. 탄약 Cost는 앞선 Commit에서 적용한다.
	FinalizeShot(Ctx, MuzzleLoc, AimPoint, bHasAimDecision, bHasAimDecision ? &AimDecision.Hit : nullptr);
	return true;
}

bool UGA_BaseFire::TryAutoReload(UAbilitySystemComponent* ASC, AAscenduntWeapon* Weapon)
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

bool UGA_BaseFire::CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return Weapon && ASC && Weapon->CanFire(GetAmmoCostPerTrigger(ASC));
}

void UGA_BaseFire::ApplyCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(GetSourceObject(Handle, ActorInfo));
	if (!Weapon || Weapon->IsInfiniteMag())
	{
		return;
	}

	FGameplayEffectSpecHandle Spec =
	    MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, UAscWeaponAmmoCost::StaticClass(), GetAbilityLevel(Handle, ActorInfo));
	if (Spec.IsValid())
	{
		const FActiveGameplayEffectHandle CostHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
		if (ActorInfo && !ActorInfo->IsNetAuthority())
		{
			Weapon->TrackPredictedAmmoCost(CostHandle);
		}
	}
}
