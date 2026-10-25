#include "AscenduntProjectile.h"
// Fill out your copyright notice in the Description page of Project Settings.



#include "ASCENDUNT/System/AscenduntProjectilePoolSubsystem.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/AbilitySystem/Utility/AscTargetUtil.h"

#include "AbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "GenericTeamAgentInterface.h"
#include "ASCENDUNT/AbilitySystem/AscenduntGameplayEffectContext.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySourceInterface.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Components/AscCombatContributionComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Character/HitCollision/AscHitCollisionComponent.h"

#include "Net/UnrealNetwork.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

namespace AscProjInternal
{
	static const AActor* GetSourceActor(const AAscenduntProjectile* P)
	{
		if (!P) return nullptr;
		if (const APawn* Inst = P->GetInstigator())
		{
			return Inst;
		}
		return P->GetOwner();
	}

	static UAscCombatContributionComponent* ResolveCombatContribution(UAbilitySystemComponent* SourceASC)
	{
		if (!SourceASC) return nullptr;
		AAscenduntPlayerState* PlayerState = Cast<AAscenduntPlayerState>(SourceASC->GetOwnerActor());
		return PlayerState ? PlayerState->GetCombatContributionComponent() : nullptr;
	}

	static bool IsCriticalWeaponHit(UAbilitySystemComponent* SourceASC, const UPrimitiveComponent* HitComponent)
	{
		const UAscHitCollisionComponent* HitCollision = Cast<UAscHitCollisionComponent>(HitComponent);
		return SourceASC && HitCollision && HitCollision->IsHeadZone() &&
			SourceASC->HasMatchingGameplayTag(AscenduntGameplayTags::Data_Weapon_AllowHeadshot);
	}

	static bool IsFriendly(const AActor* A, const AActor* B)
	{
		if (!A || !B) return false;

		const IGenericTeamAgentInterface* TA = Cast<IGenericTeamAgentInterface>(A);
		const IGenericTeamAgentInterface* TB = Cast<IGenericTeamAgentInterface>(B);
		if (!TA || !TB) return false;

		return TA->GetGenericTeamId() == TB->GetGenericTeamId();
	}

	static bool IsFullHealth(const AActor* Actor)
	{
		const UAscenduntHealthComponent* HC = UAscenduntHealthComponent::FindHealthComponent(Actor);
		if (!HC || !HC->HasValidHealthSet())
		{
			// 측정 불가면 "풀피 아님" 취급(힐이 가능하도록)
			return false;
		}
		const float H = HC->GetHealth();
		const float M = HC->GetMaxHealth();
		return (M > 0.f) && (H >= M - KINDA_SMALL_NUMBER);
	}

	static void NudgeForward(AActor* Actor, const FVector& Dir, float Dist)
	{
		if (!Actor) return;
		Actor->SetActorLocation(Actor->GetActorLocation() + Dir * Dist, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

AAscenduntProjectile::AAscenduntProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(8.f);
	Collision->SetSimulatePhysics(false);
	Collision->SetMobility(EComponentMobility::Movable);

	// Query 기반 (ProjectileMovement가 Sweep)
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// ObjectType은 일단 WorldDynamic 유지 (프로젝트에서 별도 오브젝트 채널 쓰면 바꿔도 됨)
	Collision->SetCollisionObjectType(ECC_WorldDynamic);

	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);


	Collision->SetGenerateOverlapEvents(true);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AAscenduntProjectile::OnProjectileBeginOverlap);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.f;
	Movement->SetUpdatedComponent(Collision);
	Movement->bAutoActivate = false;

	// 월드 Blocking으로 멈췄을 때 처리 (벽 맞고 풀 복귀 안되던 문제 해결)
	Movement->OnProjectileStop.AddDynamic(this, &AAscenduntProjectile::HandleProjectileStop);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	MeshComp->SetupAttachment(Collision);
	MeshComp->SetRelativeLocation(FVector::ZeroVector);
	MeshComp->SetRelativeRotation(FRotator::ZeroRotator);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetGenerateOverlapEvents(false);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	// X-Ray Blocker용 CustomDepth만 사용한다.
	MeshComp->SetRenderCustomDepth(true);
	MeshComp->MarkRenderStateDirty();
}

