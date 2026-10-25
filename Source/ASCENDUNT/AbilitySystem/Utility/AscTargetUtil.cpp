#include "AscTargetUtil.h"

#include "AbilitySystemGlobals.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "GenericTeamAgentInterface.h"

bool UAscTargetUtil::IsEnemyActor(const AActor* SourceActor, const AActor* TargetActor)
{
	if (!SourceActor || !TargetActor || SourceActor == TargetActor)
	{
		return false;
	}

	const IGenericTeamAgentInterface* SourceTeam = Cast<IGenericTeamAgentInterface>(SourceActor);
	const IGenericTeamAgentInterface* TargetTeam = Cast<IGenericTeamAgentInterface>(TargetActor);
	if (SourceTeam && TargetTeam)
	{
		return SourceTeam->GetGenericTeamId() != TargetTeam->GetGenericTeamId();
	}

	return true;
}


bool UAscTargetUtil::IsFriendlyActor(const AActor* SourceActor, const AActor* TargetActor)
{
	if (!SourceActor || !TargetActor)
	{
		return false;
	}

	if (SourceActor == TargetActor)
	{
		return true;
	}

	const IGenericTeamAgentInterface* SourceTeam = Cast<IGenericTeamAgentInterface>(SourceActor);
	const IGenericTeamAgentInterface* TargetTeam = Cast<IGenericTeamAgentInterface>(TargetActor);
	return SourceTeam && TargetTeam && SourceTeam->GetGenericTeamId() == TargetTeam->GetGenericTeamId();
}

void UAscTargetUtil::FindEnemyActorsInRadius(AActor* SourceActor, float Radius, TArray<AActor*>& OutTargets)
{
	OutTargets.Reset();

	if (!SourceActor || Radius <= 0.0f)
	{
		return;
	}

	UWorld* World = SourceActor->GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AscFindEnemyActorsInRadius), false);
	Params.AddIgnoredActor(SourceActor);

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		SourceActor->GetActorLocation(),
		FQuat::Identity,
		ObjParams,
		FCollisionShape::MakeSphere(Radius),
		Params);

	TSet<AActor*> UniqueTargets;
	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* TargetActor = Result.GetActor();
		if (!TargetActor || UniqueTargets.Contains(TargetActor))
		{
			continue;
		}

		if (!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
		{
			continue;
		}

		if (IsEnemyActor(SourceActor, TargetActor))
		{
			UniqueTargets.Add(TargetActor);
			OutTargets.Add(TargetActor);
		}
	}
}

void UAscTargetUtil::ApplyKnockback(AActor* SourceActor, AActor* TargetActor, float Strength, float UpwardStrength)
{
	if (!SourceActor || !TargetActor || Strength <= 0.0f)
	{
		return;
	}

	FVector Dir = TargetActor->GetActorLocation() - SourceActor->GetActorLocation();
	Dir.Z = 0.0f;
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = SourceActor->GetActorForwardVector();
	}

	const FVector LaunchVelocity = Dir * Strength + FVector::UpVector * UpwardStrength;

	if (ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
	{
		TargetCharacter->LaunchCharacter(LaunchVelocity, true, true);
		return;
	}

	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
	{
		RootPrim->AddImpulse(LaunchVelocity, NAME_None, true);
	}
}
