#include "HakukoFoxComponent.h"

#include "ASCENDUNT/AscenduntGameplayTags.h"

#include "ASCENDUNT/AbilitySystem/Utility/AscTargetUtil.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoCharacter.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoFoxActor.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

UHakukoFoxComponent::UHakukoFoxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UHakukoFoxComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UHakukoFoxComponent, AttackFoxTarget, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHakukoFoxComponent, SupportFoxTarget, COND_OwnerOnly);
}

void UHakukoFoxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeinitializeForGameplay();
	Super::EndPlay(EndPlayReason);
}

void UHakukoFoxComponent::InitializeForGameplay()
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

	if (UAscenduntHealthComponent* HealthComponent = Hakuko->GetAscenduntHealthComponent())
	{
		HealthComponent->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::HandleOwnerDeathStarted);
	}

	if (Hakuko->HasAuthority())
	{
		SpawnPersistentFoxes_Server();
		AssignFox_Server(EHakukoFoxType::Attack, Hakuko);
		AssignFox_Server(EHakukoFoxType::Support, Hakuko);
	}

	SetComponentTickEnabled(Hakuko->HasAuthority() || ShouldRunLocalTargeting());

	if (ShouldRunLocalTargeting())
	{
		OnSelectedFoxChanged.Broadcast(SelectedFoxType);
	}
}

void UHakukoFoxComponent::DeinitializeForGameplay()
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();

	SetLocalTarget(nullptr);

	if (Hakuko)
	{
		if (UAscenduntHealthComponent* HealthComponent = Hakuko->GetAscenduntHealthComponent())
		{
			HealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleOwnerDeathStarted);
		}

		if (Hakuko->HasAuthority())
		{
			ClearAllFoxEffects_Server();
			DestroyPersistentFoxes_Server();
		}
	}

	bGameplayInitialized = false;
	SetComponentTickEnabled(false);
}

void UHakukoFoxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !bGameplayInitialized)
	{
		return;
	}

	if (ShouldRunLocalTargeting())
	{
		UpdateLocalTarget();
	}

	if (Hakuko->HasAuthority())
	{
		ValidateAssignedTargets_Server();
	}
}

void UHakukoFoxComponent::SelectFox(EHakukoFoxType NewFoxType)
{
	if (!ShouldRunLocalTargeting() || SelectedFoxType == NewFoxType)
	{
		return;
	}

	SelectedFoxType = NewFoxType;
	OnSelectedFoxChanged.Broadcast(SelectedFoxType);
	if (LocalTargetActor.IsValid())
	{
		OnTargetChanged.Broadcast(LocalTargetActor.Get(), SelectedFoxType);
	}
}

void UHakukoFoxComponent::RequestSendSelectedFox()
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->IsLocallyControlled())
	{
		return;
	}

	// E with no valid target recalls the selected fox to Hakuko.
	AActor* RequestedTarget = LocalTargetActor.IsValid() ? LocalTargetActor.Get() : Hakuko;
	ServerAssignSelectedFox(SelectedFoxType, RequestedTarget);
}

void UHakukoFoxComponent::ActivateGlobalUltimate()
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->HasAuthority())
	{
		return;
	}

	AAscenduntPlayerState* OwnerPS = GetOwnerPlayerState();
	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!OwnerPS || !GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AAscenduntPlayerState* TargetPS = Cast<AAscenduntPlayerState>(PlayerState);
		if (!TargetPS || TargetPS->GetTeamId() != OwnerPS->GetTeamId() ||
			TargetPS->GetRespawnState() != EAscPlayerRespawnState::Alive)
		{
			continue;
		}

		APawn* TargetPawn = TargetPS->GetPawn();
		if (!IsValid(TargetPawn) || !IsTargetAlive(TargetPawn))
		{
			continue;
		}

		// Each fox type is handled independently. A teammate already carrying a
		// normal fox does not receive the same ultimate fox or its temporary visual.
		if (ApplyUltimateEffectToTarget_Server(TargetPawn, EHakukoFoxType::Attack))
		{
			SpawnFoxActor_Server(
				AttackFoxActorClass,
				EHakukoFoxType::Attack,
				TargetPawn,
				UltimateVisualDuration);
		}

		if (ApplyUltimateEffectToTarget_Server(TargetPawn, EHakukoFoxType::Support))
		{
			SpawnFoxActor_Server(
				SupportFoxActorClass,
				EHakukoFoxType::Support,
				TargetPawn,
				UltimateVisualDuration);
		}
	}
}

