#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "AscenduntWorldSettings.generated.h"

class UHeroPawnData;

/**
 * Project WorldSettings extension for map-scoped ASCENDUNT settings.
 *
 * EditorTestPawnData is intentionally editor-only. It is stored per map and is
 * consumed only by AEditorTestGameMode, so normal Match/Training GameModes are
 * unaffected.
 */
UCLASS()
class ASCENDUNT_API AAscenduntWorldSettings : public AWorldSettings
{
    GENERATED_BODY()

public:
    /** Returns the map's editor-test hero, or nullptr outside editor builds. */
    const UHeroPawnData* GetEditorTestPawnData() const;

    /** Hero PawnData spawned immediately by AEditorTestGameMode for this map. */
    UPROPERTY(EditAnywhere, Category = "Ascendunt|Editor Test", meta = (DisplayName = "Editor Test Pawn Data"))
    TObjectPtr<UHeroPawnData> EditorTestPawnData = nullptr;
};
