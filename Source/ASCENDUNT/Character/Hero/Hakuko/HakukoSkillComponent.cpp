#include "HakukoSkillComponent.h"

#include "ASCENDUNT/AbilitySystem/Components/AscCombatContributionComponent.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoCharacter.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

UHakukoSkillComponent::UHakukoSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UHakukoSkillComponent::InitializeForGameplay()
{
	if (bGameplayInitialized)
	{
		return;
	}

	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko)
	{
		return;
	}
	bGameplayInitialized = true;

	if (Hakuko->HasAuthority())
	{
		if (AAscenduntPlayerState* PS = Hakuko->GetPlayerState<AAscenduntPlayerState>())
		{
			if (UAscCombatContributionComponent* Contribution = PS->GetCombatContributionComponent())
			{
				BoundContribution = Contribution;
				DamageDealtDelegateHandle = Contribution->OnDamageDealtNative.AddUObject(this, &ThisClass::HandleDamageDealt);
			}
		}
	}

	if (UWeaponEquipmentComponent* Equipment = Hakuko->GetWeaponEquipmentComponent())
	{
		WeaponChangedDelegateHandle = Equipment->OnCurrentWeaponChanged.AddUObject(this, &ThisClass::HandleWeaponChanged);
	}

	SetComponentTickEnabled(ShouldRunLocalTargeting());
	RefreshUltimateWeaponVFX();
}

void UHakukoSkillComponent::DeinitializeForGameplay()
{
	if (UAscCombatContributionComponent* Contribution = BoundContribution.Get())
	{
		if (DamageDealtDelegateHandle.IsValid())
		{
			Contribution->OnDamageDealtNative.Remove(DamageDealtDelegateHandle);
		}
	}
	BoundContribution.Reset();
	DamageDealtDelegateHandle.Reset();

	if (AHakukoCharacter* Hakuko = GetHakukoOwner())
	{
		if (UWeaponEquipmentComponent* Equipment = Hakuko->GetWeaponEquipmentComponent())
		{
			if (WeaponChangedDelegateHandle.IsValid())
			{
				Equipment->OnCurrentWeaponChanged.Remove(WeaponChangedDelegateHandle);
			}
		}
		if (Hakuko->HasAuthority())
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(UltimateTimer);
			}
			DeactivateUltimate_Server();
		}
	}
	WeaponChangedDelegateHandle.Reset();

	SetLocalTarget(nullptr);
	ClearUltimateWeaponVFX();
	SetComponentTickEnabled(false);
	bGameplayInitialized = false;
}

void UHakukoSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeinitializeForGameplay();
	Super::EndPlay(EndPlayReason);
}

void UHakukoSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bGameplayInitialized && ShouldRunLocalTargeting())
	{
		UpdateLocalTarget();
	}
}

void UHakukoSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UHakukoSkillComponent, HealGrenadeGauge, COND_OwnerOnly);
	DOREPLIFETIME(UHakukoSkillComponent, bUltimateActive);
	DOREPLIFETIME(UHakukoSkillComponent, UltimateAOERadius);
	DOREPLIFETIME(UHakukoSkillComponent, UltimateAOEDamageCoefficient);
	DOREPLIFETIME(UHakukoSkillComponent, UltimateWeaponVFX);
}

AHakukoCharacter* UHakukoSkillComponent::GetHakukoOwner() const
{
	return Cast<AHakukoCharacter>(GetOwner());
}

bool UHakukoSkillComponent::ShouldRunLocalTargeting() const
{
	const AHakukoCharacter* Hakuko = GetHakukoOwner();
	return Hakuko && Hakuko->GetNetMode() != NM_DedicatedServer && Hakuko->IsLocallyControlled();
}

void UHakukoSkillComponent::UpdateLocalTarget()
{
	SetLocalTarget(FindBestFriendlyTarget(false));
}

void UHakukoSkillComponent::SetLocalTarget(AActor* NewTarget)
{
	if (LocalStimTargetActor.Get() == NewTarget)
	{
		return;
	}
	LocalStimTargetActor = NewTarget;
	OnStimTargetChanged.Broadcast(NewTarget);
}

AActor* UHakukoSkillComponent::FindBestFriendlyTarget_Server() const
{
	const AHakukoCharacter* Hakuko = GetHakukoOwner();
	return Hakuko && Hakuko->HasAuthority() ? FindBestFriendlyTarget(true) : nullptr;
}

