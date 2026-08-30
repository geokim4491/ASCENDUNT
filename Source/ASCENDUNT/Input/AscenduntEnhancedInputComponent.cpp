// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntEnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

UAscenduntEnhancedInputComponent::UAscenduntEnhancedInputComponent(const FObjectInitializer& ObjectInitializer)
{
}

void UAscenduntEnhancedInputComponent::AddInputMappings(const UAscenduntInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	// Here you can handle any custom logic to add something from your input config if required
}

void UAscenduntEnhancedInputComponent::RemoveInputMappings(const UAscenduntInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	// Here you can handle any custom logic to remove input mappings that you may have added above
}

void UAscenduntEnhancedInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
