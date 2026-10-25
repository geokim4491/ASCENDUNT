#include "AscWeaponFireGameplayCueNotify_Burst.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "GameFramework/Pawn.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"

UAscWeaponFireGameplayCueNotify_Burst::UAscWeaponFireGameplayCueNotify_Burst()
{
	bExecuteBlueprintCue = false;
}

bool UAscWeaponFireGameplayCueNotify_Burst::ExecuteCueNative(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(const_cast<UObject*>(Parameters.GetSourceObject())))
	{
		Weapon->PlayFirePresentation(FMath::Max(0.01f, Parameters.RawMagnitude));
		if (USoundCue* Sound = Weapon->GetCurrentModeStats().FireSoundSFX)
		{
			const APawn* Pawn = Cast<APawn>(Weapon->GetOwner());
			const bool bOwner = Pawn && Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled();
			if (bOwner)
			{
				UGameplayStatics::PlaySound2D(Weapon, Sound);
			}
			else
			{
				UGameplayStatics::PlaySoundAtLocation(Weapon, Sound, Weapon->GetMuzzleVFXTransform().GetLocation());
			}
		}

		const auto& Stats = Weapon->GetCurrentModeStats();
		const FTransform Muzzle = Weapon->GetMuzzleVFXTransform();
		if (Stats.FireVFXNiagara)
		{
			if (USkeletalMeshComponent* Mesh = Weapon->GetMuzzleAttachMesh())
			{
				UNiagaraFunctionLibrary::SpawnSystemAttached(Stats.FireVFXNiagara, Mesh, Weapon->MuzzleSocketName, FVector::ZeroVector, FRotator::ZeroRotator,
				    EAttachLocation::SnapToTarget, true);
			}
			else
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(Weapon->GetWorld(), Stats.FireVFXNiagara, Muzzle.GetLocation(), Muzzle.Rotator());
			}
		}

		if (Stats.FireVFXParticle)
		{
			UGameplayStatics::SpawnEmitterAtLocation(Weapon->GetWorld(), Stats.FireVFXParticle, Muzzle);
		}
	}

	return Super::ExecuteCueNative(MyTarget, Parameters);
}
