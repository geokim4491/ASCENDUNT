#pragma once

#include "CoreMinimal.h"
#include "AscPvEGameTypes.generated.h"

UENUM(BlueprintType)
enum class EAscPvEGamePhase : uint8
{
    HeroSelection  UMETA(DisplayName = "Hero Selection"),
    Playing        UMETA(DisplayName = "Playing"),
    MissionClear   UMETA(DisplayName = "Mission Clear"),
    MissionFailed  UMETA(DisplayName = "Mission Failed")
};
