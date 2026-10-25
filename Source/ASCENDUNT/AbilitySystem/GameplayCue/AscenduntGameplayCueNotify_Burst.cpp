#include "AscenduntGameplayCueNotify_Burst.h"
// Fill out your copyright notice in the Description page of Project Settings.



#include "Kismet/GameplayStatics.h"
#include "GameplayEffectTypes.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "NiagaraFunctionLibrary.h"

bool UAscenduntGameplayCueNotify_Burst::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!ShouldPlay(MyTarget, Parameters))
	{
		return false;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[WeaponCue] Net=%d Tag=%s Target=%s"), int32(MyTarget->GetNetMode()), *Parameters.MatchedTagName.ToString(), *GetNameSafe(MyTarget));
	const bool bNativeExecuted = ExecuteCueNative(MyTarget, Parameters);
	const bool bBlueprintExecuted = bExecuteBlueprintCue && K2_ExecuteCue(MyTarget, Parameters);

	return bNativeExecuted || bBlueprintExecuted;
}

bool UAscenduntGameplayCueNotify_Burst::ExecuteCueNative(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Shared native world-impact layer. Existing ImpactWorld BP cues may remain as routing shells;
	// weapon data now chooses Normal/AOE World VFX from the paired impact structs.
	const bool bWorldImpact = Parameters.MatchedTagName.MatchesTagExact(AscenduntGameplayTags::GameplayCue_Weapon_ImpactWorld)
	    || Parameters.OriginalTag.MatchesTagExact(AscenduntGameplayTags::GameplayCue_Weapon_ImpactWorld);
	if (!bWorldImpact || !MyTarget)
	{
		return false;
	}

	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(Parameters.GetSourceObject());
	if (!Weapon || !Weapon->WeaponData)
	{
		return false;
	}

	const FAscenduntWeaponModeStats& Stats = Weapon->GetCurrentModeStats();
	const bool bAOEImpact = Parameters.AggregatedSourceTags.HasTagExact(AscenduntGameplayTags::Effect_Weapon_AOEImpact);
	const FAscenduntWeaponImpactVFX& VFX = bAOEImpact ? Stats.AOEImpactVFX : Stats.ImpactVFX;

	FVector Location = Parameters.Location;
	FVector Normal = Parameters.Normal;
	if (Parameters.EffectContext.IsValid())
	{
		if (const FHitResult* Hit = Parameters.EffectContext.GetHitResult())
		{
			if (!Hit->ImpactPoint.IsNearlyZero())
			{
				Location = Hit->ImpactPoint;
			}
			if (!Hit->ImpactNormal.IsNearlyZero())
			{
				Normal = Hit->ImpactNormal;
			}
		}
	}
	if (Location.IsNearlyZero())
	{
		Location = MyTarget->GetActorLocation();
	}
	const FRotator Rotation = (Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal()).Rotation();
	bool bSpawned = false;
	if (VFX.WorldNiagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(MyTarget->GetWorld(), VFX.WorldNiagara, Location, Rotation);
		bSpawned = true;
	}
	if (VFX.WorldParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(MyTarget->GetWorld(), VFX.WorldParticle, Location, Rotation);
		bSpawned = true;
	}
	return bSpawned;
}

bool UAscenduntGameplayCueNotify_Burst::ShouldPlay(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return false;
	}

	UWorld* World = MyTarget->GetWorld();
	if (!World)
	{
		return false;
	}

	if (bSkipOnDedicatedServer && World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	APawn* LocalPawn = GetLocalPawn(World);
	APawn* TargetPawn = GetTargetPawn(MyTarget);
	APawn* InstigatorPawn = GetInstigatorPawn(Parameters);

	switch (Audience)
	{
	case EAscenduntCueAudience::AllRelevant:
		return true;

	case EAscenduntCueAudience::AllExceptTarget:
		// "타깃 제외" = 타깃이 로컬 컨트롤이면 스킵
		return !(TargetPawn && TargetPawn->IsLocallyControlled());

	case EAscenduntCueAudience::AllExceptInstigator:
		// "가해자 제외" = 로컬 pawn이 instigator면 스킵
		return !(LocalPawn && InstigatorPawn && (LocalPawn == InstigatorPawn));

	default:
		return true;
	}
}

APawn* UAscenduntGameplayCueNotify_Burst::GetLocalPawn(UWorld* World) const
{
	// 로컬 플레이어 0 기준(일반 멀티 FPS 기준)
	return World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
}

APawn* UAscenduntGameplayCueNotify_Burst::GetTargetPawn(AActor* MyTarget) const
{
	if (APawn* P = Cast<APawn>(MyTarget))
	{
		return P;
	}
	// 타깃이 캐릭터가 아닌 액터일 때 대비(필요하면 확장)
	return MyTarget ? Cast<APawn>(MyTarget->GetInstigator()) : nullptr;
}

APawn* UAscenduntGameplayCueNotify_Burst::GetInstigatorPawn(const FGameplayCueParameters& Parameters) const
{
	AActor* Inst = Parameters.Instigator.Get();
	if (!Inst && Parameters.EffectContext.IsValid())
	{
		Inst = Parameters.EffectContext.GetOriginalInstigator();
	}

	if (APawn* P = Cast<APawn>(Inst))
	{
		return P;
	}
	return Inst ? Cast<APawn>(Inst->GetInstigator()) : nullptr;
}
