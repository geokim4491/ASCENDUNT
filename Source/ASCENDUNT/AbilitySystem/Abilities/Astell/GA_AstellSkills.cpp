#include "GA_AstellSkills.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "ASCENDUNT/AbilitySystem/Components/AscSkillStockComponent.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Data/AscAbilityData.h"
#include "ASCENDUNT/AbilitySystem/Utility/AscTargetUtil.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/Hero/Astell/AstellCharacter.h"
#include "ASCENDUNT/System/AscCollisionChannels.h"
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"

namespace AscAstellAbility
{
	static AAstellCharacter* GetAstell(const FGameplayAbilityActorInfo* ActorInfo)
	{
		return ActorInfo ? Cast<AAstellCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	}

	static FGameplayTag ResolveStockTag(FGameplayTag DataTag, FGameplayTag FallbackTag)
	{
		return DataTag.IsValid() ? DataTag : FallbackTag;
	}

	static FVector GetAimDirection(const FGameplayAbilityActorInfo* ActorInfo)
	{
		if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
		{
			return FVector::ForwardVector;
		}

		FVector ViewLoc;
		FRotator ViewRot;
		if (ActorInfo->PlayerController.IsValid())
		{
			ActorInfo->PlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);
			return ViewRot.Vector().GetSafeNormal();
		}

		return ActorInfo->AvatarActor->GetActorForwardVector();
	}

	static bool IsInvincibleTarget(AActor* TargetActor)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
		return TargetASC
			&& TargetASC->HasMatchingGameplayTag(AscenduntGameplayTags::Status_Invincible);
	}

	static FHitResult MakeFallbackHit(AActor* SourceActor, AActor* TargetActor)
	{
		FHitResult Hit;
		Hit.ImpactPoint = TargetActor ? TargetActor->GetActorLocation() : FVector::ZeroVector;
		Hit.Location = Hit.ImpactPoint;
		Hit.TraceStart = SourceActor ? SourceActor->GetActorLocation() : FVector::ZeroVector;
		Hit.TraceEnd = Hit.ImpactPoint;
		return Hit;
	}
}

UGA_AstellSwordWave::UGA_AstellSwordWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Astell_SwordWave);
}

const UAscAbilityData* UGA_AstellSwordWave::GetAbilityData() const
{
	return SwordWaveData;
}

bool UGA_AstellSwordWave::GetSkillStockDefinition(FGameplayTag& OutStockTag, int32& OutMaxStock, float& OutRecoverDuration) const
{
	if (!SwordWaveData)
	{
		return Super::GetSkillStockDefinition(OutStockTag, OutMaxStock, OutRecoverDuration);
	}

	OutStockTag = AscAstellAbility::ResolveStockTag(SwordWaveData->StockTag, AscenduntGameplayTags::Stock_Astell_SwordWave);
	OutMaxStock = SwordWaveData->MaxStock;
	OutRecoverDuration = SwordWaveData->RefillToMaxTime;
	return OutStockTag.IsValid() && OutMaxStock > 0;
}

bool UGA_AstellSwordWave::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !SwordWaveData)
	{
		return false;
	}

	const AAstellCharacter* Astell = AscAstellAbility::GetAstell(ActorInfo);
	const UAscSkillStockComponent* StockComp = Astell ? Astell->GetSkillStockComponent() : nullptr;
	const FGameplayTag StockTag = AscAstellAbility::ResolveStockTag(SwordWaveData->StockTag, AscenduntGameplayTags::Stock_Astell_SwordWave);
	return StockComp && (StockComp->GetMaxStock(StockTag) <= 0 || StockComp->CanConsumeStock(StockTag, 1));
}

void UGA_AstellSwordWave::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!SwordWaveData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AAstellCharacter* Astell = AscAstellAbility::GetAstell(ActorInfo);
	UAscSkillStockComponent* StockComp = Astell ? Astell->GetSkillStockComponent() : nullptr;
	const FGameplayTag StockTag = AscAstellAbility::ResolveStockTag(SwordWaveData->StockTag, AscenduntGameplayTags::Stock_Astell_SwordWave);
	if (StockComp && Astell && Astell->HasAuthority())
	{
		StockComp->RegisterStock(StockTag, SwordWaveData->MaxStock, SwordWaveData->RefillToMaxTime, EAscStockRecoverMode::RefillToMax, false);
		if (!StockComp->TryConsumeStock(StockTag, 1, true))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	const bool bWaitForMontageEnd = StartAbilityMontageLifecycle(SwordWaveData);

	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		SpawnSwordWaveProjectile(SwordWaveData);
	}

	if (!bWaitForMontageEnd)
	{
		FinishDataAbility(false);
	}
}

