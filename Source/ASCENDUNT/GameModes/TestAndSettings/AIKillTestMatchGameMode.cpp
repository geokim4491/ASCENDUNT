#include "AIKillTestMatchGameMode.h"

#include "ASCENDUNT/AI/AISpawner.h"
#include "ASCENDUNT/GameModes/PVP/MatchGameState.h"
#include "EngineUtils.h"

void AAIKillTestMatchGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    int32 BoundSpawnerCount = 0;

    for (TActorIterator<AAISpawner> It(GetWorld()); It; ++It)
    {
        AAISpawner* Spawner = *It;
        if (!IsValid(Spawner))
        {
            continue;
        }

        Spawner->OnMonsterDiedNative.AddUObject(
            this,
            &ThisClass::HandleMonsterDied);

        ++BoundSpawnerCount;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[AIKillTestGM] Bound AISpawners=%d"),
        BoundSpawnerCount);
}

void AAIKillTestMatchGameMode::HandleMonsterDied(AActor* DeadActor)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bScoreApplied = AddTeamScore(
        AIScoringTeamId,
        ScorePerAIKill);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[AIKillTestGM] Enemy AI defeated Actor=%s Team=%d Score=%d Applied=%s"),
        *GetNameSafe(DeadActor),
        AIScoringTeamId,
        ScorePerAIKill,
        bScoreApplied ? TEXT("true") : TEXT("false"));
}
