// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "GameplayEffectTypes.h"

#include "AscenduntGameplayEffectContext.generated.h"

class IAscenduntAbilitySourceInterface;
class AActor;
class FArchive;
class UObject;

/**
 * 
 */
USTRUCT()
struct FAscenduntGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()
	
	FAscenduntGameplayEffectContext()
		: FGameplayEffectContext()
	{
	}
	
	FAscenduntGameplayEffectContext(AActor* InInstigator, AActor* InEffectCauser)
	: FGameplayEffectContext(InInstigator, InEffectCauser)
	{
	}
	
	/** Returns the wrapped FAscenduntGameplayEffectContext from the handle, or nullptr if it doesn't exist or is the wrong type */
	static ASCENDUNT_API FAscenduntGameplayEffectContext* ExtractEffectContext(struct FGameplayEffectContextHandle Handle);

	/** Sets the object used as the ability source */
	void SetAbilitySource(const IAscenduntAbilitySourceInterface* InObject, float InSourceLevel);

	/** Returns the ability source interface associated with the source object. Only valid on the authority. */
	const IAscenduntAbilitySourceInterface* GetAbilitySource() const;

	virtual FGameplayEffectContext* Duplicate() const override
	{
		FAscenduntGameplayEffectContext* NewContext = new FAscenduntGameplayEffectContext();
		*NewContext = *this;
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FAscenduntGameplayEffectContext::StaticStruct();
	}

	/** Overridden to serialize new fields */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

public:
	/** ID to allow the identification of multiple bullets that were part of the same cartridge */
	UPROPERTY()
	int32 CartridgeID = -1;

protected:
	/** Ability Source object (should implement IAscenduntAbilitySystemInterface). NOT replicated currently */
	UPROPERTY()
	TWeakObjectPtr<const UObject> AbilitySourceObject;
};

template<>
struct TStructOpsTypeTraits<FAscenduntGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FAscenduntGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
