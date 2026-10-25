// Fill out your copyright notice in the Description page of Project Settings.

#include "AscGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UAscGameInstance::Init()
{
    Super::Init();

    LoadConfig();

    if (GEngine)
    {
        GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
        GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
    }
}

void UAscGameInstance::Shutdown()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().RemoveAll(this);
        GEngine->OnTravelFailure().RemoveAll(this);
    }

    Super::Shutdown();
}

void UAscGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    ReturnLocalPlayerToMainMenu(World, ErrorString);
}

void UAscGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    ReturnLocalPlayerToMainMenu(World, ErrorString);
}

void UAscGameInstance::ReturnLocalPlayerToMainMenu(UWorld* World, const FString& Reason)
{
    UWorld* EffectiveWorld = World ? World : GetWorld();
    if (!EffectiveWorld || EffectiveWorld->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const FString CurrentMapName = EffectiveWorld->GetMapName();
    if (CurrentMapName.Contains(TEXT("L_MainMenu")))
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AscGameInstance] Returning to main menu. Reason=%s"), *Reason);

    if (APlayerController* PC = GetFirstLocalPlayerController())
    {
        PC->ClientTravel(MainMenuMapPath, TRAVEL_Absolute);
        return;
    }

    UGameplayStatics::OpenLevel(EffectiveWorld, FName(*MainMenuMapPath));
}
