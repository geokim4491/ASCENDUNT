#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ASCENDUNT/HeroSelection/AscHeroSelectionAuthorityInterface.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "TrainingGameMode.generated.h"

class AAscenduntPlayerController;
class AAscenduntPlayerState;
class UAscHeroSelectionComponent;

/**
 * Lightweight GameMode used to test the shared hero-selection flow in a local
 * training range before wiring the full match phase / external DS flow.
 *
 * Flow:
 * - Player enters TrainingMap.
 * - GameMode keeps the player as spectator until a hero is confirmed.
 * - Server opens the shared hero selection UI on the owning client.
 * - UI calls AAscenduntPlayerController::Server_RequestConfirmHero().
 * - UAscHeroSelectionComponent validates HeroId and sets PlayerState PawnData.
 * - TrainingGameMode immediately spawns PawnData->PawnClass and possesses it.
 * - HeroInitComponent then initializes ASC actor info, input, health, weapons, etc.
 */
UCLASS()
class ASCENDUNT_API ATrainingGameMode : public AGameMode, public IAscHeroSelectionAuthorityInterface
{
    GENERATED_BODY()

public:
    ATrainingGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    /** IAscHeroSelectionAuthorityInterface entry point used by PlayerController. */
    virtual bool HandleHeroConfirmRequest(AAscenduntPlayerController* PlayerController, int32 HeroId, EAscHeroSelectionContext Context) override;

    UFUNCTION(BlueprintCallable, Category = "Ascendunt|Training")
    bool SpawnOrReplaceHeroPawn(AAscenduntPlayerController* PlayerController);

protected:
    /** Shared hero selection implementation. Configure HeroRosterData on this component in BP_TrainingGameMode. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    TObjectPtr<UAscHeroSelectionComponent> HeroSelectionComponent;

    /** Training range should normally allow all duplicates. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Hero Selection")
    FAscHeroSelectionRules HeroSelectionRules;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Training")
    uint8 TrainingTeamId = 1;

    /** Immediately ask the owning client to open the hero selection UI after entering the training map. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Training")
    bool bOpenHeroSelectionOnPostLogin = true;

    /** Destroy the previous pawn before spawning the newly selected hero pawn. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Training")
    bool bDestroyPreviousPawnOnHeroChange = true;

private:
    bool ApplyTrainingHeroConfirm(AAscenduntPlayerController* PlayerController, int32 HeroId);
};
