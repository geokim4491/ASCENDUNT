#include "AscAIStressTestPlayerController.h"

#include "ASCENDUNT/AI/StressTest/AscAIStressTestDirector.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

namespace AscAIStressTest
{
	static AAscAIStressTestDirector* FindDirector(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AAscAIStressTestDirector> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

void AAscAIStressTestPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	FInputKeyBinding& SpawnBinding = InputComponent->BindKey(
		EKeys::F1,
		IE_Pressed,
		this,
		&ThisClass::HandleSpawnNextZonePressed);
	SpawnBinding.bConsumeInput = true;

	FInputKeyBinding& DummyBinding = InputComponent->BindKey(
		EKeys::F2,
		IE_Pressed,
		this,
		&ThisClass::HandleSpawnDummiesPressed);
	DummyBinding.bConsumeInput = true;

	FInputKeyBinding& ResetBinding = InputComponent->BindKey(
		EKeys::F3,
		IE_Pressed,
		this,
		&ThisClass::HandleResetStressTestPressed);
	ResetBinding.bConsumeInput = true;
}

void AAscAIStressTestPlayerController::HandleSpawnNextZonePressed()
{
	Server_SpawnNextStressZone();
}

void AAscAIStressTestPlayerController::HandleSpawnDummiesPressed()
{
	Server_SpawnStressDummies();
}

void AAscAIStressTestPlayerController::HandleResetStressTestPressed()
{
	Server_ResetStressTest();
}

void AAscAIStressTestPlayerController::Server_SpawnNextStressZone_Implementation()
{
	if (AAscAIStressTestDirector* Director = AscAIStressTest::FindDirector(GetWorld()))
	{
		Director->SpawnNextZone();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AIStress] No AAscAIStressTestDirector found in this level."));
	}
}

void AAscAIStressTestPlayerController::Server_SpawnStressDummies_Implementation()
{
	if (AAscAIStressTestDirector* Director = AscAIStressTest::FindDirector(GetWorld()))
	{
		Director->SpawnDummiesAndAssignTargets();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AIStress] No AAscAIStressTestDirector found in this level."));
	}
}

void AAscAIStressTestPlayerController::Server_ResetStressTest_Implementation()
{
	if (AAscAIStressTestDirector* Director = AscAIStressTest::FindDirector(GetWorld()))
	{
		Director->ResetStressTest();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AIStress] No AAscAIStressTestDirector found in this level."));
	}
}
