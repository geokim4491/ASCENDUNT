#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AIPawnData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UBehaviorTree;
class UAscenduntAbilitySet;

UCLASS(BlueprintType)
class ASCENDUNT_API UAIPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 1. 외형 및 기본 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Data | Visual")
	TSoftClassPtr<class ABaseAICharacter> BPClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Data | Visual")
	TSubclassOf<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Data | Visual")
	TObjectPtr<USkeletalMesh> EnemyMesh;

	// 2. GAS 패키지 (여기에 체력 GE, 스킬 등이 다 들어있음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Data | GAS")
	TObjectPtr<UAscenduntAbilitySet> AbilitySet;

	// 3. 뇌 (Logic)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Data | BT")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// 4. 시야 (Perception)
	UPROPERTY(EditDefaultsOnly, Category = "Perception")
	float SightRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Perception")
	float LoseSightRadius = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Perception")
	float FieldOfView = 90.0f;

	// 5. 애니메이션 몽타주 (공용 스킬에서 읽어갈 데이터)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Data | Animation")
	TObjectPtr<class UAnimMontage> HitReactMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI Data | Animation")
	TObjectPtr<class UAnimMontage> DeathMontage;

	// 근접 몽타주는 여기 없다. 무기마다 다르므로 WeaponDefinitionData의
	// FAscenduntMeleeStep::HeroMontage / WeaponMontage 가 관리한다.
};
