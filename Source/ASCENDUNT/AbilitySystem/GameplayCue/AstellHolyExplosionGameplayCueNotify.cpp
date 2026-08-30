#include "AstellHolyExplosionGameplayCueNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

bool UAstellHolyExplosionGameplayCueNotify::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget || !HolyExplosionVFX)
	{
		return false;
	}

	UWorld* World = MyTarget->GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	USkeletalMeshComponent* MeshComponent = nullptr;
	if (ACharacter* Character = Cast<ACharacter>(MyTarget))
	{
		MeshComponent = Character->GetMesh();
	}
	if (!MeshComponent)
	{
		MeshComponent = MyTarget->FindComponentByClass<USkeletalMeshComponent>();
	}
	if (!MeshComponent)
	{
		return false;
	}

	const FName AttachSocket = !AttachSocketName.IsNone() && MeshComponent->DoesSocketExist(AttachSocketName)
		? AttachSocketName
		: NAME_None;

	UNiagaraComponent* BurstComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		HolyExplosionVFX,
		MeshComponent,
		AttachSocket,
		RelativeLocation,
		RelativeRotation,
		RelativeScale,
		EAttachLocation::KeepRelativeOffset,
		true,
		ENCPoolMethod::AutoRelease,
		true,
		true);

	return BurstComponent != nullptr;
}