bool UGA_AstellSwordWave::SpawnSwordWaveProjectile(const UAstellSwordWaveData* Data) const
{
	if (!CurrentActorInfo || !Data || !Data->ProjectileClass)
	{
		return false;
	}

	AActor* AvatarActor = CurrentActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !SourceASC || !World)
	{
		return false;
	}

	const FVector SpawnLoc =  AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * 100.0f;
	const FVector Dir = AscAstellAbility::GetAimDirection(CurrentActorInfo);
	const FTransform SpawnTransform(Dir.Rotation(), SpawnLoc);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAscenduntProjectile* Projectile = World->SpawnActor<AAscenduntProjectile>(Data->ProjectileClass, SpawnTransform, SpawnParams);
	if (!Projectile)
	{
		return false;
	}

	FAscenduntProjectileInitData InitData;
	InitData.LaunchDir = Dir;
	InitData.Speed = Data->ProjectileSpeed;
	InitData.Scale = Data->ProjectileScale;
	InitData.ActiveDuration = Data->ActiveDuration;
	InitData.DamageCoef = Data->DamageCoef;
	InitData.DamageOverride = Data->DamageOverride;
	InitData.bCanBeNullified = true;
	InitData.Instances = 1.f;

	Projectile->SetDamageContext(SourceASC, AvatarActor, Data->DamageEffectClass, nullptr, false, 0.0f, 0.0f, true);
	Projectile->ActivateFromGA(InitData);
	return true;
}

UGA_AstellDashSlash::UGA_AstellDashSlash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Astell_DashSlash);
}

const UAscAbilityData* UGA_AstellDashSlash::GetAbilityData() const
{
	return DashData;
}

bool UGA_AstellDashSlash::GetSkillStockDefinition(FGameplayTag& OutStockTag, int32& OutMaxStock, float& OutRecoverDuration) const
{
	if (!DashData)
	{
		return Super::GetSkillStockDefinition(OutStockTag, OutMaxStock, OutRecoverDuration);
	}

	OutStockTag = AscAstellAbility::ResolveStockTag(DashData->StockTag, AscenduntGameplayTags::Stock_Astell_DashSlash);
	OutMaxStock = DashData->MaxStock;
	OutRecoverDuration = DashData->RecoverOneTime;
	return OutStockTag.IsValid() && OutMaxStock > 0;
}

bool UGA_AstellDashSlash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !DashData)
	{
		return false;
	}

	const AAstellCharacter* Astell = AscAstellAbility::GetAstell(ActorInfo);
	const UAscSkillStockComponent* StockComp = Astell ? Astell->GetSkillStockComponent() : nullptr;
	const FGameplayTag StockTag = AscAstellAbility::ResolveStockTag(DashData->StockTag, AscenduntGameplayTags::Stock_Astell_DashSlash);
	return StockComp && (StockComp->GetMaxStock(StockTag) <= 0 || StockComp->CanConsumeStock(StockTag, 1));
}

void UGA_AstellDashSlash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bDashHitWindowActive = false;
	bDashFinishing = false;
	DashHitActors.Reset();

	if (!DashData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AAstellCharacter* Astell = AscAstellAbility::GetAstell(ActorInfo);
	UAscSkillStockComponent* StockComp = Astell ? Astell->GetSkillStockComponent() : nullptr;
	const FGameplayTag StockTag = AscAstellAbility::ResolveStockTag(DashData->StockTag, AscenduntGameplayTags::Stock_Astell_DashSlash);
	if (StockComp && Astell && Astell->HasAuthority())
	{
		StockComp->RegisterStock(StockTag, DashData->MaxStock, DashData->RecoverOneTime, EAscStockRecoverMode::RecoverOne, false);
		if (!StockComp->TryConsumeStock(StockTag, 1, true))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	const bool bWaitForMontageEnd = StartAbilityMontageLifecycle(DashData);

	// Dash movement is intentionally montage-only. This ability keeps stock consumption,
	// body-centered hit box timing, and damage.
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		ScheduleDashTimer(DashHitStartTimer, GetDashScaledTime(DashData->HitStartTime), &ThisClass::BeginDashHitWindow);
		ScheduleDashTimer(DashHitEndTimer, GetDashScaledTime(GetDashHitEndTime()), &ThisClass::EndDashHitWindow);
	}

	if (!bWaitForMontageEnd)
	{
		FinishDataAbility(false);
	}
}

