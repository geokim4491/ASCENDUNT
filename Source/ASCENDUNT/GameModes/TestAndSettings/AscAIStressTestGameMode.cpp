#include "AscAIStressTestGameMode.h"

#include "ASCENDUNT/Player/AscAIStressTestPlayerController.h"

AAscAIStressTestGameMode::AAscAIStressTestGameMode()
{
	PlayerControllerClass = AAscAIStressTestPlayerController::StaticClass();
}