AActor* UHakukoSkillComponent::FindBestFriendlyTarget(bool bServerQuery) const
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	AAscenduntPlayerState* OwnerPS = Hakuko ? Hakuko->GetPlayerState<AAscenduntPlayerState>() : nullptr;
	APlayerController* PC = Hakuko ? Cast<APlayerController>(Hakuko->GetController()) : nullptr;
	AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Hakuko || !OwnerPS || !PC || !GS)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (bServerQuery)
	{
		// Dedicated Server has no local camera. ControlRotation is the authoritative replicated aim.
		ViewLocation = Hakuko->GetPawnViewLocation();
		ViewRotation = PC->GetControlRotation();
	}
	else
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(TargetingHalfAngleDegrees));

	float BestDot = MinDot;
	float BestDistanceSq = TNumericLimits<float>::Max();
	AActor* BestTarget = nullptr;

	for (APlayerState* PSBase : GS->PlayerArray)
	{
		AAscenduntPlayerState* CandidatePS = Cast<AAscenduntPlayerState>(PSBase);
		if (!CandidatePS || CandidatePS == OwnerPS || CandidatePS->GetTeamId() != OwnerPS->GetTeamId() ||
			CandidatePS->GetRespawnState() != EAscPlayerRespawnState::Alive)
		{
			continue;
		}

		APawn* Candidate = CandidatePS->GetPawn();
		if (!IsValid(Candidate))
		{
			continue;
		}
		if (const UAscenduntHealthComponent* HC = UAscenduntHealthComponent::FindHealthComponent(Candidate))
		{
			if (HC->IsDeadOrDying())
			{
				continue;
			}
		}

		const FVector TargetPoint = Candidate->GetActorLocation() + TargetPointOffset;
		const FVector ToTarget = TargetPoint - ViewLocation;
		const float DistanceSq = ToTarget.SizeSquared();
		if (DistanceSq <= KINDA_SMALL_NUMBER || DistanceSq > FMath::Square(TargetingRange))
		{
			continue;
		}
		const float Dot = FVector::DotProduct(ViewForward, ToTarget.GetSafeNormal());
		if (Dot < MinDot)
		{
			continue;
		}
		if (bRequireLineOfSight && !IsCandidateVisible(ViewLocation, Candidate))
		{
			continue;
		}

		if (Dot > BestDot + KINDA_SMALL_NUMBER || (FMath::IsNearlyEqual(Dot, BestDot) && DistanceSq < BestDistanceSq))
		{
			BestDot = Dot;
			BestDistanceSq = DistanceSq;
			BestTarget = Candidate;
		}
	}
	return BestTarget;
}

bool UHakukoSkillComponent::IsCandidateVisible(const FVector& ViewLocation, AActor* Candidate) const
{
	if (!Candidate || !GetWorld())
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HakukoStimTargetLOS), true);
	Params.AddIgnoredActor(GetOwner());
	const FVector TargetPoint = Candidate->GetActorLocation() + TargetPointOffset;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TargetPoint, LineOfSightTraceChannel, Params);
	return !bHit || Hit.GetActor() == Candidate;
}

void UHakukoSkillComponent::HandleDamageDealt(float ActualDamage)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->HasAuthority() || ActualDamage <= 0.f)
	{
		return;
	}
	const float Gain = ActualDamage / FMath::Max(1.f, DamageForFullHealGauge) * 100.f;
	HealGrenadeGauge = FMath::Clamp(HealGrenadeGauge + Gain, 0.f, 100.f);
	OnHealGrenadeGaugeChanged.Broadcast(HealGrenadeGauge);
}

float UHakukoSkillComponent::CalculateHealGrenadeAmount(float MinHeal, float MaxHeal) const
{
	const float Alpha = FMath::Clamp(HealGrenadeGauge / 100.f, 0.f, 1.f);
	const float LowHeal = FMath::Min(MinHeal, MaxHeal);
	const float HighHeal = FMath::Max(MinHeal, MaxHeal);
	return FMath::Lerp(LowHeal, HighHeal, Alpha);
}