void UGA_AstellDashSlash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearDashTimers();
	bDashHitWindowActive = false;
	DashHitActors.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AstellDashSlash::ScheduleDashTimer(FTimerHandle& TimerHandle, float Delay, void (UGA_AstellDashSlash::*Callback)())
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TimerHandle);
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		(this->*Callback)();
		return;
	}

	World->GetTimerManager().SetTimer(TimerHandle, this, Callback, Delay, false);
}

float UGA_AstellDashSlash::GetDashScaledTime(float RawTime) const
{
	const float PlayRate = DashData ? FMath::Max(0.01f, DashData->Montage.PlayRate) : 1.0f;
	return FMath::Max(0.0f, RawTime) / PlayRate;
}

float UGA_AstellDashSlash::GetDashHitEndTime() const
{
	if (!DashData)
	{
		return 0.0f;
	}

	return DashData->HitEndTime > DashData->HitStartTime
		? DashData->HitEndTime
		: DashData->HitStartTime;
}

void UGA_AstellDashSlash::OnDataAbilityMontageFinished(bool bWasCancelled)
{
	if (bDashFinishing)
	{
		return;
	}

	bDashFinishing = true;
	EndDashHitWindow();
}

void UGA_AstellDashSlash::BeginDashHitWindow()
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	bDashHitWindowActive = true;
	DashHitActors.Reset();
	TickDashHitBox_Server();

	UWorld* World = GetWorld();
	if (World && DashData)
	{
		World->GetTimerManager().SetTimer(
			DashHitTickTimer,
			this,
			&ThisClass::TickDashHitBox_Server,
			GetDashScaledTime(FMath::Max(0.005f, DashData->HitTickInterval)),
			true);
	}
}

void UGA_AstellDashSlash::TickDashHitBox_Server()
{
	ACharacter* Character = GetCharacterFromActorInfo();
	if (!bDashHitWindowActive || !Character || !DashData || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector ActorLoc = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector();
	const FVector Right = Character->GetActorRightVector();
	const FVector Up = FVector::UpVector;
	const FVector Center = ActorLoc
		+ Forward * DashData->HitBoxLocalOffset.X
		+ Right * DashData->HitBoxLocalOffset.Y
		+ Up * DashData->HitBoxLocalOffset.Z;
	const FQuat Rotation = Character->GetActorQuat();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AscAstellDashSlash), false);
	Params.AddIgnoredActor(Character);
	Params.bFindInitialOverlaps = true;

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps,
		Center,
		Rotation,
		AscCollision::WeaponTrace,
		FCollisionShape::MakeBox(DashData->HitBoxHalfExtent),
		Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || DashHitActors.Contains(HitActor) || !UAscTargetUtil::IsEnemyActor(Character, HitActor))
		{
			continue;
		}

		if (!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor))
		{
			continue;
		}

		const FHitResult Hit = AscAstellAbility::MakeFallbackHit(Character, HitActor);
		if (ApplyDamageEffectToActor(HitActor, DashData->DamageEffectClass, Hit, DashData->DamageOverride, DashData->DamageCoef))
		{
			DashHitActors.Add(HitActor);
		}
	}

	if (DashData->bDrawDebugHitBox)
	{
		DrawDebugBox(World, Center, DashData->HitBoxHalfExtent, Rotation, FColor::Cyan, false,
			DashData->DebugDrawDuration, 0, DashData->DebugDrawThickness);
	}
}

void UGA_AstellDashSlash::EndDashHitWindow()
{
	bDashHitWindowActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashHitTickTimer);
	}
}


void UGA_AstellDashSlash::ClearDashTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(DashHitStartTimer);
		TimerManager.ClearTimer(DashHitTickTimer);
		TimerManager.ClearTimer(DashHitEndTimer);
	}
}

UGA_AstellHolyExplosion::UGA_AstellHolyExplosion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Astell_HolyExplosion);
}

const UAscAbilityData* UGA_AstellHolyExplosion::GetAbilityData() const
{
	return ExplosionData;
}

