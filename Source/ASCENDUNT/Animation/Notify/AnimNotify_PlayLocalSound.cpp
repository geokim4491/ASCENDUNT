// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_PlayLocalSound.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UAnimNotify_PlayLocalSound::UAnimNotify_PlayLocalSound()
{
}

void UAnimNotify_PlayLocalSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	if (!MeshComp || !Sound)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	if (const APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (bRequireLocallyControlled && !PawnOwner->IsLocallyControlled())
		{
			return;
		}
	}
	else
	{
		if (!bAllowNonPawnOwner)
		{
			return;
		}
	}

	if (bAttachToMesh)
	{
		UGameplayStatics::SpawnSoundAttached(
			Sound,
			MeshComp,
			AttachPointName,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			true,
			VolumeMultiplier,
			PitchMultiplier
		);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(
			Owner,
			Sound,
			MeshComp->GetComponentLocation(),
			VolumeMultiplier,
			PitchMultiplier
		);
	}
}

FString UAnimNotify_PlayLocalSound::GetNotifyName_Implementation() const
{
	if (Sound)
	{
		return FString::Printf(TEXT("PlayLocalSound: %s"), *Sound->GetName());
	}

	return TEXT("PlayLocalSound");
}
