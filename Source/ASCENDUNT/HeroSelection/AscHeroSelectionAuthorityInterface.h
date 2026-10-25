#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "AscHeroSelectionAuthorityInterface.generated.h"

class AAscenduntPlayerController;

/**
 * Implemented by GameModes that can accept server-authoritative hero confirms.
 *
 * PlayerController should not know whether the current map is a match, training
 * range, tutorial, or another mode. It just forwards the confirm request to the
 * current GameMode through this interface.
 */
UINTERFACE(MinimalAPI)
class UAscHeroSelectionAuthorityInterface : public UInterface
{
    GENERATED_BODY()
};

class ASCENDUNT_API IAscHeroSelectionAuthorityInterface
{
    GENERATED_BODY()

public:
    virtual bool HandleHeroConfirmRequest(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context) = 0;
};
