#include "AscAIStressTestDirector.h"

#include "AscAITestDummy.h"
#include "ASCENDUNT/AI/BaseAIController.h"
#include "ASCENDUNT/Character/AI/AIPawnData.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/System/AIPoolSubsystem.h"
#include "Engine/World.h"

AAscAIStressTestDirector::AAscAIStressTestDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	DummyClass = AAscAITestDummy::StaticClass();
}

bool AAscAIStressTestDirector::IsZoneConfigured(const int32 ZoneIndex) const
{
	return ZoneIndex >= 0 &&
		ZoneIndex < MaxStressZones &&
		ZoneCenters.IsValidIndex(ZoneIndex) &&
		IsValid(ZoneCenters[ZoneIndex]);
}

FTransform AAscAIStressTestDirector::MakeGridSpawnTransform(const int32 ZoneIndex, const int32 LocalAIIndex) const
{
	const AActor* CenterActor = IsZoneConfigured(ZoneIndex) ? ZoneCenters[ZoneIndex].Get() : nullptr;
	if (!CenterActor)
	{
		return FTransform::Identity;
	}

	const int32 Columns = FMath::Max(1, SpawnGridColumns);
	const int32 Rows = FMath::Max(1, FMath::CeilToInt(static_cast<float>(AIPerZone) / static_cast<float>(Columns)));

	const int32 Column = LocalAIIndex % Columns;
	const int32 Row = LocalAIIndex / Columns;

	const float CenteredColumn = static_cast<float>(Column) - (static_cast<float>(Columns - 1) * 0.5f);
	const float CenteredRow = static_cast<float>(Row) - (static_cast<float>(Rows - 1) * 0.5f);

	FTransform Result = CenterActor->GetActorTransform();
	const FVector LocalOffset(CenteredColumn * SpawnSpacing, CenteredRow * SpawnSpacing, 0.f);
	Result.AddToTranslation(CenterActor->GetActorRotation().RotateVector(LocalOffset));
	return Result;
}

ABaseAICharacter* AAscAIStressTestDirector::SpawnOneAI(const int32 ZoneIndex, const int32 LocalAIIndex)
{
	if (!HasAuthority() || !SpawnData || SpawnData->BPClass.IsNull() || !IsZoneConfigured(ZoneIndex))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	UAIPoolSubsystem* Pool = World ? World->GetSubsystem<UAIPoolSubsystem>() : nullptr;
	if (!World || !Pool)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = SpawnData->BPClass.LoadSynchronous();
	if (!ClassToSpawn)
	{
		return nullptr;
	}

	ABaseAICharacter* SpawnedAI = Pool->AcquireAI(ClassToSpawn, MakeGridSpawnTransform(ZoneIndex, LocalAIIndex));
	if (!SpawnedAI)
	{
		return nullptr;
	}

	SpawnedAI->SetPawnData(SpawnData);

	if (!SpawnedAI->GetController())
	{
		SpawnedAI->SpawnDefaultController();
	}

	if (ABaseAIController* AIC = Cast<ABaseAIController>(SpawnedAI->GetController()))
	{
		// 성능 테스트에서는 플레이어를 우연히 감지해 Combat으로 들어가는 변수를 제거한다.
		// Perception 자체는 살아 있고, Target 획득 정책만 Director가 소유한다.
		AIC->SetAutoTargetFromPerceptionEnabled(false);
		AIC->ClearTargetActor();

		if (ZoneDummies[ZoneIndex].IsValid())
		{
			AIC->SetTargetActor(ZoneDummies[ZoneIndex].Get());
		}
	}

	return SpawnedAI;
}

void AAscAIStressTestDirector::SpawnNextZone()
{
	if (!HasAuthority())
	{
		return;
	}

	if (NextZoneIndex >= MaxStressZones)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AIStress] All %d zones are already spawned. TotalAI=%d"),
			MaxStressZones,
			GetSpawnedAICount());
		return;
	}

	if (!IsZoneConfigured(NextZoneIndex))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[AIStress] ZoneCenters[%d] is not configured. Assign exactly four center actors in the level."),
			NextZoneIndex);
		return;
	}

	const int32 ZoneIndex = NextZoneIndex;
	int32 SpawnedCount = 0;

	ZoneAIs[ZoneIndex].Reserve(AIPerZone);
	for (int32 LocalIndex = 0; LocalIndex < AIPerZone; ++LocalIndex)
	{
		if (ABaseAICharacter* SpawnedAI = SpawnOneAI(ZoneIndex, LocalIndex))
		{
			ZoneAIs[ZoneIndex].Add(SpawnedAI);
			++SpawnedCount;
		}
	}

	if (SpawnedCount > 0)
	{
		++NextZoneIndex;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[AIStress] Spawn Zone=%d Requested=%d Spawned=%d TotalAI=%d ActiveZones=%d/%d"),
		ZoneIndex + 1,
		AIPerZone,
		SpawnedCount,
		GetSpawnedAICount(),
		NextZoneIndex,
		MaxStressZones);
}

