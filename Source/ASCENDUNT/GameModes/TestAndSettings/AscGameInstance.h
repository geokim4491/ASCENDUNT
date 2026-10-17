// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AscGameInstance.generated.h"

UENUM(BlueprintType)
enum class EAscPendingMainMenuPresentation : uint8
{
    None,
    PvEMissionSuccess
};

/**
 * GameInstance after removing local dedicated-server launch and old SessionSubsystem flow.
 *
 * Kept:
 * - network/travel failure handling
 * - local client return-to-main-menu helper
 */
UCLASS(Config = Game)
class ASCENDUNT_API UAscGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;

    UFUNCTION(BlueprintPure, Category = "Ascendunt|Maps")
    const FString& GetMainMenuMapPath() const { return MainMenuMapPath; }

    /** Stores a one-shot local presentation request across ClientTravel. */
    void SetPendingMainMenuPresentation(EAscPendingMainMenuPresentation InPresentation)
    {
        PendingMainMenuPresentation = InPresentation;
    }

    void ClearPendingMainMenuPresentation()
    {
        PendingMainMenuPresentation = EAscPendingMainMenuPresentation::None;
    }

    /** Returns and clears the pending request so it can only be presented once. */
    EAscPendingMainMenuPresentation ConsumePendingMainMenuPresentation()
    {
        const EAscPendingMainMenuPresentation Result = PendingMainMenuPresentation;
        PendingMainMenuPresentation = EAscPendingMainMenuPresentation::None;
        return Result;
    }

protected:
    UPROPERTY(Config, EditAnywhere, Category = "Ascendunt|Maps")
    FString MainMenuMapPath = TEXT("/Game/Maps/L_MainMenu");

private:
    UFUNCTION()
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

    UFUNCTION()
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

    void ReturnLocalPlayerToMainMenu(UWorld* World, const FString& Reason);

    EAscPendingMainMenuPresentation PendingMainMenuPresentation = EAscPendingMainMenuPresentation::None;
};