void UGA_AstellHolyExplosion::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bExplosionWaitingForMontageEnd = false;
	bExplosionDamageResolved = false;
	bExplosionKnockbackResolved = false;
	ExplosionAffectedTargets.Reset();
	ClearExplosionTimers();

	if (!ExplosionData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AAstellCharacter* Astell = AscAstellAbility::GetAstell(ActorInfo);

	bExplosionWaitingForMontageEnd = StartAbilityMontageLifecycle(ExplosionData);

	if (ActorInfo && ActorInfo->IsNetAuthority() && Astell)
	{
		ScheduleExplosionTimer(ExplosionDamageTimer, GetExplosionScaledTime(ExplosionData->DamageApplyTime), &ThisClass::ApplyExplosionDamage_Server);
		ScheduleExplosionTimer(ExplosionKnockbackTimer, GetExplosionScaledTime(ExplosionData->KnockbackApplyTime), &ThisClass::ApplyExplosionKnockback_Server);
	}
	else
	{
		// Non-authority instances do not apply gameplay effects. They only keep the montage lifecycle.
		bExplosionDamageResolved = true;
		bExplosionKnockbackResolved = true;
	}

	if (!bExplosionWaitingForMontageEnd)
	{
		TryFinishExplosionAfterTimedEvents();
	}
}

void UGA_AstellHolyExplosion::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearExplosionTimers();
	ExplosionAffectedTargets.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AstellHolyExplosion::ScheduleExplosionTimer(FTimerHandle& TimerHandle, float Delay, void (UGA_AstellHolyExplosion::*Callback)())
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TimerHandle);
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		(this->*Callback)();
		return;
	}

	World->GetTimerManager().SetTimer(TimerHandle, this, Callback, Delay, false);
}

float UGA_AstellHolyExplosion::GetExplosionScaledTime(float RawTime) const
{
	const float PlayRate = ExplosionData ? FMath::Max(0.01f, ExplosionData->Montage.PlayRate) : 1.0f;
	return FMath::Max(0.0f, RawTime) / PlayRate;
}

void UGA_AstellHolyExplosion::ApplyExplosionDamage_Server()
{
	if (bExplosionDamageResolved)
	{
		return;
	}

	bExplosionDamageResolved = true;

	AAstellCharacter* Astell = AscAstellAbility::GetAstell(CurrentActorInfo);
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !Astell || !ExplosionData)
	{
		TryFinishExplosionAfterTimedEvents();
		return;
	}

	TArray<AActor*> Targets;
	UAscTargetUtil::FindEnemyActorsInRadius(Astell, ExplosionData->Radius, Targets);

	ExplosionAffectedTargets.Reset();


	for (AActor* Target : Targets)
	{
		if (!Target || AscAstellAbility::IsInvincibleTarget(Target))
		{
			continue;
		}

		ExplosionAffectedTargets.AddUnique(TWeakObjectPtr<AActor>(Target));


		const FHitResult Hit = AscAstellAbility::MakeFallbackHit(Astell, Target);
		const bool bAppliedBaseDamage = ApplyDamageEffectToActor(Target, ExplosionData->BaseDamageEffectClass, Hit,
			ExplosionData->BaseDamageOverride, ExplosionData->BaseDamageCoef);


		if (bAppliedBaseDamage)
		{
			UAscenduntAbilitySystemComponent* TargetASC = Cast<UAscenduntAbilitySystemComponent>(
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target));

			const FGameplayTag CueTag = ExplosionData->GameplayCueTag.IsValid()
				? ExplosionData->GameplayCueTag
				: AscenduntGameplayTags::GameplayCue_Replicated_Astell_HolyExplosion_Target;

			if (TargetASC && TargetASC->IsOwnerActorAuthoritative() && CueTag.IsValid())
			{
				FGameplayCueParameters CueParameters;
				CueParameters.Instigator = Astell;
				CueParameters.EffectCauser = Astell;
				CueParameters.Location = Target->GetActorLocation();

				TargetASC->ExecuteGameplayCue(CueTag, CueParameters);
			}
		}

	}

	DrawExplosionDebugSphere(FColor::Red);
	TryFinishExplosionAfterTimedEvents();
}

void UGA_AstellHolyExplosion::ApplyExplosionKnockback_Server()
{
	if (bExplosionKnockbackResolved)
	{
		return;
	}

	bExplosionKnockbackResolved = true;

	AAstellCharacter* Astell = AscAstellAbility::GetAstell(CurrentActorInfo);
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !Astell || !ExplosionData)
	{
		TryFinishExplosionAfterTimedEvents();
		return;
	}

	if (ExplosionAffectedTargets.Num() > 0)
	{
		for (const TWeakObjectPtr<AActor>& TargetPtr : ExplosionAffectedTargets)
		{
			AActor* Target = TargetPtr.Get();
			if (Target && !AscAstellAbility::IsInvincibleTarget(Target))
			{
				UAscTargetUtil::ApplyKnockback(Astell, Target, ExplosionData->KnockbackStrength);
			}
		}
	}
	else
	{
		TArray<AActor*> Targets;
		UAscTargetUtil::FindEnemyActorsInRadius(Astell, ExplosionData->Radius, Targets);
		for (AActor* Target : Targets)
		{
			if (Target && !AscAstellAbility::IsInvincibleTarget(Target))
			{
				UAscTargetUtil::ApplyKnockback(Astell, Target, ExplosionData->KnockbackStrength);
			}
		}
	}

	DrawExplosionDebugSphere(FColor::Blue);
	TryFinishExplosionAfterTimedEvents();
}