AAscAITestDummy* AAscAIStressTestDirector::EnsureDummyForZone(const int32 ZoneIndex)
{
	if (!HasAuthority() || !IsZoneConfigured(ZoneIndex))
	{
		return nullptr;
	}

	if (ZoneDummies[ZoneIndex].IsValid())
	{
		return ZoneDummies[ZoneIndex].Get();
	}

	UWorld* World = GetWorld();
	UClass* ClassToSpawn = DummyClass ? DummyClass.Get() : AAscAITestDummy::StaticClass();
	if (!World || !ClassToSpawn)
	{
		return nullptr;
	}

	FTransform SpawnTransform = ZoneCenters[ZoneIndex]->GetActorTransform();
	SpawnTransform.AddToTranslation(FVector(0.f, 0.f, DummyZOffset));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AAscAITestDummy* Dummy = World->SpawnActor<AAscAITestDummy>(ClassToSpawn, SpawnTransform, SpawnParams);
	if (Dummy)
	{
		ZoneDummies[ZoneIndex] = Dummy;
	}

	return Dummy;
}

void AAscAIStressTestDirector::AssignZoneTarget(const int32 ZoneIndex, AActor* TargetActor)
{
	if (!HasAuthority() || ZoneIndex < 0 || ZoneIndex >= MaxStressZones)
	{
		return;
	}

	ZoneAIs[ZoneIndex].RemoveAll([](const TWeakObjectPtr<ABaseAICharacter>& AI)
	{
		return !AI.IsValid();
	});

	for (const TWeakObjectPtr<ABaseAICharacter>& AIWeak : ZoneAIs[ZoneIndex])
	{
		ABaseAICharacter* AI = AIWeak.Get();
		ABaseAIController* AIC = AI ? Cast<ABaseAIController>(AI->GetController()) : nullptr;
		if (!AIC)
		{
			continue;
		}

		AIC->SetAutoTargetFromPerceptionEnabled(false);
		AIC->SetTargetActor(TargetActor);
	}
}

void AAscAIStressTestDirector::SpawnDummiesAndAssignTargets()
{
	if (!HasAuthority())
	{
		return;
	}

	int32 DummyCount = 0;
	for (int32 ZoneIndex = 0; ZoneIndex < MaxStressZones; ++ZoneIndex)
	{
		if (!IsZoneConfigured(ZoneIndex))
		{
			continue;
		}

		if (AAscAITestDummy* Dummy = EnsureDummyForZone(ZoneIndex))
		{
			++DummyCount;
			AssignZoneTarget(ZoneIndex, Dummy);
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[AIStress] Combat targets ready. Dummies=%d TotalAI=%d"),
		DummyCount,
		GetSpawnedAICount());
}

void AAscAIStressTestDirector::ResetStressTest()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	UAIPoolSubsystem* Pool = World ? World->GetSubsystem<UAIPoolSubsystem>() : nullptr;

	for (int32 ZoneIndex = 0; ZoneIndex < MaxStressZones; ++ZoneIndex)
	{
		for (const TWeakObjectPtr<ABaseAICharacter>& AIWeak : ZoneAIs[ZoneIndex])
		{
			ABaseAICharacter* AI = AIWeak.Get();
			if (!AI)
			{
				continue;
			}

			if (ABaseAIController* AIC = Cast<ABaseAIController>(AI->GetController()))
			{
				AIC->ClearTargetActor();
				AIC->SetAutoTargetFromPerceptionEnabled(true);
			}

			if (Pool)
			{
				Pool->ReleaseAI(AI);
			}
		}
		ZoneAIs[ZoneIndex].Reset();

		if (AAscAITestDummy* Dummy = ZoneDummies[ZoneIndex].Get())
		{
			Dummy->Destroy();
		}
		ZoneDummies[ZoneIndex].Reset();
	}

	NextZoneIndex = 0;
	UE_LOG(LogTemp, Warning, TEXT("[AIStress] Reset complete."));
}

int32 AAscAIStressTestDirector::GetSpawnedAICount() const
{
	int32 Count = 0;
	for (int32 ZoneIndex = 0; ZoneIndex < MaxStressZones; ++ZoneIndex)
	{
		for (const TWeakObjectPtr<ABaseAICharacter>& AI : ZoneAIs[ZoneIndex])
		{
			if (AI.IsValid())
			{
				++Count;
			}
		}
	}
	return Count;
}
