// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AscAISpawnPoint.generated.h"

/**
 * AAISpawner가 사용할 실제 AI 생성 지점.
 *
 * SpawnRadius가 0이면 Actor Transform을 그대로 사용하고,
 * 0보다 크면 XY 평면의 원 내부에서 균등하게 위치를 흩뿌린다.
 */
UCLASS(BlueprintType)
class ASCENDUNT_API AAscAISpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AAscAISpawnPoint();

	FTransform MakeSpawnTransform() const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ascendunt|Spawner", meta = (ClampMin = "0.0"))
	float SpawnRadius = 0.0f;
};