void UGA_AstellHolyExplosion::ClearExplosionTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(ExplosionDamageTimer);
		TimerManager.ClearTimer(ExplosionKnockbackTimer);
	}
}

void UGA_AstellHolyExplosion::DrawExplosionDebugSphere(const FColor& Color) const
{
	AAstellCharacter* Astell = AscAstellAbility::GetAstell(CurrentActorInfo);
	if (!Astell || !ExplosionData || !ExplosionData->bDrawDebugRadius)
	{
		return;
	}

	DrawDebugSphere(
		Astell->GetWorld(),
		Astell->GetActorLocation(),
		ExplosionData->Radius,
		32,
		Color,
		false,
		ExplosionData->DebugDrawDuration,
		0,
		ExplosionData->DebugDrawThickness);
}

void UGA_AstellHolyExplosion::TryFinishExplosionAfterTimedEvents()
{
	if (bExplosionWaitingForMontageEnd)
	{
		return;
	}

	if (bExplosionDamageResolved && bExplosionKnockbackResolved)
	{
		FinishDataAbility(false);
	}
}

UGA_AstellLeap::UGA_AstellLeap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Astell_Leap);
}

const UAscAbilityData* UGA_AstellLeap::GetAbilityData() const
{
	return LeapData;
}

bool UGA_AstellLeap::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !LeapData || !bLeapAvailable)
	{
		return false;
	}

	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
	return MoveComp && MoveComp->IsFalling();
}

void UGA_AstellLeap::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!LeapData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bLeapAvailable = false;
	const bool bWaitForMontageEnd = StartAbilityMontageLifecycle(LeapData);

	ACharacter* Character = GetCharacterFromActorInfo();
	if (Character)
	{
		const FVector LaunchVelocity = Character->GetActorForwardVector() * LeapData->ForwardStrength + FVector::UpVector * LeapData->UpwardStrength;
		Character->LaunchCharacter(LaunchVelocity, true, true);

		if (UWorld* World = Character->GetWorld())
		{
			World->GetTimerManager().ClearTimer(LeapResetTimer);
			World->GetTimerManager().SetTimer(LeapResetTimer, this, &ThisClass::TryResetLeapAvailability, LeapData->GroundCheckInterval, true);
		}
	}

	AAstellCharacter* Astell = AscAstellAbility::GetAstell(ActorInfo);
	if (ActorInfo && ActorInfo->IsNetAuthority() && Astell)
	{
		TArray<AActor*> Targets;
		UAscTargetUtil::FindEnemyActorsInRadius(Astell, LeapData->HitRadius, Targets);
		for (AActor* Target : Targets)
		{
			if (!Target)
			{
				continue;
			}

			const FHitResult Hit = AscAstellAbility::MakeFallbackHit(Astell, Target);
			ApplyDamageEffectToActor(Target, LeapData->DamageEffectClass, Hit, LeapData->DamageOverride, LeapData->DamageCoef);
			UAscTargetUtil::ApplyKnockback(Astell, Target, LeapData->KnockbackStrength);

		}
	}

	if (!bWaitForMontageEnd)
	{
		FinishDataAbility(false);
	}
}

void UGA_AstellLeap::TryResetLeapAvailability()
{
	ACharacter* Character = GetCharacterFromActorInfo();
	const UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
	if (!MoveComp || !MoveComp->IsFalling())
	{
		bLeapAvailable = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LeapResetTimer);
		}
	}
}

UGA_AstellUlt::UGA_AstellUlt(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(AscenduntGameplayTags::Ability_Astell_Ultimate);
}

const UAscAbilityData* UGA_AstellUlt::GetAbilityData() const
{
	return UltData;
}

void UGA_AstellUlt::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!UltData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bWaitForMontageEnd = StartAbilityMontageLifecycle(UltData);


	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		ApplyGameplayEffectToOwner(UltData->BuffEffectClass, GetAbilityLevel());
	}

	if (!bWaitForMontageEnd)
	{
		FinishDataAbility(false);
	}
}

