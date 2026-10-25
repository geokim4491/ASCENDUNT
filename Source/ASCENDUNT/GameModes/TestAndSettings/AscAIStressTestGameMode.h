#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/GameModes/TestAndSettings/EditorTestGameMode.h"
#include "AscAIStressTestGameMode.generated.h"

/**
 * Dedicated Server + 1 Client 대규모 AI 벤치마크 맵 전용 GameMode.
 * EditorTestGameMode의 WorldSettings 기반 즉시 Hero Spawn 흐름은 그대로 재사용하고,
 * PlayerController만 StressTest 전용 클래스로 교체한다.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AAscAIStressTestGameMode : public AEditorTestGameMode
{
	GENERATED_BODY()

public:
	AAscAIStressTestGameMode();
};
