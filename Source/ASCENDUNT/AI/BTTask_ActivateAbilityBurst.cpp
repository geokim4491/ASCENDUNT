#include "BTTask_ActivateAbilityBurst.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"

UBTTask_ActivateAbilityBurst::UBTTask_ActivateAbilityBurst()
{
	NodeName = TEXT("Activate Ability Burst");

	bNotifyTick = true;
	bCreateNodeInstance = false;
}

uint16 UBTTask_ActivateAbilityBurst::GetInstanceMemorySize() const
{
	return sizeof(FBurstMemory);
}

FString UBTTask_ActivateAbilityBurst::GetStaticDescription() const
{
	const FString Amount = (ShotsPerBurst > 0)
		? FString::Printf(TEXT("Shots: %d"), ShotsPerBurst)
		: FString::Printf(TEXT("Duration: %.2fs"), BurstDuration);

	return FString::Printf(TEXT("%s\nTag: %s\n%s"),
		*Super::GetStaticDescription(),
		*AbilityTag.ToString(),
		*Amount);
}

float UBTTask_ActivateAbilityBurst::ResolveHoldDuration(const UAbilitySystemComponent* ASC) const
{
	if (ShotsPerBurst <= 0)
	{
		return BurstDuration;
	}

	// UGA_Fire_Auto 는 활성화 즉시 1발을 쏘고, 그 뒤 1/RateOfFire 간격으로 이어 쏜다.
	// 따라서 N 발에 필요한 시간은 (N-1) * 간격 이고, 마지막 발이 확실히 나가도록
	// 약간의 여유를 더한다.
	const float ROF = ASC
		? ASC->GetNumericAttribute(UAscenduntWeaponSet::GetRateOfFireAttribute())
		: 1.f;

	const float Interval = 1.f / FMath::Max(0.1f, ROF);

	return (ShotsPerBurst - 1) * Interval + (Interval * 0.1f);
}

EBTNodeResult::Type UBTTask_ActivateAbilityBurst::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBurstMemory* Memory = CastInstanceNodeMemory<FBurstMemory>(NodeMemory);
	*Memory = FBurstMemory();

	AAIController* AIC = OwnerComp.GetAIOwner();
	APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC || !AbilityTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	// 태그로 Spec 을 찾는다.
	// TryActivateAbilitiesByTag 를 쓰지 않는 이유는, 나중에 입력을 떼려면
	// 어떤 Spec 이 발동됐는지 핸들을 알고 있어야 하기 때문이다.
	FGameplayAbilitySpecHandle FoundHandle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(AbilityTag))
		{
			FoundHandle = Spec.Handle;
			break;
		}
	}

	if (!FoundHandle.IsValid())
	{
		// 무기 AbilitySet 이 이 태그의 어빌리티를 주지 않았다.
		// 근접 BT 를 원거리 AI 에 재사용했을 때 흔히 걸리는 지점이다.
		// 부여된 목록을 같이 찍어야 "무엇이 없는지"가 아니라 "무엇이 있는지"를 알 수 있다.
		FString Granted;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability)
			{
				Granted += FString::Printf(TEXT("\n    - %s [%s]"),
					*GetNameSafe(Spec.Ability), *Spec.Ability->GetAssetTags().ToStringSimple());
			}
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[AI] %s: '%s' 태그의 어빌리티가 없습니다. 부여된 어빌리티:%s"),
			*GetNameSafe(Pawn), *AbilityTag.ToString(),
			Granted.IsEmpty() ? TEXT(" (없음 - AbilitySet 미부여 또는 무기 미장착)") : *Granted);

		return EBTNodeResult::Failed;
	}

	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(FoundHandle);
	if (!Spec)
	{
		return EBTNodeResult::Failed;
	}

	// 활성화보다 먼저 눌러둔다. WhileInputActive 어빌리티가 ActivateAbility 시점에
	// 입력 상태를 확인하더라도 눌린 것으로 보이게 하기 위해서다.
	Spec->InputPressed = true;
	ASC->AbilitySpecInputPressed(*Spec);

	if (!ASC->TryActivateAbility(FoundHandle))
	{
		// 쿨다운/코스트/태그 차단 등으로 발동 실패. 누른 입력을 반드시 되돌린다.
		Spec->InputPressed = false;
		ASC->AbilitySpecInputReleased(*Spec);

		// 원거리 AI 에서 가장 흔한 원인은 무기 미장착이다.
		// GA_BaseFire::FireOnce_Trigger 가 GetASCAndWeapon() 에서 바로 실패한다.
		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);

		UE_LOG(LogTemp, Warning,
			TEXT("[AI] %s: '%s' 발동 실패. 보유 태그: %s"),
			*GetNameSafe(Pawn), *AbilityTag.ToString(), *OwnedTags.ToStringSimple());

		return EBTNodeResult::Failed;
	}

	Memory->ASC = ASC;
	Memory->SpecHandle = FoundHandle;
	Memory->Elapsed = 0.f;

	// 유지 시간은 활성화 시점의 RateOfFire 로 결정한다.
	// 무기 스탯 GE 가 적용된 뒤라야 올바른 값이 나오므로 여기서 계산한다.
	Memory->HoldDuration = ResolveHoldDuration(ASC);

	// 1발만 쏘는 경우(ShotsPerBurst == 1)나 유지 시간이 0이면 즉시 뗀다.
	if (Memory->HoldDuration <= 0.f)
	{
		ReleaseInput(*Memory);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_ActivateAbilityBurst::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBurstMemory* Memory = CastInstanceNodeMemory<FBurstMemory>(NodeMemory);

	UAbilitySystemComponent* ASC = Memory->ASC.Get();
	if (!ASC)
	{
		// 폰이 죽었거나 풀로 반납되는 등 ASC 가 사라졌다.
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 어빌리티가 스스로 끝났는지 확인(탄 부족, 무기 해제 등).
	if (bFinishWhenAbilityEnds)
	{
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Memory->SpecHandle);
		if (!Spec || !Spec->IsActive())
		{
			ReleaseInput(*Memory);
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	Memory->Elapsed += DeltaSeconds;
	if (Memory->Elapsed >= Memory->HoldDuration)
	{
		ReleaseInput(*Memory);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_ActivateAbilityBurst::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 여기서 떼지 않으면 발사가 영원히 멈추지 않는다.
	// 피격/사망/타깃 상실 등으로 상위 브랜치가 이 태스크를 자를 때 반드시 통과하는 경로다.
	FBurstMemory* Memory = CastInstanceNodeMemory<FBurstMemory>(NodeMemory);
	ReleaseInput(*Memory);

	return EBTNodeResult::Aborted;
}

void UBTTask_ActivateAbilityBurst::ReleaseInput(FBurstMemory& Memory) const
{
	UAbilitySystemComponent* ASC = Memory.ASC.Get();
	if (!ASC || !Memory.SpecHandle.IsValid())
	{
		return;
	}

	if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Memory.SpecHandle))
	{
		Spec->InputPressed = false;
		ASC->AbilitySpecInputReleased(*Spec);
	}

	// 두 번 떼지 않도록 무효화한다.
	Memory.ASC = nullptr;
	Memory.SpecHandle = FGameplayAbilitySpecHandle();
}