FText UHakukoFoxComponent::GetSelectedFoxDisplayText() const
{
	return SelectedFoxType == EHakukoFoxType::Attack
		? FText::FromString(TEXT("공격 여우"))
		: FText::FromString(TEXT("수호 여우"));
}

AActor* UHakukoFoxComponent::GetAssignedTarget(EHakukoFoxType FoxType) const
{
	return FoxType == EHakukoFoxType::Attack ? AttackFoxTarget.Get() : SupportFoxTarget.Get();
}

void UHakukoFoxComponent::ServerAssignSelectedFox_Implementation(EHakukoFoxType FoxType, AActor* RequestedTarget)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko)
	{
		return;
	}

	AActor* ResolvedTarget = IsValid(RequestedTarget) ? RequestedTarget : Hakuko;
	if (!IsValidRecipient_Server(ResolvedTarget, true))
	{
		return;
	}

	AssignFox_Server(FoxType, ResolvedTarget);
}

void UHakukoFoxComponent::HandleOwnerDeathStarted(AActor* OwningActor)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || OwningActor != Hakuko)
	{
		return;
	}

	SetLocalTarget(nullptr);
	SetComponentTickEnabled(false);

	if (Hakuko->HasAuthority())
	{
		ClearAllFoxEffects_Server();
		DestroyPersistentFoxes_Server();
	}

	bGameplayInitialized = false;
}

void UHakukoFoxComponent::OnRep_AttackFoxTarget()
{
	OnFoxAssignmentChanged.Broadcast(EHakukoFoxType::Attack, AttackFoxTarget);
}

void UHakukoFoxComponent::OnRep_SupportFoxTarget()
{
	OnFoxAssignmentChanged.Broadcast(EHakukoFoxType::Support, SupportFoxTarget);
}

AHakukoCharacter* UHakukoFoxComponent::GetHakukoOwner() const
{
	return Cast<AHakukoCharacter>(GetOwner());
}

AAscenduntPlayerState* UHakukoFoxComponent::GetOwnerPlayerState() const
{
	const AHakukoCharacter* Hakuko = GetHakukoOwner();
	return Hakuko ? Hakuko->GetPlayerState<AAscenduntPlayerState>() : nullptr;
}

bool UHakukoFoxComponent::ShouldRunLocalTargeting() const
{
	const AHakukoCharacter* Hakuko = GetHakukoOwner();
	return Hakuko && Hakuko->GetNetMode() != NM_DedicatedServer && Hakuko->IsLocallyControlled();
}

void UHakukoFoxComponent::UpdateLocalTarget()
{
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	SetLocalTarget(FindBestFriendlyTarget(ViewLocation, ViewForward));
}

void UHakukoFoxComponent::SetLocalTarget(AActor* NewTarget)
{
	if (LocalTargetActor.Get() == NewTarget)
	{
		return;
	}

	LocalTargetActor = NewTarget;
	OnTargetChanged.Broadcast(NewTarget, SelectedFoxType);
}

