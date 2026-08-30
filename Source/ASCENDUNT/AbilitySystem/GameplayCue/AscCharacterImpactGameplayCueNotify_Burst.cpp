#include "AscCharacterImpactGameplayCueNotify_Burst.h"

#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Character/BaseCharacter.h"
#include "ASCENDUNT/Character/AscCharacterFeedbackData.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

namespace AscCharacterImpactCue
{
	static const UObject* ResolveSourceObject(const FGameplayCueParameters& Parameters)
	{
		// FGameplayCueParameters가 SourceObject / EffectContext를 기준으로 원본 SourceObject를 반환한다.
		return Parameters.GetSourceObject();
	}

	static FRotator MakeImpactRotation(const FVector& Normal)
	{
		const FVector SafeNormal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		return SafeNormal.Rotation();
	}

	static bool SpawnImpactVFX(
		UWorld* World,
		const FVector& Location,
		const FRotator& Rotation,
		UNiagaraSystem* Niagara,
		UParticleSystem* Particle)
	{
		bool bSpawned = false;

		if (Niagara)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				Niagara,
				Location,
				Rotation);

			bSpawned = true;
		}

		if (Particle)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				World,
				Particle,
				Location,
				Rotation,
				FVector::OneVector,
				true);

			bSpawned = true;
		}

		return bSpawned;
	}
}

bool UAscCharacterImpactGameplayCueNotify_Burst::ExecuteCueNative(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
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

	FVector ImpactLocation = MyTarget->GetActorLocation();
	FVector ImpactNormal = Parameters.Normal;

	// Damage EffectContext에 HitResult가 있으면 가장 정확한 실제 충돌 지점을 우선 사용한다.
	if (Parameters.EffectContext.IsValid())
	{
		if (const FHitResult* HitResult = Parameters.EffectContext.GetHitResult())
		{
			ImpactLocation = HitResult->ImpactPoint;
			ImpactNormal = HitResult->ImpactNormal;
		}
		else if (!Parameters.Location.IsNearlyZero())
		{
			ImpactLocation = Parameters.Location;
		}
	}
	else if (!Parameters.Location.IsNearlyZero())
	{
		ImpactLocation = Parameters.Location;
	}

	const FRotator ImpactRotation = AscCharacterImpactCue::MakeImpactRotation(ImpactNormal);

	bool bSpawnedAny = false;

	// -----------------------------------------------------------------
	// Layer 1: Weapon impact
	// "무엇으로 때렸는가". Health/Shield 공통 하나만 사용한다.
	// -----------------------------------------------------------------
	const UObject* SourceObject = AscCharacterImpactCue::ResolveSourceObject(Parameters);
	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(SourceObject);

	if (Weapon && Weapon->WeaponData)
	{
		const FAscenduntWeaponModeStats& WeaponStats = Weapon->GetCurrentModeStats();
		const bool bAOEImpact = Parameters.AggregatedSourceTags.HasTagExact(AscenduntGameplayTags::Effect_Weapon_AOEImpact);
		const bool bAOESplash = Parameters.AggregatedSourceTags.HasTagExact(AscenduntGameplayTags::Effect_Weapon_AOESplash);
		const FAscenduntWeaponImpactVFX& ImpactVFX = bAOEImpact ? WeaponStats.AOEImpactVFX : WeaponStats.ImpactVFX;

		// Splash victims get only their target-material feedback. The explosion itself is rendered
		// once at the direct/world impact point, not once per actor in the radius.
		if (!bAOESplash)
		{
			bSpawnedAny |= AscCharacterImpactCue::SpawnImpactVFX(
				World,
				ImpactLocation,
				ImpactRotation,
				ImpactVFX.CharacterNiagara,
				ImpactVFX.CharacterParticle);
		}
	}

	// -----------------------------------------------------------------
	// Layer 2: Target feedback
	// "무엇을 때렸는가". 실제 Health/Shield Cue Tag에 따라 선택한다.
	// -----------------------------------------------------------------
	const ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(MyTarget);
	const UAscCharacterFeedbackData* FeedbackData =
		TargetCharacter ? TargetCharacter->GetCharacterFeedbackData() : nullptr;

	if (!FeedbackData)
	{
		return bSpawnedAny;
	}

	const bool bHealthImpact =
		Parameters.MatchedTagName.MatchesTagExact(
			AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactCharacter_Health) ||
		Parameters.OriginalTag.MatchesTagExact(
			AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactCharacter_Health);

	const bool bShieldImpact =
		Parameters.MatchedTagName.MatchesTagExact(
			AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactCharacter_Shield) ||
		Parameters.OriginalTag.MatchesTagExact(
			AscenduntGameplayTags::GameplayCue_Replicated_Weapon_ImpactCharacter_Shield);

	if (bHealthImpact)
	{
		bSpawnedAny |= AscCharacterImpactCue::SpawnImpactVFX(
			World,
			ImpactLocation,
			ImpactRotation,
			FeedbackData->HealthImpactVFXNiagara,
			FeedbackData->HealthImpactVFXParticle);
	}
	else if (bShieldImpact)
	{
		bSpawnedAny |= AscCharacterImpactCue::SpawnImpactVFX(
			World,
			ImpactLocation,
			ImpactRotation,
			FeedbackData->ShieldImpactVFXNiagara,
			FeedbackData->ShieldImpactVFXParticle);
	}

	return bSpawnedAny;
}