void AAscenduntProjectile::SetDamageContext(
	UAbilitySystemComponent* InSourceASC,
	UObject* InSourceObject,
	TSubclassOf<UGameplayEffect> InDamageEffect,
	TSubclassOf<UGameplayEffect> InHealEffect,
	bool bInCanHeal,
	float InBaseDamage,
	float InBaseHeal,
	bool bInReturnToPoolOnWorldHit)
{
	SourceASC = InSourceASC;
	SourceObject = InSourceObject;
	DamageEffectClass = InDamageEffect;
	HealEffectClass = InHealEffect;
	bCanHeal = bInCanHeal;
	BaseDamage = InBaseDamage;
	BaseHeal = InBaseHeal;

	bReturnToPoolOnWorldHit = bInReturnToPoolOnWorldHit;
}

void AAscenduntProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void AAscenduntProjectile::SetActiveState(bool bInActive)
{
	bActive = bInActive;

	SetActorHiddenInGame(!bActive);
	SetActorTickEnabled(bActive);

	Collision->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (!bActive)
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
		GetWorldTimerManager().ClearTimer(LifeTimer);
	}
	else
	{
		Movement->SetUpdatedComponent(Collision);
		Movement->Activate(true);
	}
}

void AAscenduntProjectile::OnAcquiredFromPool()
{
	// 필요하면 Trail Reset 등
	if(Collision)
	{
		Collision->ClearMoveIgnoreActors();
		Collision->ClearMoveIgnoreComponents();
	}
}

void AAscenduntProjectile::OnReturnedToPool()
{
	GetWorldTimerManager().ClearTimer(OverlapEnableTimer);

	bHitConsumed = false;
	SourceASC = nullptr;
	SourceObject = nullptr;
	DamageEffectClass = nullptr;
	HealEffectClass = nullptr;
	bCanHeal = false;
	BaseDamage = 0.f;
	BaseHeal = 0.f;

	bReturnToPoolOnWorldHit = false;
}

void AAscenduntProjectile::ActivateFromGA(const FAscenduntProjectileInitData& InInit)
{
	check(HasAuthority());

	RepInit = InInit;
	bActive = true;

	SetActiveState(true);

	// 스폰 직후 Owner 겹침 방지
	/*Collision->SetGenerateOverlapEvents(false);
	GetWorldTimerManager().ClearTimer(OverlapEnableTimer);
	GetWorldTimerManager().SetTimer(
		OverlapEnableTimer,
		this,
		&AAscenduntProjectile::EnableOverlapEvents,
		0.02f,
		false);*/

	ApplyInitData(InInit);

	if(Collision)
	{
		Collision->ClearMoveIgnoreActors();
		Collision->ClearMoveIgnoreComponents(); // 가능하면 함께
		Collision->SetGenerateOverlapEvents(true);
	}

	// 생명시간 타이머(Impact에서 즉시 반환 안 해도 여기서 정리 가능)
	GetWorldTimerManager().ClearTimer(LifeTimer);
	GetWorldTimerManager().SetTimer(LifeTimer, this, &AAscenduntProjectile::ReleaseToPool, RepInit.ActiveDuration, false);

	ForceNetUpdate();
}

void AAscenduntProjectile::ApplyInitData(const FAscenduntProjectileInitData& Data)
{
	SetActorScale3D(FVector(Data.Scale));

	// 오너/인스티게이터 무시
	if (AActor* OwnerActor = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor, true);
	}
	if (APawn* Inst = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Inst, true);
	}

	Movement->SetUpdatedComponent(Collision);
	Movement->StopMovementImmediately();
	Movement->Activate(true);

	const FVector Dir = Data.LaunchDir.GetSafeNormal();
	Movement->Velocity = Dir * Data.Speed;
	Movement->InitialSpeed = Data.Speed;
	Movement->MaxSpeed = Data.Speed;
	Movement->UpdateComponentVelocity();
}

void AAscenduntProjectile::OnRep_Active()
{
	SetActiveState(bActive);
}