AActor* UHakukoFoxComponent::FindBestFriendlyTarget(FVector& OutViewLocation, FVector& OutViewForward)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	AAscenduntPlayerState* OwnerPS = GetOwnerPlayerState();
	APlayerController* PC = Hakuko ? Cast<APlayerController>(Hakuko->GetController()) : nullptr;
	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Hakuko || !OwnerPS || !PC || !GameState)
	{
		return nullptr;
	}

	FRotator ViewRotation;
	PC->GetPlayerViewPoint(OutViewLocation, ViewRotation);
	OutViewForward = ViewRotation.Vector().GetSafeNormal();

	const float MinDot = FMath::Cos(FMath::DegreesToRadians(TargetingHalfAngleDegrees));
	float BestDot = MinDot;
	float BestDistanceSq = TNumericLimits<float>::Max();
	AActor* BestTarget = nullptr;
	TArray<AActor*> Candidates;
	TSet<TWeakObjectPtr<AActor>> BlockedCandidates;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AAscenduntPlayerState* CandidatePS = Cast<AAscenduntPlayerState>(PlayerState);
		if (!CandidatePS || CandidatePS == OwnerPS || CandidatePS->GetTeamId() != OwnerPS->GetTeamId() ||
			CandidatePS->GetRespawnState() != EAscPlayerRespawnState::Alive)
		{
			continue;
		}

		APawn* CandidatePawn = CandidatePS->GetPawn();
		if (!IsValid(CandidatePawn) || !IsTargetAlive(CandidatePawn))
		{
			continue;
		}

		const FVector TargetPoint = CandidatePawn->GetActorLocation() + TargetPointOffset;
		const FVector ToTarget = TargetPoint - OutViewLocation;
		const float DistanceSq = ToTarget.SizeSquared();
		if (DistanceSq > FMath::Square(TargetingRange) || DistanceSq <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector Direction = ToTarget.GetSafeNormal();
		const float Dot = FVector::DotProduct(OutViewForward, Direction);
		if (Dot < MinDot)
		{
			continue;
		}

		Candidates.Add(CandidatePawn);
		if (bRequireLineOfSight && !IsCandidateVisible(OutViewLocation, CandidatePawn))
		{
			BlockedCandidates.Add(CandidatePawn);
			continue;
		}

		if (Dot > BestDot + KINDA_SMALL_NUMBER ||
			(FMath::IsNearlyEqual(Dot, BestDot) && DistanceSq < BestDistanceSq))
		{
			BestDot = Dot;
			BestDistanceSq = DistanceSq;
			BestTarget = CandidatePawn;
		}
	}

	if (bDrawDebugTargetCone)
	{
		DrawTargetingDebug(OutViewLocation, OutViewForward, Candidates, BestTarget, BlockedCandidates);
	}

	return BestTarget;
}

bool UHakukoFoxComponent::IsCandidateVisible(const FVector& ViewLocation, AActor* Candidate) const
{
	if (!Candidate || !GetWorld())
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HakukoFoxTargetLOS), true);
	Params.AddIgnoredActor(GetOwner());

	const FVector TargetPoint = Candidate->GetActorLocation() + TargetPointOffset;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TargetPoint, LineOfSightTraceChannel, Params);
	return !bHit || Hit.GetActor() == Candidate;
}

void UHakukoFoxComponent::DrawTargetingDebug(const FVector& ViewLocation, const FVector& ViewForward,
	const TArray<AActor*>& Candidates, AActor* BestTarget, const TSet<TWeakObjectPtr<AActor>>& BlockedCandidates) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float ConeRadians = FMath::DegreesToRadians(TargetingHalfAngleDegrees);
	DrawDebugCone(
		World,
		ViewLocation,
		ViewForward,
		TargetingRange,
		ConeRadians,
		ConeRadians,
		DebugConeSegments,
		FColor::Yellow,
		false,
		0.0f,
		0,
		DebugLineThickness);

	for (AActor* Candidate : Candidates)
	{
		if (!Candidate)
		{
			continue;
		}

		const FVector TargetPoint = Candidate->GetActorLocation() + TargetPointOffset;
		FColor Color = FColor::Blue;
		if (BlockedCandidates.Contains(TWeakObjectPtr<AActor>(Candidate)))
		{
			Color = FColor::Red;
		}
		if (Candidate == BestTarget)
		{
			Color = FColor::Green;
		}

		DrawDebugLine(World, ViewLocation, TargetPoint, Color, false, 0.0f, 0, DebugLineThickness);
		DrawDebugSphere(World, TargetPoint, 12.0f, 8, Color, false, 0.0f, 0, DebugLineThickness);
	}
}

