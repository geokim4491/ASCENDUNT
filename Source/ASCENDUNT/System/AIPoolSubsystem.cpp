// Fill out your copyright notice in the Description page of Project Settings.


#include "AIPoolSubsystem.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "Engine/World.h"

#include "ASCENDUNT/Character/AI/AIPawnData.h"

void UAIPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UAIPoolSubsystem::Deinitialize()
{
	InactivePool.Empty();
	Super::Deinitialize();
}

ABaseAICharacter* UAIPoolSubsystem::AcquireAI(TSubclassOf<ABaseAICharacter> ClassToSpawn, const FTransform& SpawnTransform)
{
	if (!ClassToSpawn)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ABaseAICharacter* SpawnedAI = nullptr;

	// Check if we have an inactive AI of this class
	FAIPoolArray* PoolArray = InactivePool.Find(ClassToSpawn);
	if (PoolArray)
	{
		// 풀에 담긴 액터가 외부에서 파괴되면 UPROPERTY 배열 항목이 null이 된다.
		// 검사 없이 Pop 결과를 역참조하면 크래시하므로, 유효한 것이 나올 때까지 버린다.
		while (PoolArray->Characters.Num() > 0)
		{
			ABaseAICharacter* Candidate = PoolArray->Characters.Pop();
			if (IsValid(Candidate))
			{
				SpawnedAI = Candidate;
				break;
			}

			UE_LOG(LogTemp, Warning,
				TEXT("[AIPool] %s 풀에서 무효한 항목을 발견해 폐기했습니다. "
					 "풀에 반납된 AI를 외부에서 파괴하고 있는지 확인하세요."),
				*GetNameSafe(ClassToSpawn));
		}
	}

	if (SpawnedAI)
	{
		// Reused actors do not pass through SpawnActor's collision adjustment.
		// Keep them inactive and let the character place its current-size Capsule safely first.
		SpawnedAI->PrepareForPoolReuse(SpawnTransform);
	}
	else
	{
		// Spawn a new one if pool is empty
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnedAI = World->SpawnActor<ABaseAICharacter>(ClassToSpawn, SpawnTransform, SpawnParams);
	}

	// Make sure it doesn't stay hidden
	if (SpawnedAI)
	{
		// Let the AI know it's being acquired
		// (Wait, logic reactivation happens in InitializeFromPawnData)
	}

	return SpawnedAI;
}

void UAIPoolSubsystem::ReleaseAI(ABaseAICharacter* AICharacter)
{
	if (!AICharacter || AICharacter->IsPendingKillPending())
	{
		return;
	}

	UClass* AIClass = AICharacter->GetClass();
	FAIPoolArray& PoolArray = InactivePool.FindOrAdd(AIClass);
	
	PoolArray.Characters.AddUnique(AICharacter);

	// Reset the AI's state for pooling
	AICharacter->ResetForPool();
}
