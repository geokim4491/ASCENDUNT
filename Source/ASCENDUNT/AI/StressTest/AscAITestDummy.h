#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/Actor.h"
#include "AscAITestDummy.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

/**
 * 대규모 AI 성능 측정용 영구 Target Dummy.
 *
 * - 체력/ASC를 의도적으로 가지지 않아 죽지 않는다.
 * - IGenericTeamAgentInterface만 구현해 ABaseAIController의 Hostile 판정을 통과한다.
 * - Capsule은 AI Projectile(Pawn overlap)과 WeaponTrace를 받을 수 있다.
 * - VisualMesh는 선택 사항이다. BP 자식에서 Cube/Mannequin 등을 넣어 위치 확인용으로 쓸 수 있다.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AAscAITestDummy : public AActor, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAscAITestDummy();

	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(TeamId); }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** PvE Player와 같은 팀. 기본 AI TeamId(99) 기준 Hostile이 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test|Team")
	uint8 TeamId = 1;
};