void UHakukoFoxComponent::SpawnPersistentFoxes_Server()
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->HasAuthority())
	{
		return;
	}

	if (!IsValid(AttackFoxActor))
	{
		AttackFoxActor = SpawnFoxActor_Server(AttackFoxActorClass, EHakukoFoxType::Attack, Hakuko);
	}
	if (!IsValid(SupportFoxActor))
	{
		SupportFoxActor = SpawnFoxActor_Server(SupportFoxActorClass, EHakukoFoxType::Support, Hakuko);
	}
}

AHakukoFoxActor* UHakukoFoxComponent::SpawnFoxActor_Server(TSubclassOf<AHakukoFoxActor> FoxClass,
	EHakukoFoxType FoxType, AActor* FollowTarget, float LifeSpanSeconds)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	UWorld* World = GetWorld();
	if (!Hakuko || !Hakuko->HasAuthority() || !World || !FoxClass || !FollowTarget)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Hakuko;
	SpawnParams.Instigator = Hakuko;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHakukoFoxActor* FoxActor = World->SpawnActor<AHakukoFoxActor>(FoxClass, FollowTarget->GetActorTransform(), SpawnParams);
	if (FoxActor)
	{
		FoxActor->InitializeFox(FoxType, FollowTarget);
		if (LifeSpanSeconds > 0.0f)
		{
			FoxActor->SetLifeSpan(LifeSpanSeconds);
		}
	}

	return FoxActor;
}

void UHakukoFoxComponent::DestroyPersistentFoxes_Server()
{
	if (IsValid(AttackFoxActor))
	{
		AttackFoxActor->Destroy();
	}
	if (IsValid(SupportFoxActor))
	{
		SupportFoxActor->Destroy();
	}

	AttackFoxActor = nullptr;
	SupportFoxActor = nullptr;
}

bool UHakukoFoxComponent::IsValidRecipient_Server(AActor* TargetActor, bool bAllowOwner) const
{
	const AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !IsValid(TargetActor))
	{
		return false;
	}

	if (TargetActor == Hakuko)
	{
		if (!bAllowOwner)
		{
			return false;
		}

		if (const UAscenduntHealthComponent* HealthComponent =
			UAscenduntHealthComponent::FindHealthComponent(TargetActor))
		{
			if (HealthComponent->IsDeadOrDying())
			{
				return false;
			}
		}

		// GameplayReady는 MatchGameMode가 Alive로 바꾸기 전에 호출될 수 있다.
		if (const AAscenduntPlayerState* PlayerState =
			Hakuko->GetPlayerState<AAscenduntPlayerState>())
		{
			const EAscPlayerRespawnState State =
				PlayerState->GetRespawnState();

			return State == EAscPlayerRespawnState::Alive ||
				State == EAscPlayerRespawnState::Respawning;
		}

		return true;
	}

	if (!UAscTargetUtil::IsFriendlyActor(Hakuko, TargetActor) ||
		!IsTargetAlive(TargetActor))
	{
		return false;
	}

	return FVector::DistSquared(
		Hakuko->GetActorLocation(),
		TargetActor->GetActorLocation())
		<= FMath::Square(TargetingRange + ServerRangeTolerance);
}

bool UHakukoFoxComponent::IsTargetAlive(AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		if (const AAscenduntPlayerState* TargetPS = TargetPawn->GetPlayerState<AAscenduntPlayerState>())
		{
			if (TargetPS->GetRespawnState() != EAscPlayerRespawnState::Alive)
			{
				return false;
			}
		}
	}

	if (const UAscenduntHealthComponent* HealthComponent = UAscenduntHealthComponent::FindHealthComponent(TargetActor))
	{
		return !HealthComponent->IsDeadOrDying();
	}

	return true;
}

