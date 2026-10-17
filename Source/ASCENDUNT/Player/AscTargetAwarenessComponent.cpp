#include "AscTargetAwarenessComponent.h"

#include "ASCENDUNT/Character/BaseCharacter.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UAscTargetAwarenessComponent::UAscTargetAwarenessComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAscTargetAwarenessComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			PruneTimerHandle,
			this,
			&ThisClass::PruneExpiredTargets,
			0.25f,
			true);
	}
}

void UAscTargetAwarenessComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PruneTimerHandle);
	}

	ObservedTargets.Reset();
	OnObservedTargetsChanged.Broadcast();

	Super::EndPlay(EndPlayReason);
}

void UAscTargetAwarenessComponent::GrantTargetReveal(ABaseCharacter* Target, float DurationSeconds)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !IsValid(Target))
	{
		return;
	}

	const float RevealDuration = DurationSeconds > 0.0f ? DurationSeconds : DefaultRevealDuration;
	const float NewRevealEndTime = GetSynchronizedServerTime() + RevealDuration;

	PruneExpiredTargets();

	for (FAscObservedTargetEntry& Entry : ObservedTargets)
	{
		if (Entry.Target == Target)
		{
			Entry.RevealEndServerTime = FMath::Max(Entry.RevealEndServerTime, NewRevealEndTime);
			OwnerActor->ForceNetUpdate();
			return;
		}
	}

	FAscObservedTargetEntry& NewEntry = ObservedTargets.AddDefaulted_GetRef();
	NewEntry.Target = Target;
	NewEntry.RevealEndServerTime = NewRevealEndTime;
	OwnerActor->ForceNetUpdate();
}

bool UAscTargetAwarenessComponent::IsTargetRevealed(const ABaseCharacter* Target) const
{
	return GetTargetRevealRemainingTime(Target) > 0.0f;
}

float UAscTargetAwarenessComponent::GetTargetRevealRemainingTime(const ABaseCharacter* Target) const
{
	if (!IsValid(Target))
	{
		return 0.0f;
	}

	const float CurrentServerTime = GetSynchronizedServerTime();
	for (const FAscObservedTargetEntry& Entry : ObservedTargets)
	{
		if (Entry.Target == Target)
		{
			return FMath::Max(0.0f, Entry.RevealEndServerTime - CurrentServerTime);
		}
	}

	return 0.0f;
}

void UAscTargetAwarenessComponent::OnRep_ObservedTargets()
{
	OnObservedTargetsChanged.Broadcast();
}

void UAscTargetAwarenessComponent::PruneExpiredTargets()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	const float CurrentServerTime = GetSynchronizedServerTime();
	const int32 RemovedCount = ObservedTargets.RemoveAllSwap(
		[CurrentServerTime](const FAscObservedTargetEntry& Entry)
		{
			if (!IsValid(Entry.Target) || Entry.RevealEndServerTime <= CurrentServerTime || Entry.Target->IsHidden())
			{
				return true;
			}

			const UAscenduntHealthComponent* HealthComponent = Entry.Target->GetAscenduntHealthComponent();
			return HealthComponent && HealthComponent->IsDeadOrDying();
		},
		EAllowShrinking::No);

	if (RemovedCount > 0)
	{
		OwnerActor->ForceNetUpdate();
	}
}

float UAscTargetAwarenessComponent::GetSynchronizedServerTime() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void UAscTargetAwarenessComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UAscTargetAwarenessComponent, ObservedTargets, COND_OwnerOnly);
}
