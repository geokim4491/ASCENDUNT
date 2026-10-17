// Fill out your copyright notice in the Description page of Project Settings.


#include "HeroPawnData.h"
#include "ASCENDUNT/Input/AscenduntInputConfig.h"

UHeroPawnData::UHeroPawnData(const FObjectInitializer& ObjectInitializer) 
	: UPrimaryDataAsset(ObjectInitializer)
{
	PawnClass = nullptr;

	static ConstructorHelpers::FObjectFinder<UAscenduntInputConfig> InputDataAsset(TEXT("/Game/Input/InputData_Hero"));
	if (InputDataAsset.Succeeded())
	{
		InputConfig = InputDataAsset.Object;
	}

	// DefaultCameraMode = nullptr;
}
