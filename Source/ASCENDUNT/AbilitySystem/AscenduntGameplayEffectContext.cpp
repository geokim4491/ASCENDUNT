// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntGameplayEffectContext.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySourceInterface.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Serialization/GameplayEffectContextNetSerializer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AscenduntGameplayEffectContext)

ASCENDUNT_API FAscenduntGameplayEffectContext* FAscenduntGameplayEffectContext::ExtractEffectContext(
	FGameplayEffectContextHandle Handle)
{
	FGameplayEffectContext* BaseEffectContext = Handle.Get();
	if ((BaseEffectContext != nullptr) && BaseEffectContext->GetScriptStruct()
		->IsChildOf(FAscenduntGameplayEffectContext::StaticStruct()))
	{
		return (FAscenduntGameplayEffectContext*)BaseEffectContext;
	}

	return nullptr;
}

#if UE_WITH_IRIS
namespace UE::Net
{
	// Forward to FGameplayEffectContextNetSerializer
	// Note: If FAscenduntGameplayEffectContext::NetSerialize() is modified, a custom NetSerializesr must be implemented as the current fallback will no longer be sufficient.
	UE_NET_IMPLEMENT_FORWARDING_NETSERIALIZER_AND_REGISTRY_DELEGATES(AscenduntGameplayEffectContext, FGameplayEffectContextNetSerializer);
}
#endif

void FAscenduntGameplayEffectContext::SetAbilitySource(const IAscenduntAbilitySourceInterface* InObject, float InSourceLevel)
{
	AbilitySourceObject = MakeWeakObjectPtr(Cast<const UObject>(InObject));
	//SourceLevel = InSourceLevel;
}

const IAscenduntAbilitySourceInterface* FAscenduntGameplayEffectContext::GetAbilitySource() const
{
	return Cast<IAscenduntAbilitySourceInterface>(AbilitySourceObject.Get());
}

bool FAscenduntGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

	// Not serialized for post-activation use:
	// CartridgeID

	return true;
}
