#pragma once

#include "CoreMinimal.h"
#include "AscenduntGameplayCueNotify_Burst.h"
#include "AscWeaponTraceGameplayCueNotify_Burst.generated.h"

/**
 * Hitscan Trail 전용 GameplayCue.
 *
 * SourceObject의 AAscenduntWeapon::GetCurrentModeStats().FireTraceVFXNiagara를 재생한다.
 * Start = EffectContext.Origin
 * End   = GameplayCueParameters.Location
 *
 * Niagara System에는 아래 Position User Parameter가 있어야 한다.
 * - User.User_TraceStart
 * - User.User_TraceEnd
 */
UCLASS(Blueprintable)
class ASCENDUNT_API UAscWeaponTraceGameplayCueNotify_Burst : public UAscenduntGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool ExecuteCueNative(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
