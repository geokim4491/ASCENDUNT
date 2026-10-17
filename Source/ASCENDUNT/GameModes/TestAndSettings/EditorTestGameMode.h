#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/GameModes/TestAndSettings/TrainingGameMode.h"
#include "EditorTestGameMode.generated.h"

/**
 * Editor workflow GameMode that immediately spawns the HeroPawnData configured
 * in the current map's AAscenduntWorldSettings.
 *
 * Hero selection UI is skipped. Existing TrainingGameMode/HeroInitComponent
 * paths still own Pawn replacement, ASC ActorInfo, PawnData AbilitySets,
 * attributes, health, weapons, camera, and client-only input initialization.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AEditorTestGameMode : public ATrainingGameMode
{
    GENERATED_BODY()

public:
    AEditorTestGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
};
