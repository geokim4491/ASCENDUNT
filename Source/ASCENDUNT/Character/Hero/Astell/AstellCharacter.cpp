#include "AstellCharacter.h"

#include "ASCENDUNT/AbilitySystem/Components/AscSkillStockComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AAstellCharacter::AAstellCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SkillStockComponent = CreateDefaultSubobject<UAscSkillStockComponent>(TEXT("SkillStockComponent"));

	// Astell uses the common GA_Jump input ability, while ACharacter handles the actual jump count.
	JumpMaxCount = 2;
}

void AAstellCharacter::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMeshComponent* MeshComp = GetMesh();
}

void AAstellCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	constexpr int32 DoubleJumpCount = 2;
	if (JumpCurrentCount != DoubleJumpCount)
	{
		return;
	}

	// The autonomous proxy applies the launch immediately for prediction, and the server
	// applies the authoritative launch. Simulated proxies receive the resulting movement.
	if (IsLocallyControlled() || HasAuthority())
	{
		ApplyDoubleJumpLaunch();
	}

	const FVector VFXLocation = GetDoubleJumpVFXLocation();
	const FRotator VFXRotation = GetDoubleJumpVFXRotation();

	// The owning client plays immediately for input responsiveness.
	if (IsLocallyControlled())
	{
		PlayDoubleJumpCosmeticsLocal(VFXLocation, VFXRotation);
	}

	// The server forwards cosmetic playback and the authoritative platform location to
	// simulated proxies. The owning client ignores the multicast to avoid duplication.
	if (HasAuthority())
	{
		MulticastPlayDoubleJumpCosmetics(VFXLocation, VFXRotation);
	}
}

void AAstellCharacter::ApplyDoubleJumpLaunch()
{
	// Preserve horizontal momentum and explicitly replace only the vertical velocity.
	// This makes the movement independent from montage root motion.
	LaunchCharacter(
		FVector(0.0f, 0.0f, DoubleJumpVerticalVelocity),
		false,
		true);
}

void AAstellCharacter::PlayDoubleJumpCosmeticsLocal(const FVector& VFXLocation, const FRotator& VFXRotation)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (DoubleJumpMontage && AnimInstance)
	{
		AnimInstance->Montage_Play(DoubleJumpMontage);
	}

	if (DoubleJumpVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			DoubleJumpVFX,
			VFXLocation,
			VFXRotation);
	}
}

FVector AAstellCharacter::GetDoubleJumpVFXLocation() const
{
	FVector Origin = GetActorLocation();

	const USkeletalMeshComponent* MeshComp = GetMesh();
	if (!DoubleJumpVFXSocketName.IsNone() && MeshComp && MeshComp->DoesSocketExist(DoubleJumpVFXSocketName))
	{
		Origin = MeshComp->GetSocketLocation(DoubleJumpVFXSocketName);
	}
	else if (const UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		Origin -= FVector::UpVector * CapsuleComp->GetScaledCapsuleHalfHeight();
	}

	return Origin + GetActorTransform().TransformVectorNoScale(DoubleJumpVFXLocationOffset);
}

FRotator AAstellCharacter::GetDoubleJumpVFXRotation() const
{
	return FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
}

void AAstellCharacter::MulticastPlayDoubleJumpCosmetics_Implementation(FVector_NetQuantize10 VFXLocation, FRotator VFXRotation)
{
	if (IsLocallyControlled())
	{
		return;
	}

	PlayDoubleJumpCosmeticsLocal(VFXLocation, VFXRotation);
}
