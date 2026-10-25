#pragma once

#include "CoreMinimal.h"
#include "AscMainMenuTypes.generated.h"

UENUM(BlueprintType)
enum class EAscMainMenuPage : uint8
{
    Main         UMETA(DisplayName = "Main"),
    Play         UMETA(DisplayName = "Play"),
    Hero         UMETA(DisplayName = "Hero"),
    Option       UMETA(DisplayName = "Option"),
    Social       UMETA(DisplayName = "Social"),
    Profile      UMETA(DisplayName = "Profile")
};