void AAscenduntProjectile::OnRep_Init()
{
	if (bActive)
	{
		ApplyInitData(RepInit);
	}
}

void AAscenduntProjectile::EnableOverlapEvents()
{
	if (Collision && bActive)
	{
		Collision->SetGenerateOverlapEvents(true);
	}
}

void AAscenduntProjectile::InvalidateProjectile()
{
	if (!RepInit.bCanBeNullified)
		return;

	if (HasAuthority())
	{
		ReleaseToPool();
	}
}

void AAscenduntProjectile::ConsumeAndMaybeEnd(bool bReturnToPool)
{
	if (bHitConsumed) return;
	bHitConsumed = true;

	GetWorldTimerManager().ClearTimer(OverlapEnableTimer);

	// 충돌/오버랩 끄기 + 이동 정지
	if (Collision)
	{
		Collision->SetGenerateOverlapEvents(false);
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Movement)
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
		Movement->SetUpdatedComponent(nullptr);
	}

	// 즉시 반환이면 Pool로
	if (bReturnToPool)
	{
		ReleaseToPool();
	}
	// else: ActiveDuration 타이머나 외부 로직이 정리(예: 폭발 연출 후 InvalidateProjectile)
}

void AAscenduntProjectile::ApplyHealToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& Hit) const
{
	if (!SourceASC.IsValid() || !TargetASC || !HealEffectClass) return;

	AActor* SourceAvatar = SourceASC->GetAvatarActor_Direct();
	AActor* SourceInstigator = GetInstigator() ? Cast<AActor>(GetInstigator()) : SourceAvatar;
	AActor* EffectCauser = SourceAvatar ? SourceAvatar : GetOwner();

	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject);
	Ctx.AddInstigator(SourceInstigator, EffectCauser);
	Ctx.AddHitResult(Hit, true);

	if (FAscenduntGameplayEffectContext* Typed = FAscenduntGameplayEffectContext::ExtractEffectContext(Ctx))
	{
		Typed->SetAbilitySource(Cast<IAscenduntAbilitySourceInterface>(SourceObject), /*Level=*/1.f);
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(HealEffectClass, /*Level=*/1.f, Ctx);
	if (!SpecHandle.IsValid()) return;

	// Projectile 발사 시점에 스냅샷으로 저장해둔 무기 Heal 값을 GE에 직접 주입한다.
	SpecHandle.Data->SetSetByCallerMagnitude(
		AscenduntGameplayTags::Data_Weapon_BaseHeal,
		FMath::Max(0.f, BaseHeal)
	);

	// 차지/강화 계수. HealExecution에서도 현재 Data.DamageCoef를 계수로 사용 중이면 유지.
	SpecHandle.Data->SetSetByCallerMagnitude(
		AscenduntGameplayTags::Data_DamageCoef,
		FMath::Max(0.f, RepInit.DamageCoef)
	);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void AAscenduntProjectile::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& Hit) const
{
	if (!SourceASC.IsValid() || !TargetASC || !DamageEffectClass) return;

	AActor* SourceAvatar = SourceASC->GetAvatarActor_Direct();
	AActor* SourceInstigator = GetInstigator() ? Cast<AActor>(GetInstigator()) : SourceAvatar;
	AActor* EffectCauser = SourceAvatar ? SourceAvatar : GetOwner();

	FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject);
	Ctx.AddInstigator(SourceInstigator, EffectCauser);
	Ctx.AddHitResult(Hit, true);

	if (FAscenduntGameplayEffectContext* Typed = FAscenduntGameplayEffectContext::ExtractEffectContext(Ctx))
	{
		Typed->SetAbilitySource(Cast<IAscenduntAbilitySourceInterface>(SourceObject), /*Level=*/1.f);
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, /*Level=*/1.f, Ctx);
	if (!SpecHandle.IsValid()) return;

	// Projectile 발사 시점에 스냅샷으로 저장해둔 무기 Damage 값을 GE에 직접 주입한다.
	SpecHandle.Data->SetSetByCallerMagnitude(
		AscenduntGameplayTags::Data_Weapon_BaseDamage,
		FMath::Max(0.f, BaseDamage)
	);

	if (RepInit.DamageOverride >= 0.f)
	{
		// 절대 데미지 우선. Execution에서 DamageOverride가 있으면 BaseDamage * Coef 경로를 무시하게 하면 됨.
		SpecHandle.Data->SetSetByCallerMagnitude(
			AscenduntGameplayTags::Data_DamageOverride,
			RepInit.DamageOverride
		);
	}
	else
	{
		SpecHandle.Data->SetSetByCallerMagnitude(
			AscenduntGameplayTags::Data_DamageCoef,
			FMath::Max(0.f, RepInit.DamageCoef)
		);
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		AscenduntGameplayTags::Data_Instances,
		FMath::Max(1.f, RepInit.Instances)
	);

	if (RepInit.bAOEImpact)
	{
		SpecHandle.Data->AddDynamicAssetTag(AscenduntGameplayTags::Effect_Weapon_AOEImpact);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}


