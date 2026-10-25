// Fill out your copyright notice in the Description page of Project Settings.


#include "BTT_PlayMontageDirect.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"

UBTTask_PlayMontageDirect::UBTTask_PlayMontageDirect() { NodeName = "Play Montage Direct"; }

EBTNodeResult::Type UBTTask_PlayMontageDirect::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIC = OwnerComp.GetAIOwner();
	ACharacter* TargetCharacter = AIC ? Cast<ACharacter>(AIC->GetPawn()) : nullptr;

	if (TargetCharacter && MontageToPlay)
	{
		if (ABaseAICharacter* AIChar = Cast<ABaseAICharacter>(TargetCharacter))
		{
			// Dedicated Server에서는 AI Pose를 계산하지 않는다.
			// BT는 서버에서 실행되므로 클라이언트 연출만 Multicast한다.
			AIChar->Multicast_PlayPresentationMontage(MontageToPlay, 1.f, NAME_None);
		}
		else
		{
			TargetCharacter->PlayAnimMontage(MontageToPlay);
		}
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
