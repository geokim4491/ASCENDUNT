#pragma once

#include "CoreMinimal.h"
#include "HakukoTypes.generated.h"

UENUM(BlueprintType)
enum class EHakukoFoxType : uint8
{
	Attack UMETA(DisplayName = "공격 여우"),
	Support UMETA(DisplayName = "수호 여우")
};
