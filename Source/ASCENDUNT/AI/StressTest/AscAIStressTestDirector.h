#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AscAIStressTestDirector.generated.h"

class ABaseAICharacter;
class AAscAITestDummy;
class UAIPawnData;

/**
 * Dedicated Server AI 성능 측정용 Director.
 *
 * ZoneCenters[0..3]을 맵 네 구역의 중앙에 배치한다.
 * SpawnNextZone()을 호출할 때마다 다음 Zone에 정확히 AIPerZone만큼 추가한다.
 * SpawnDummiesAndAssignTargets()는 네 Zone 중앙에 Dummy를 만들고 해당 Zone AI에게
 * TargetActor를 직접 지정해 Combat workload를 결정적으로 만든다.
 */
UCLASS(Blueprintable)
class ASCENDUNT_API AAscAIStressTestDirector : public AActor
{
	GENERATED_BODY()

public:
	AAscAIStressTestDirector();

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI Stress Test")
	void SpawnNextZone();

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI Stress Test")
	void SpawnDummiesAndAssignTargets();

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|AI Stress Test")
	void ResetStressTest();

	UFUNCTION(BlueprintPure, Category = "Ascendunt|AI Stress Test")
	int32 GetSpawnedZoneCount() const { return NextZoneIndex; }

	UFUNCTION(BlueprintPure, Category = "Ascendunt|AI Stress Test")
	int32 GetSpawnedAICount() const;

protected:
	/** 반드시 0~3 순서로 맵 네 구역의 중앙 TargetPoint를 지정한다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test")
	TArray<TObjectPtr<AActor>> ZoneCenters;

	/** 4개 Zone 모두 같은 AI Data를 사용해 workload를 통제한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test")
	TObjectPtr<UAIPawnData> SpawnData;

	/** 기본 native Dummy를 써도 되고, VisualMesh를 지정한 BP 자식을 넣어도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test")
	TSubclassOf<AAscAITestDummy> DummyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test", meta = (ClampMin = "1"))
	int32 AIPerZone = 50;

	/** 50마리 기준 기본 10 Columns x 5 Rows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test|Spawn Grid", meta = (ClampMin = "1"))
	int32 SpawnGridColumns = 10;

	/** 랜덤 Spawn 대신 고정 Grid를 사용해 반복 측정 시 초기 조건을 동일하게 만든다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test|Spawn Grid", meta = (ClampMin = "1.0"))
	float SpawnSpacing = 180.f;

	/** ZoneCenter가 지면 TargetPoint인 경우 Dummy capsule 중심까지 올릴 높이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|AI Stress Test|Dummy")
	float DummyZOffset = 96.f;

private:
	static constexpr int32 MaxStressZones = 4;

	FTransform MakeGridSpawnTransform(int32 ZoneIndex, int32 LocalAIIndex) const;
	ABaseAICharacter* SpawnOneAI(int32 ZoneIndex, int32 LocalAIIndex);
	AAscAITestDummy* EnsureDummyForZone(int32 ZoneIndex);
	void AssignZoneTarget(int32 ZoneIndex, AActor* TargetActor);
	bool IsZoneConfigured(int32 ZoneIndex) const;

	int32 NextZoneIndex = 0;
	TArray<TWeakObjectPtr<ABaseAICharacter>> ZoneAIs[MaxStressZones];
	TWeakObjectPtr<AAscAITestDummy> ZoneDummies[MaxStressZones];
};