void UHakukoFoxComponent::ValidateAssignedTargets_Server()
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko)
	{
		return;
	}

	if (AttackFoxTarget && AttackFoxTarget != Hakuko && !IsValidRecipient_Server(AttackFoxTarget, false))
	{
		RecallFox_Server(EHakukoFoxType::Attack);
	}

	if (SupportFoxTarget && SupportFoxTarget != Hakuko && !IsValidRecipient_Server(SupportFoxTarget, false))
	{
		RecallFox_Server(EHakukoFoxType::Support);
	}
}

void UHakukoFoxComponent::AssignFox_Server(EHakukoFoxType FoxType, AActor* TargetActor)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	if (!Hakuko || !Hakuko->HasAuthority() || !IsValidRecipient_Server(TargetActor, true))
	{
		return;
	}

	AActor* CurrentTarget = FoxType == EHakukoFoxType::Attack ? AttackFoxTarget.Get() : SupportFoxTarget.Get();
	if (CurrentTarget == TargetActor)
	{
		return;
	}

	RemoveFoxEffect_Server(FoxType);
	if (FoxType == EHakukoFoxType::Attack)
	{
		AttackFoxTarget = TargetActor;
	}
	else
	{
		SupportFoxTarget = TargetActor;
	}
	ApplyFoxEffect_Server(FoxType, TargetActor);
	OnFoxAssignmentChanged.Broadcast(FoxType, TargetActor);
	Hakuko->ForceNetUpdate();

	if (AHakukoFoxActor* FoxActor = GetPersistentFoxActor(FoxType))
	{
		FoxActor->SetFollowTarget(TargetActor);
	}
}

void UHakukoFoxComponent::RemoveFoxEffect_Server(EHakukoFoxType FoxType)
{
	TArray<FActiveGameplayEffectHandle>& EffectHandles = FoxType == EHakukoFoxType::Attack
		? AttackFoxEffectHandles
		: SupportFoxEffectHandles;
	TWeakObjectPtr<UAbilitySystemComponent>& EffectASC = FoxType == EHakukoFoxType::Attack
		? AttackFoxEffectASC
		: SupportFoxEffectASC;

	if (EffectASC.IsValid())
	{
		for (const FActiveGameplayEffectHandle& EffectHandle : EffectHandles)
		{
			if (EffectHandle.IsValid())
			{
				EffectASC->RemoveActiveGameplayEffect(EffectHandle);
			}
		}
	}

	EffectHandles.Reset();
	EffectASC.Reset();
}

void UHakukoFoxComponent::ApplyFoxEffect_Server(EHakukoFoxType FoxType, AActor* TargetActor)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hakuko);
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!Hakuko || !SourceASC || !TargetASC)
	{
		return;
	}

	TArray<TSubclassOf<UGameplayEffect>> EffectClasses;
	GetNormalEffectClasses(FoxType, EffectClasses);
	TArray<FActiveGameplayEffectHandle>& EffectHandles = FoxType == EHakukoFoxType::Attack
		? AttackFoxEffectHandles
		: SupportFoxEffectHandles;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Hakuko, Hakuko);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectClasses)
	{
		if (!EffectClass)
		{
			continue;
		}

		const FActiveGameplayEffectHandle Handle = SourceASC->ApplyGameplayEffectToTarget(
			EffectClass->GetDefaultObject<UGameplayEffect>(), TargetASC, 1.0f, Context);
		if (Handle.IsValid())
		{
			EffectHandles.Add(Handle);
		}
	}

	if (FoxType == EHakukoFoxType::Attack)
	{
		AttackFoxEffectASC = TargetASC;
	}
	else
	{
		SupportFoxEffectASC = TargetASC;
	}
}

void UHakukoFoxComponent::RecallFox_Server(EHakukoFoxType FoxType)
{
	if (AHakukoCharacter* Hakuko = GetHakukoOwner())
	{
		AssignFox_Server(FoxType, Hakuko);
	}
}