void UHakukoSkillComponent::ResetHealGrenadeGauge_Server()
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->HasAuthority())
	{
		return;
	}
	HealGrenadeGauge = 0.f;
	OnHealGrenadeGaugeChanged.Broadcast(HealGrenadeGauge);
}

void UHakukoSkillComponent::ActivateUltimate_Server(float Duration, float AOERadius, float AOEDamageCoefficient, UNiagaraSystem* WeaponVFX, bool bDebugDrawAOERadius, float AOEDebugDrawDuration)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->HasAuthority())
	{
		return;
	}
	bUltimateActive = true;
	UltimateAOERadius = FMath::Max(0.f, AOERadius);
	UltimateAOEDamageCoefficient = FMath::Max(0.f, AOEDamageCoefficient);
	bUltimateDebugDrawAOERadius = bDebugDrawAOERadius;
	UltimateAOEDebugDrawDuration = FMath::Max(0.f, AOEDebugDrawDuration);
	UltimateWeaponVFX = WeaponVFX;
	OnUltimateStateChanged.Broadcast(true);
	RefreshUltimateWeaponVFX();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UltimateTimer);
		if (Duration > 0.f)
		{
			World->GetTimerManager().SetTimer(UltimateTimer, this, &ThisClass::DeactivateUltimate_Server, Duration, false);
		}
	}
	Hakuko->ForceNetUpdate();
}

void UHakukoSkillComponent::DeactivateUltimate_Server()
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->HasAuthority())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UltimateTimer);
	}
	bUltimateActive = false;
	UltimateAOERadius = 0.f;
	UltimateAOEDamageCoefficient = 1.f;
	bUltimateDebugDrawAOERadius = false;
	UltimateAOEDebugDrawDuration = 0.f;
	UltimateWeaponVFX = nullptr;

	// The ASC lives on PlayerState, so explicitly remove the duration buff when this Pawn
	// deinitializes/dies instead of allowing the old hero's ultimate modifier to survive respawn.
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hakuko))
	{
		FGameplayTagContainer UltimateTags;
		UltimateTags.AddTag(AscenduntGameplayTags::Status_Hakuko_Ultimate);
		ASC->RemoveActiveEffectsWithGrantedTags(UltimateTags);
	}
	OnUltimateStateChanged.Broadcast(false);
	ClearUltimateWeaponVFX();
	Hakuko->ForceNetUpdate();
}

bool UHakukoSkillComponent::GetUltimateAOEParameters(
	float& OutRadius,
	float& OutDamageCoefficient,
	bool& bOutDebugDrawRadius,
	float& OutDebugDrawDuration) const
{
	OutRadius = UltimateAOERadius;
	OutDamageCoefficient = UltimateAOEDamageCoefficient;
	bOutDebugDrawRadius = bUltimateDebugDrawAOERadius;
	OutDebugDrawDuration = UltimateAOEDebugDrawDuration;
	return bUltimateActive && UltimateAOERadius > KINDA_SMALL_NUMBER;
}

void UHakukoSkillComponent::OnRep_HealGrenadeGauge()
{
	OnHealGrenadeGaugeChanged.Broadcast(HealGrenadeGauge);
}

void UHakukoSkillComponent::OnRep_UltimateState()
{
	OnUltimateStateChanged.Broadcast(bUltimateActive);
	RefreshUltimateWeaponVFX();
}

void UHakukoSkillComponent::HandleWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PreviousWeapon)
{
	RefreshUltimateWeaponVFX();
}

void UHakukoSkillComponent::RefreshUltimateWeaponVFX()
{
	ClearUltimateWeaponVFX();
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!bUltimateActive || !UltimateWeaponVFX || !Hakuko || Hakuko->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UWeaponEquipmentComponent* Equipment = Hakuko->GetWeaponEquipmentComponent();
	AAscenduntWeapon* Weapon = Equipment ? Equipment->GetCurrentWeapon() : nullptr;
	UMeshComponent* Mesh = Weapon ? Weapon->GetActiveWeaponMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	UltimateWeaponVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		UltimateWeaponVFX, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::None, true);
}

void UHakukoSkillComponent::ClearUltimateWeaponVFX()
{
	if (UltimateWeaponVFXComponent)
	{
		UltimateWeaponVFXComponent->Deactivate();
		UltimateWeaponVFXComponent->DestroyComponent();
		UltimateWeaponVFXComponent = nullptr;
	}
}
