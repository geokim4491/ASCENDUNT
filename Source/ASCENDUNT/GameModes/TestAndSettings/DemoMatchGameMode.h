#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ASCENDUNT/Match/AscMatchTypes.h"
#include "DemoMatchGameMode.generated.h"

class AAscenduntPlayerState;
class UHeroPawnData;

/**
 * Demo-only GameMode for direct match-map connection.
 *
 * Purpose:
 * - Bypass Lobby / Steam Lobby / FastAPI room flow.
 * - Allow raw IP/DDNS connection directly into L_Match.
 * - Assign Astell/Hakuko PawnData by URL option or join order.
 *
 * Example server:
 * ASCENDUNTServer.exe /Game/Maps/Game/L_Match?game=/Script/ASCENDUNT.DemoMatchGameMode?AscMaxPlayers=2 -log -port=7777 -nosteam
 *
 * Example clients:
 * open ASCENDUNT-SERVER.iptime.org:7777?Hero=Astell
 * open ASCENDUNT-SERVER.iptime.org:7777?Hero=Hakuko
 */
UCLASS()
class ASCENDUNT_API ADemoMatchGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    ADemoMatchGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void InitGameState() override;

    virtual void PreLogin(
        const FString& Options,
        const FString& Address,
        const FUniqueNetIdRepl& UniqueId,
        FString& ErrorMessage) override;

    virtual FString InitNewPlayer(
        APlayerController* NewPlayerController,
        const FUniqueNetIdRepl& UniqueId,
        const FString& Options,
        const FString& Portal) override;

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:
    /** Assigned when client uses ?Hero=Astell or when first player joins without Hero option. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Demo")
    TObjectPtr<const UHeroPawnData> AstellPawnData;

    /** Assigned when client uses ?Hero=Hakuko or when second player joins without Hero option. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Demo")
    TObjectPtr<const UHeroPawnData> HakukoPawnData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Demo")
    uint8 AstellTeamId = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Demo")
    uint8 HakukoTeamId = 2;

    /** Used when ?AscMaxPlayers is not provided. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Demo")
    int32 DefaultMaxPlayers = 3;

private:
    FAscMatchRuntimeSettings PendingMatchSettings;

    /** Raw pointer key is fine here because entries are removed on Logout and only used during login/start. */
    TMap<APlayerController*, FString> PendingPlayerOptions;

    int32 NextAutoAssignIndex = 0;

private:
    const UHeroPawnData* ResolvePawnDataFromOptions(const FString& Options);
    void AssignDemoPawnData(APlayerController* PC, const FString& Options);
    int32 GetResolvedMaxPlayers() const;
};
