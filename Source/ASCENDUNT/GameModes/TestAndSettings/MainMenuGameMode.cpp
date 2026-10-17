// Fill out your copyright notice in the Description page of Project Settings.


#include "MainMenuGameMode.h"

#include "ASCENDUNT/Player/AscenduntPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
    // 메인 메뉴는 실제 플레이 월드가 아니므로 Pawn 없이 UI 중심으로 동작
    DefaultPawnClass = nullptr;
    PlayerControllerClass = AAscenduntPlayerController::StaticClass();
    HUDClass = nullptr;

    // 플레이어가 자동으로 Pawn을 점유하지 않도록
    bStartPlayersAsSpectators = true;

    // MainMenu -> Lobby -> Match 흐름에서 유지해도 무방
    bUseSeamlessTravel = true;
}

void AMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    // TODO:
    // 메인 메뉴 전용 초기화가 필요하면 여기에 추가
    // 예: 세션 상태 초기화, 저장 데이터 검사, 버전 체크 등
}

UClass* AMainMenuGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    // 메인 메뉴에서는 Pawn 스폰 안 함
    return nullptr;
}

APawn* AMainMenuGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
    // 메인 메뉴에서는 Pawn 스폰 안 함
    return nullptr;
}
