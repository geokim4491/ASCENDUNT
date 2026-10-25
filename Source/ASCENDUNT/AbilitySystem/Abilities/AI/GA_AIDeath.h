// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AscenduntGameplayAbility.h"
#include "GA_AIDeath.generated.h"

/**
 * 몬스터 사망 어빌리티. 사망 연출과 사망으로 인한 게임플레이 효과를 담당한다.
 *
 * ── 흐름 ────────────────────────────────────────────────────────────
 *   HealthComponent::StartDeath()
 *     → OnDeathStarted
 *     → ABaseAICharacter 가 충돌/이동/AI로직을 정지하고 DeathEventTag 이벤트 발송
	 *     → 이 어빌리티: 클라이언트에 DeathMontage 연출 전파
	 *     → 서버 Timer 만료 → ABaseAICharacter::FinishDeathDeferred()
 *     → FinishDeath → OnDeathFinished → 풀 반납
 *
 * ── 책임 경계 ───────────────────────────────────────────────────────
 * 넣으면 안 되는 것 (전부 ABaseAICharacter / UAIPoolSubsystem 소유):
 *   - Destroy Actor          액터 수명
 *   - Un Possess             컨트롤러 소유
 *   - Set Collision Enabled  충돌 상태
 *
 * 넣을 것:
	 *   - 사망 몽타주 연출 + 서버 사망 타이머 (이 클래스가 처리)
 *   - 킬러에게 GE 적용(스탯 상승 등), 아이템 드랍, 주변 유닛 버프
 *   - VFX / SFX → 단, 아래 주의사항 참고
 *
 * ── 주의: 이 어빌리티는 서버에서만 실행된다 ─────────────────────────
	 * AI 폰은 소유 클라이언트가 없어서 어빌리티 활성화가 시뮬레이티드 프록시에
	 * 전파되지 않는다. Dedicated Server는 AI AnimBP/Pose를 계산하지 않으므로
	 * ABaseAICharacter의 NetMulticast를 통해 클라이언트에서만 몽타주를 재생한다.
	 * 여기서 직접 스폰한 VFX/SFX 는 서버에서만 보인다.
 * 모든 플레이어에게 보여야 하는 연출은 BP_BaseAICharacter 의
 * On Death Started 이벤트에 붙일 것. (그 델리게이트는 모든 머신에서 발생한다)
 *
 * ── 안전장치 ────────────────────────────────────────────────────────
 * 이 어빌리티가 어떤 이유로 끝나든(정상 종료, 취소, 차단) EndAbility 에서
 * 사망 완료를 보장한다. 그렇지 않으면 폰이 풀로 돌아오지 못한다.
 * 아예 활성화조차 되지 않은 경우는 ABaseAICharacter 가 트리거 개수로 감지해
 * 직접 완료시킨다.
 */
UCLASS()
class ASCENDUNT_API UGA_AIDeath : public UAscenduntGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AIDeath(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** 연출 확장점. 위 "주의" 항목대로 서버에서만 실행된다는 점에 유의할 것. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Death", DisplayName = "On Death Presentation")
	void K2_OnDeathPresentation();

	/**
	 * 데스 몽타주가 끝난 시점의 연출 확장점. 폰이 풀로 반납되기 직전이다.
	 *
	 * 쓰러지는 동작만 있고 착지 프레임이 없는 몽타주(Paragon 미니언 등)에서
	 * 그 뒤를 이어받을 연출을 붙이는 자리다. 파편 폭발이 대표적인 예다.
	 *
	 * 기본 구현이 없으므로, 이 이벤트를 구현하지 않은 BP는 아무 영향도 받지 않는다.
	 *
	 * 주의: K2_OnDeathPresentation 과 마찬가지로 서버에서만 실행된다.
	 * 모든 플레이어에게 보여야 하는 VFX/SFX 는 여기서 직접 스폰하지 말고
	 * ASC->ExecuteGameplayCue()를 통해 내보낼 것.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|Death", DisplayName = "On Death Montage Finished")
	void K2_OnDeathMontageFinished();

	/** 클라이언트 사망 몽타주 재생 속도. 서버 Timer도 같은 배율을 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Death", meta = (ClampMin = "0.01"))
	float DeathMontagePlayRate = 1.f;

private:
	UFUNCTION()
	void HandleDeathPresentationFinished();

	/** 사망 완료를 예약한다. 어빌리티를 종료시키지 않으므로 EndAbility 에서도 안전하게 부를 수 있다. */
	void ScheduleFinishDeath();

	FTimerHandle DeathTimerHandle;

	// InstancedPerActor 라 인스턴스가 재사용된다. EndAbility 에서 반드시 초기화한다.
	bool bFinishScheduled = false;
};