void AAscenduntProjectile::ApplyAOEDamageAtLocation(const FVector& ImpactPoint, AActor* DirectTarget) const
{
	if (!HasAuthority() || !RepInit.bAOEImpact || RepInit.AOERadius <= KINDA_SMALL_NUMBER || !SourceASC.IsValid() || !DamageEffectClass)
	{
		return;
	}

	AActor* SourceActor = GetInstigator() ? Cast<AActor>(GetInstigator()) : GetOwner();
	UWorld* World = GetWorld();
	if (!World || !SourceActor)
	{
		return;
	}

	if (RepInit.bDebugDrawAOERadius)
	{
		if (AAscenduntWeapon* SourceWeapon = Cast<AAscenduntWeapon>(SourceObject.Get()))
		{
			SourceWeapon->Multicast_DrawAOEDebugSphere(ImpactPoint, RepInit.AOERadius, RepInit.AOEDebugDrawDuration);
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AscProjectileAOE), false, SourceActor);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, ImpactPoint, FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(RepInit.AOERadius), Params);

	TSet<AActor*> Applied;
	const float DirectDamageBase = RepInit.DamageOverride >= 0.f
		? RepInit.DamageOverride
		: BaseDamage * RepInit.DamageCoef;
	const float SplashDamage = FMath::Max(0.f, DirectDamageBase * RepInit.AOEDamageCoefficient);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* TargetActor = Overlap.GetActor();
		if (!TargetActor || TargetActor == SourceActor || TargetActor == DirectTarget || Applied.Contains(TargetActor))
		{
			continue;
		}
		Applied.Add(TargetActor);
		if (!UAscTargetUtil::IsEnemyActor(SourceActor, TargetActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
		if (!TargetASC)
		{
			continue;
		}
		FHitResult Hit(TargetActor, nullptr, TargetActor->GetActorLocation(), FVector::UpVector);
		Hit.TraceStart = ImpactPoint;
		FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
		Ctx.AddSourceObject(SourceObject);
		Ctx.AddInstigator(GetInstigator(), const_cast<AAscenduntProjectile*>(this));
		Ctx.AddOrigin(ImpactPoint);
		Ctx.AddHitResult(Hit, true);
		if (FAscenduntGameplayEffectContext* Typed = FAscenduntGameplayEffectContext::ExtractEffectContext(Ctx))
		{
			Typed->SetAbilitySource(Cast<IAscenduntAbilitySourceInterface>(SourceObject), 1.f);
		}
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Ctx);
		if (!Spec.IsValid())
		{
			continue;
		}
		Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_DamageOverride, SplashDamage);
		Spec.Data->SetSetByCallerMagnitude(AscenduntGameplayTags::Data_Instances, 1.f);
		Spec.Data->AddDynamicAssetTag(AscenduntGameplayTags::Effect_Weapon_AOESplash);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

