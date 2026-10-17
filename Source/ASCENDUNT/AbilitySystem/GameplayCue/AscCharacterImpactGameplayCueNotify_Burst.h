#pragma once

#include "CoreMinimal.h"
#include "AscenduntGameplayCueNotify_Burst.h"
#include "AscCharacterImpactGameplayCueNotify_Burst.generated.h"

/**
 * Health / Shield 캐릭터 피격 GameplayCue 공용 실행기.
 *
 * 하나의 Cue에서 두 레이어를 같이 재생한다.
 * 1) WeaponDefinitionData::WeaponImpactCharacterVFX : 무엇으로 때렸는가
 * 2) CharacterFeedbackData의 Health/ShieldImpactVFX : 무엇을 때렸는가
 *
 * Health/Shield 여부는 현재 실행된 GameplayCue Tag로 자동 판별한다.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API UAscCharacterImpactGameplayCueNotify_Burst : public UAscenduntGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool ExecuteCueNative(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
