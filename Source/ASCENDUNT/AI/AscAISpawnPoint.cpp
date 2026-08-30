// Fill out your copyright notice in the Description page of Project Settings.

#include "AscAISpawnPoint.h"
#include "Components/SceneComponent.h"

AAscAISpawnPoint::AAscAISpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
}

FTransform AAscAISpawnPoint::MakeSpawnTransform() const
{
	FTransform SpawnTransform = GetActorTransform();

	if (SpawnRadius <= 0.0f)
	{
		return SpawnTransform;
	}

	// 원 내부에 균등 분포시키기 위해 거리 값에 sqrt를 적용한다.
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = SpawnRadius * FMath::Sqrt(FMath::FRand());
	const FVector Offset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
	SpawnTransform.AddToTranslation(Offset);

	return SpawnTransform;
}
