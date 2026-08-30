// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotifyState_MeleeHitWindow.h"

#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"

AAscenduntWeapon* UAnimNotifyState_MeleeHitWindow::FindWeapon(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp) return nullptr;

	// 1) Notify가 캐릭터 메쉬에서 뜬 경우
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return nullptr;

	if (AHeroCharacter* HeroChar = Cast<AHeroCharacter>(Owner))
	{
		if (auto* WEC = HeroChar->GetWeaponEquipmentComponent())
		{
			return WEC->GetCurrentWeapon();
		}
	}

	// 1-5) Notify가 AI 캐릭터 메쉬에서 뜬 경우
	if (ABaseAICharacter* AIChar = Cast<ABaseAICharacter>(Owner))
	{
		if (UWeaponEquipmentComponent* WEC = AIChar->FindComponentByClass<UWeaponEquipmentComponent>())
		{
			return WEC->GetCurrentWeapon();
		}
	}

	// 2) Notify가 무기 메쉬에서 뜬 경우
	if (AAscenduntWeapon* W = Cast<AAscenduntWeapon>(MeshComp->GetOwner()))
	{
		return W;
	}

	return nullptr;
}

void UAnimNotifyState_MeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	if (!MeshComp)
		return;

	AAscenduntWeapon* Weapon = FindWeapon(MeshComp);
	if (!Weapon)
		return;

	APawn* PawnOwner = Cast<APawn>(MeshComp->GetOwner());

	// fallback: Weapon Instigator / Owner 체인
	if (!PawnOwner) PawnOwner = Weapon->GetInstigator();
	if (!PawnOwner) PawnOwner = Cast<APawn>(Weapon->GetOwner());
	if (!PawnOwner || !PawnOwner->IsLocallyControlled()) return;

	// Only the locally controlled pawn reports the window. Without this guard,
	// a remote player's server montage and its owning-client montage can both
	// report the same Notify and reopen/reset the authoritative hit window.
	Weapon->NotifyMeleeHitWindow(true, StepIndex);
}

void UAnimNotifyState_MeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);

	if (!MeshComp)
		return;

	AAscenduntWeapon* Weapon = FindWeapon(MeshComp);
	if (!Weapon)
		return;

	APawn* PawnOwner = Cast<APawn>(MeshComp->GetOwner());
	if (!PawnOwner) PawnOwner = Weapon->GetInstigator();
	if (!PawnOwner) PawnOwner = Cast<APawn>(Weapon->GetOwner());
	if (!PawnOwner || !PawnOwner->IsLocallyControlled()) return;

	Weapon->NotifyMeleeHitWindow(false, StepIndex);
}