void UHakukoFoxComponent::ClearAllFoxEffects_Server()
{
	RemoveFoxEffect_Server(EHakukoFoxType::Attack);
	RemoveFoxEffect_Server(EHakukoFoxType::Support);
	AttackFoxTarget = nullptr;
	SupportFoxTarget = nullptr;
	OnFoxAssignmentChanged.Broadcast(EHakukoFoxType::Attack, nullptr);
	OnFoxAssignmentChanged.Broadcast(EHakukoFoxType::Support, nullptr);

	if (AHakukoCharacter* Hakuko = GetHakukoOwner())
	{
		Hakuko->ForceNetUpdate();
	}
}

void UHakukoFoxComponent::GetNormalEffectClasses(EHakukoFoxType FoxType,
	TArray<TSubclassOf<UGameplayEffect>>& OutEffectClasses) const
{
	OutEffectClasses.Reset();
	if (FoxType == EHakukoFoxType::Attack)
	{
		OutEffectClasses.Add(AttackFoxEffectClass);
	}
	else
	{
		OutEffectClasses.Add(SupportFoxEffectClass);
		OutEffectClasses.Add(SupportFoxPeriodicHealEffectClass);
	}
}

void UHakukoFoxComponent::GetUltimateEffectClasses(EHakukoFoxType FoxType,
	TArray<TSubclassOf<UGameplayEffect>>& OutEffectClasses) const
{
	OutEffectClasses.Reset();
	if (FoxType == EHakukoFoxType::Attack)
	{
		OutEffectClasses.Add(UltimateAttackFoxEffectClass);
	}
	else
	{
		OutEffectClasses.Add(UltimateSupportFoxEffectClass);
		OutEffectClasses.Add(UltimateSupportFoxPeriodicHealEffectClass);
	}
}

TSubclassOf<AHakukoFoxActor> UHakukoFoxComponent::GetFoxActorClass(EHakukoFoxType FoxType) const
{
	return FoxType == EHakukoFoxType::Attack ? AttackFoxActorClass : SupportFoxActorClass;
}

AHakukoFoxActor* UHakukoFoxComponent::GetPersistentFoxActor(EHakukoFoxType FoxType) const
{
	return FoxType == EHakukoFoxType::Attack ? AttackFoxActor.Get() : SupportFoxActor.Get();
}

void UHakukoFoxComponent::SetPersistentFoxActor(EHakukoFoxType FoxType, AHakukoFoxActor* FoxActor)
{
	if (FoxType == EHakukoFoxType::Attack)
	{
		AttackFoxActor = FoxActor;
	}
	else
	{
		SupportFoxActor = FoxActor;
	}
}

bool UHakukoFoxComponent::ApplyUltimateEffectToTarget_Server(
	AActor* TargetActor,
	EHakukoFoxType FoxType)
{
	AHakukoCharacter* Hakuko = GetHakukoOwner();
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hakuko);
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!Hakuko || !SourceASC || !TargetASC)
	{
		return false;
	}

	const FGameplayTag ExistingFoxTag = FoxType == EHakukoFoxType::Attack
		? AscenduntGameplayTags::Status_Hakuko_Fox_Attack
		: AscenduntGameplayTags::Status_Hakuko_Fox_Support;

	if (TargetASC->HasMatchingGameplayTag(ExistingFoxTag))
	{
		return false;
	}

	TArray<TSubclassOf<UGameplayEffect>> EffectClasses;
	GetUltimateEffectClasses(FoxType, EffectClasses);

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Hakuko, Hakuko);

	bool bAppliedAnyEffect = false;
	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectClasses)
	{
		if (!EffectClass)
		{
			continue;
		}

		const FActiveGameplayEffectHandle Handle = SourceASC->ApplyGameplayEffectToTarget(
			EffectClass->GetDefaultObject<UGameplayEffect>(),
			TargetASC,
			1.0f,
			Context);

		bAppliedAnyEffect |= Handle.IsValid();
	}

	return bAppliedAnyEffect;
}