void AAscenduntProjectile::OnProjectileBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bActive) return;
	if (bHitConsumed) return;

	if (!OtherActor || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!TargetASC || !SourceASC.IsValid())
	{
		return;
	}

	const AActor* SourceActor = AscProjInternal::GetSourceActor(this);
	const bool bFriendly = AscProjInternal::IsFriendly(SourceActor, OtherActor);

	if (bFriendly)
	{
		// 딜 전용이면 아군 관통
		if (!bCanHeal || !HealEffectClass)
		{
			Collision->IgnoreComponentWhenMoving(OtherComp, true);
			return;
		}

		// 힐 가능한데 풀피면 관통
		if (AscProjInternal::IsFullHealth(OtherActor))
		{
			Collision->IgnoreComponentWhenMoving(OtherComp, true);
			return;
		}

		// A valid ally heal impact counts as a weapon hit for the generic accuracy statistic.
		if (UAscCombatContributionComponent* Contribution = AscProjInternal::ResolveCombatContribution(SourceASC.Get()))
		{
			Contribution->RecordWeaponHit(false);
		}

		// 힐 적용 후 종료
		ApplyHealToTarget(TargetASC, SweepResult);
		ReleaseToPool();
		return;
	}

	// 적이면 직접 피해 후 같은 impact point에서 AOE 피해를 적용한다.
	if (UAscCombatContributionComponent* Contribution = AscProjInternal::ResolveCombatContribution(SourceASC.Get()))
	{
		Contribution->RecordWeaponHit(AscProjInternal::IsCriticalWeaponHit(SourceASC.Get(), OtherComp));
	}

	ApplyDamageToTarget(TargetASC, SweepResult);
	const FVector AOEImpactPoint = SweepResult.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(SweepResult.ImpactPoint);
	ApplyAOEDamageAtLocation(AOEImpactPoint, OtherActor);
	ReleaseToPool();
}

void AAscenduntProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || !bActive) return;
	if (bHitConsumed) return;

	// 연출을 먼저 쏜다. ConsumeAndMaybeEnd 가 bReturnToPoolOnWorldHit 에 따라
	// 이 액터를 즉시 풀로 반환할 수 있으므로 순서를 바꾸면 큐가 유실된다.
	PlayWorldImpactCue(ImpactResult);
	const FVector AOEImpactPoint = ImpactResult.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(ImpactResult.ImpactPoint);
	ApplyAOEDamageAtLocation(AOEImpactPoint, nullptr);

	// 월드/구조물 Blocking만 처리
	ConsumeAndMaybeEnd(bReturnToPoolOnWorldHit);
}

void AAscenduntProjectile::PlayWorldImpactCue(const FHitResult& Hit) const
{
	UAscenduntAbilitySystemComponent* AscASC = Cast<UAscenduntAbilitySystemComponent>(SourceASC.Get());
	if (!AscASC || !AscASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	FGameplayCueParameters P;
	P.Instigator = AscASC->GetAvatarActor();
	P.EffectCauser = Cast<AActor>(SourceObject);
	P.SourceObject = SourceObject;
	P.Location = Hit.ImpactPoint;
	P.Normal = Hit.ImpactNormal;
	P.PhysicalMaterial = Hit.PhysMaterial.Get();
	if (RepInit.bAOEImpact)
	{
		P.AggregatedSourceTags.AddTag(AscenduntGameplayTags::Effect_Weapon_AOEImpact);
	}

	FGameplayEffectContextHandle Ctx = AscASC->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject);

	// 히트스캔은 Origin 에 TraceStart 를 넣는다. 투사체에는 트레이스가 없으므로
	// 발사체가 마지막으로 있던 위치를 대신 넣어 큐가 방향을 계산할 수 있게 한다.
	Ctx.AddOrigin(GetActorLocation());
	Ctx.AddHitResult(Hit, true);
	P.EffectContext = Ctx;

	AscASC->ExecuteGameplayCue(AscenduntGameplayTags::GameplayCue_Weapon_ImpactWorld, P);
}

void AAscenduntProjectile::ReleaseToPool()
{
	if (PoolSubsystem)
	{
		PoolSubsystem->Release(this);
	}
	else
	{
		Destroy();
	}
}

void AAscenduntProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAscenduntProjectile, bActive);
	DOREPLIFETIME(AAscenduntProjectile, RepInit);
}
