#include "EditorTestGameMode.h"

#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/GameModes/TestAndSettings/AscenduntWorldSettings.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"

AEditorTestGameMode::AEditorTestGameMode()
{
    // The map's WorldSettings already determines the test hero.
    bOpenHeroSelectionOnPostLogin = false;
}

void AEditorTestGameMode::PostLogin(APlayerController* NewPlayer)
{
    // Reuse TrainingGameMode's team/display-name setup while suppressing the
    // hero-selection UI through bOpenHeroSelectionOnPostLogin=false.
    Super::PostLogin(NewPlayer);

    AAscenduntPlayerController* AscPC = Cast<AAscenduntPlayerController>(NewPlayer);
    AAscenduntPlayerState* AscPS = AscPC ? AscPC->GetPlayerState<AAscenduntPlayerState>() : nullptr;

    if (!AscPC || !AscPS)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EditorTestGM] Cannot initialize test hero. PC=%s PS=%s"),
            *GetNameSafe(AscPC),
            *GetNameSafe(AscPS));
        return;
    }

    const AAscenduntWorldSettings* AscWorldSettings =
        Cast<AAscenduntWorldSettings>(GetWorldSettings());

    if (!AscWorldSettings)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EditorTestGM] WorldSettings is not AAscenduntWorldSettings. Current=%s"),
            *GetNameSafe(GetWorldSettings()));
        return;
    }

    const UHeroPawnData* EditorPawnData = AscWorldSettings->GetEditorTestPawnData();
    if (!EditorPawnData || !EditorPawnData->PawnClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EditorTestGM] Editor Test Pawn Data or PawnClass is missing. WorldSettings=%s PawnData=%s PawnClass=%s"),
            *GetNameSafe(AscWorldSettings),
            *GetNameSafe(EditorPawnData),
            EditorPawnData ? *GetNameSafe(EditorPawnData->PawnClass) : TEXT("None"));
        return;
    }

    // RestartPlayer asks GetDefaultPawnClassForController() for the Pawn class,
    // so authoritative PawnData must be set before SpawnOrReplaceHeroPawn().
    AscPS->SetPawnData(EditorPawnData);

    const bool bSpawned = SpawnOrReplaceHeroPawn(AscPC);

    UE_LOG(LogTemp, Warning,
        TEXT("[EditorTestGM] Immediate hero start. PC=%s PawnData=%s PawnClass=%s Spawned=%d"),
        *GetNameSafe(AscPC),
        *GetNameSafe(EditorPawnData),
        *GetNameSafe(EditorPawnData->PawnClass),
        bSpawned ? 1 : 0);
}
