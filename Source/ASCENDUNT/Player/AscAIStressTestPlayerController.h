#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "AscAIStressTestPlayerController.generated.h"

/**
 * AI 성능 측정 맵 전용 PlayerController.
 * F8: 다음 Zone에 +50 AI
 * F9: 네 Zone Dummy 생성 + 해당 Zone AI Target 강제 지정
 * F10: 모든 테스트 AI를 Pool로 반납하고 Dummy 제거
 */
UCLASS()
class ASCENDUNT_API AAscAIStressTestPlayerController : public AAscenduntPlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;

protected:
	void HandleSpawnNextZonePressed();
	void HandleSpawnDummiesPressed();
	void HandleResetStressTestPressed();

	UFUNCTION(Server, Reliable)
	void Server_SpawnNextStressZone();

	UFUNCTION(Server, Reliable)
	void Server_SpawnStressDummies();

	UFUNCTION(Server, Reliable)
	void Server_ResetStressTest();
};
