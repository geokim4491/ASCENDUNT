#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AscCharacterFeedbackData.generated.h"

class UNiagaraSystem;
class UParticleSystem;

/**
 * 캐릭터 자체가 어떤 피격 연출을 보여줄지 정의한다.
 *
 * WeaponDefinitionData의 WeaponImpactCharacterVFX가 "무엇으로 때렸는가"를 표현한다면,
 * 이 데이터는 "무엇을 때렸는가"를 표현한다.
 *
 * 예)
 * - Hero: 혈흔/에너지 피격
 * - Robot AI: 스파크/금속 파편
 * - Heavy AI: 중장갑 파편
 *
 * 클래스 기본값으로 BaseCharacter에 지정하므로 별도 네트워크 복제가 필요 없다.
 */
UCLASS(BlueprintType, Const)
class ASCENDUNT_API UAscCharacterFeedbackData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 실제 Health/Thorn 계층까지 피해가 들어갔을 때 대상 고유 피격 연출.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Health")
	TObjectPtr<UNiagaraSystem> HealthImpactVFXNiagara = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Health")
	TObjectPtr<UParticleSystem> HealthImpactVFXParticle = nullptr;

	// Armor/OverHealth 계층에서 피해가 막혔을 때 대상 고유 피격 연출.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Shield")
	TObjectPtr<UNiagaraSystem> ShieldImpactVFXNiagara = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Shield")
	TObjectPtr<UParticleSystem> ShieldImpactVFXParticle = nullptr;
};
