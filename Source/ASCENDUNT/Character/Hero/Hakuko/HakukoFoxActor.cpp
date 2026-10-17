#include "HakukoFoxActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"

AHakukoFoxActor::AHakukoFoxActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);
	NetUpdateFrequency = 10.0f;
	MinNetUpdateFrequency = 2.0f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	AuraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AuraComponent"));
	AuraComponent->SetupAttachment(SceneRoot);
	AuraComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AuraComponent->SetGenerateOverlapEvents(false);
	AuraComponent->SetIsReplicated(false);
}

void AHakukoFoxActor::BeginPlay()
{
	Super::BeginPlay();

	SetActorEnableCollision(false);

	OnRep_FollowTarget();
}

void AHakukoFoxActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsValid(FollowTarget))
	{
		return;
	}

	const FVector DesiredLocation = ResolveDesiredLocation();
	const FRotator DesiredRotation = ResolveDesiredRotation();

	if (!bLocationInitialized)
	{
		SnapToFollowTarget();
		return;
	}

	FVector NewLocation = GetActorLocation();
	if (bTravellingToFollowTarget)
	{
		if (TransferTravelSpeed <= KINDA_SMALL_NUMBER)
		{
			NewLocation = DesiredLocation;
			bTravellingToFollowTarget = false;
		}
		else
		{
			NewLocation = FMath::VInterpConstantTo(
				GetActorLocation(),
				DesiredLocation,
				DeltaSeconds,
				TransferTravelSpeed);

			if (FVector::DistSquared(NewLocation, DesiredLocation) <= FMath::Square(TransferArrivalTolerance))
			{
				NewLocation = DesiredLocation;
				bTravellingToFollowTarget = false;
			}
		}
	}
	else if (FollowInterpSpeed <= KINDA_SMALL_NUMBER)
	{
		NewLocation = DesiredLocation;
	}
	else
	{
		NewLocation = FMath::VInterpTo(
			GetActorLocation(),
			DesiredLocation,
			DeltaSeconds,
			FollowInterpSpeed);
	}

	const FRotator NewRotation = FMath::RInterpTo(
		GetActorRotation(),
		DesiredRotation,
		DeltaSeconds,
		FollowInterpSpeed);

	// No sweep: the fox is cosmetic and intentionally passes through walls.
	SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::None);
}

void AHakukoFoxActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHakukoFoxActor, FoxType);
	DOREPLIFETIME(AHakukoFoxActor, FollowTarget);
}

void AHakukoFoxActor::InitializeFox(EHakukoFoxType InFoxType, AActor* InFollowTarget)
{
	if (!HasAuthority())
	{
		return;
	}

	FoxType = InFoxType;
	FollowTarget = InFollowTarget;
	bLocationInitialized = false;
	bTravellingToFollowTarget = false;
	SnapToFollowTarget();
	ForceNetUpdate();
}

void AHakukoFoxActor::SetFollowTarget(AActor* NewFollowTarget)
{
	if (!HasAuthority() || FollowTarget == NewFollowTarget)
	{
		return;
	}

	FollowTarget = NewFollowTarget;
	bTravellingToFollowTarget = IsValid(FollowTarget) && bLocationInitialized;
	ForceNetUpdate();
}

void AHakukoFoxActor::OnRep_FollowTarget()
{
	if (!IsValid(FollowTarget))
	{
		bTravellingToFollowTarget = false;
		return;
	}

	// Initial replication snaps the fox beside Hakuko. Later target changes keep
	// the current world position and visibly fly toward the newly assigned ally.
	if (!bLocationInitialized)
	{
		SnapToFollowTarget();
	}
	else
	{
		bTravellingToFollowTarget = true;
	}
}

void AHakukoFoxActor::SnapToFollowTarget()
{
	if (!IsValid(FollowTarget))
	{
		return;
	}

	SetActorLocationAndRotation(
		ResolveDesiredLocation(),
		ResolveDesiredRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	bLocationInitialized = true;
	bTravellingToFollowTarget = false;
}

FVector AHakukoFoxActor::ResolveDesiredLocation() const
{
	if (!IsValid(FollowTarget))
	{
		return GetActorLocation();
	}

	const FRotator YawRotation(0.0f, FollowTarget->GetActorRotation().Yaw, 0.0f);
	return FollowTarget->GetActorLocation() + YawRotation.RotateVector(FollowOffset);
}

FRotator AHakukoFoxActor::ResolveDesiredRotation() const
{
	if (!IsValid(FollowTarget))
	{
		return RotationOffset;
	}

	const float TargetYaw = bMatchTargetYaw ? FollowTarget->GetActorRotation().Yaw : 0.0f;
	return FRotator(0.0f, TargetYaw, 0.0f) + RotationOffset;
}
