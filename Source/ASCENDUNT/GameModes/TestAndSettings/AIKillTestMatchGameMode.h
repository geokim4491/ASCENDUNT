#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/GameModes/PVP/MatchGameMode.h"
#include "AIKillTestMatchGameMode.generated.h"

UCLASS()
class ASCENDUNT_API AAIKillTestMatchGameMode : public AMatchGameMode
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

    void HandleMonsterDied(AActor* DeadActor);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Ascendunt|Match|AI Kill Test")
    uint8 AIScoringTeamId = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Ascendunt|Match|AI Kill Test",
        meta = (ClampMin = "1"))
    int32 ScorePerAIKill = 1;
};
