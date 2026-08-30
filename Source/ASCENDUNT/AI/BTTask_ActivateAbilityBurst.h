#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BTTask_ActivateAbilityBurst.generated.h"

class UAbilitySystemComponent;

/**
 * 태그로 어빌리티를 발동하고, 일정 시간 뒤 "입력을 떼서" 종료시킨다.
 *
 * UBTTask_CastAbility 와의 차이:
 *   CastAbility 는 TryActivateAbilitiesByTag 만 부르고 즉시 Succeeded 를 반환한다.
 *   그래서 EAscenduntAbilityActivationPolicy::WhileInputActive 어빌리티
 *   (UGA_Fire_Auto 등)에는 쓸 수 없다. 그 어빌리티들은 InputReleased 가 와야
 *   멈추는데, 입력 경로인 UAscenduntAbilitySystemComponent::ProcessAbilityInput 은
 *   플레이어 전용이라 AI 는 타지 않기 때문이다. 결과적으로 한 번 켜지면
 *   영원히 발사한다.
 *
 * 이 태스크는 Spec 의 입력 상태를 직접 눌렀다/뗐다 해서 점사(burst)를 만든다.
 * 중단(AbortTask)될 때도 반드시 입력을 떼므로 무한 발사가 남지 않는다.
 */
UCLASS()
class ASCENDUNT_API UBTTask_ActivateAbilityBurst : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ActivateAbilityBurst();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	/** 발동할 어빌리티의 애셋 태그. 예: Ability.Weapon.Fire */
	UPROPERTY(EditAnywhere, Category = "GAS")
	FGameplayTag AbilityTag;

	/**
	 * 한 번에 쏠 발수. 1 이상이면 RateOfFire 로 필요한 시간을 자동 계산해
	 * BurstDuration 을 무시한다. 0 이면 BurstDuration 을 그대로 쓴다.
	 *
	 * 발수 기준을 권장한다. 시간 기준은 무기의 RateOfFire 를 조절할 때마다
	 * 발수가 같이 흔들려서 밸런싱이 어렵다.
	 */
	UPROPERTY(EditAnywhere, Category = "GAS", meta = (ClampMin = "0"))
	int32 ShotsPerBurst = 2;

	/** ShotsPerBurst 가 0 일 때 쓰는 입력 유지 시간(초). */
	UPROPERTY(EditAnywhere, Category = "GAS", meta = (ClampMin = "0.0", EditCondition = "ShotsPerBurst <= 0"))
	float BurstDuration = 1.2f;

	/**
	 * 어빌리티가 스스로 끝나면(탄 부족 등) 시간이 남아도 Succeeded 로 마친다.
	 * 끄면 BurstDuration 을 항상 채운다.
	 */
	UPROPERTY(EditAnywhere, Category = "GAS")
	bool bFinishWhenAbilityEnds = true;

private:
	struct FBurstMemory
	{
		float Elapsed = 0.f;
		/** 이번 실행에서 실제로 쓸 유지 시간. ShotsPerBurst 로부터 계산되거나 BurstDuration 이다. */
		float HoldDuration = 0.f;
		FGameplayAbilitySpecHandle SpecHandle;
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
	};

	/** ShotsPerBurst 발을 쏘는 데 필요한 입력 유지 시간을 무기의 RateOfFire 로부터 계산한다. */
	float ResolveHoldDuration(const UAbilitySystemComponent* ASC) const;

	/** 입력을 떼고 메모리를 정리한다. 여러 번 불려도 안전하다. */
	void ReleaseInput(FBurstMemory& Memory) const;
};
