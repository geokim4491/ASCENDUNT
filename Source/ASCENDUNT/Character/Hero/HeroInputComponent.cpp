// Fill out your copyright notice in the Description page of Project Settings.

#include "HeroInputComponent.h"
#include "HeroCharacter.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/Input/AscenduntInputConfig.h"
#include "ASCENDUNT/Input/AscenduntEnhancedInputComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Player/AscenduntPlayerController.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/System/AscGameUserSettings.h"

#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"

UHeroInputComponent::UHeroInputComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	if (InputMappings.Num() == 0)
	{
		static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_DefaultAsset(TEXT("/Game/Input/IMC/IMC_Default"));
		static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_UIAsset(TEXT("/Game/Input/IMC/IMC_UI"));

		if (IMC_DefaultAsset.Succeeded())
		{
			InputMappings.Add(FInputMappingContextAndPriority(IMC_DefaultAsset.Object, 1));
		}

		if (IMC_UIAsset.Succeeded())
		{
			InputMappings.Add(FInputMappingContextAndPriority(IMC_UIAsset.Object, 0));
		}
	}
	
}

// Called when the game starts
void UHeroInputComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UHeroInputComponent::SetADSActive(bool bEnabled)
{
	bADSActive = bEnabled;
}

void UHeroInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedPawn.IsValid() || !CachedPawn->IsLocallyControlled())
		return;

	if (CachedASC.IsValid())
	{
		CachedASC->ProcessAbilityInput(DeltaTime, false);
	}
}

bool UHeroInputComponent::CanInitializePlayerInput(UInputComponent* PlayerInputComponent) const
{
	if (!PlayerInputComponent)
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || Pawn->GetNetMode() != NM_Client || !Pawn->IsLocallyControlled())
	{
		return false;
	}

	const AAscenduntPlayerState* PS = Pawn->GetPlayerState<AAscenduntPlayerState>();
	if (!PS || !PS->GetPawnData() || !PS->GetPawnData()->InputConfig)
	{
		return false;
	}

	const AAscenduntPlayerController* PC = Cast<AAscenduntPlayerController>(Pawn->GetController());
	const UAscenduntAbilitySystemComponent* ASC = PS->GetAscenduntAbilitySystemComponent();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedSubsystem();
	const UAscenduntEnhancedInputComponent* AscenduntIC = Cast<UAscenduntEnhancedInputComponent>(PlayerInputComponent);

	return PC && ASC && Subsystem && AscenduntIC;
}

bool UHeroInputComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	if (!PlayerInputComponent)
	{
		return false;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || Pawn->GetNetMode() != NM_Client || !Pawn->IsLocallyControlled())
	{
		return false;
	}

	AAscenduntPlayerState* PS = Pawn->GetPlayerState<AAscenduntPlayerState>();
	if (!PS || !PS->GetPawnData() || !PS->GetPawnData()->InputConfig)
	{
		return false;
	}

	AAscenduntPlayerController* PC = Cast<AAscenduntPlayerController>(Pawn->GetController());
	UAscenduntAbilitySystemComponent* ASC = PS->GetAscenduntAbilitySystemComponent();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedSubsystem();
	UAscenduntEnhancedInputComponent* AscenduntIC = Cast<UAscenduntEnhancedInputComponent>(PlayerInputComponent);
	if (!PC || !ASC || !Subsystem || !AscenduntIC)
	{
		return false;
	}

	const UAscenduntInputConfig* InputConfig = PS->GetPawnData()->InputConfig;

	// Only commit cached state after every required local dependency is valid.
	CachedPawn = Pawn;
	CachedASC = ASC;
	CachedInputSubsystem = Subsystem;
	CachedPlayerController = PC;

	PC->RegisterActiveHeroInputComponent(this);

	for (const TObjectPtr<const UInputMappingContext>& IMC : ActiveHeroIMCs)
	{
		if (IMC)
		{
			Subsystem->RemoveMappingContext(const_cast<UInputMappingContext*>(IMC.Get()));
		}
	}
	ActiveHeroIMCs.Reset();

	for (const FInputMappingContextAndPriority& Mapping : InputMappings)
	{
		UInputMappingContext* IMC = Mapping.InputMapping.Get();
		if (!IMC)
		{
			continue;
		}

		if (UEnhancedInputUserSettings* Settings = Subsystem->GetUserSettings())
		{
			Settings->RegisterInputMappingContext(IMC);
		}

		Subsystem->AddMappingContext(IMC, Mapping.Priority);
		ActiveHeroIMCs.Add(IMC);
	}

	AscenduntIC->AddInputMappings(InputConfig, Subsystem);

	TArray<uint32> BindHandles;
	AscenduntIC->BindAbilityActions(
		InputConfig,
		this,
		&ThisClass::Input_AbilityInputTagPressed,
		&ThisClass::Input_AbilityInputTagReleased,
		BindHandles);

	AscenduntIC->BindNativeAction(
		InputConfig,
		FGameplayTag::RequestGameplayTag(FName("InputTag.Move")),
		ETriggerEvent::Triggered,
		this,
		&ThisClass::Input_Move,
		false);

	AscenduntIC->BindNativeAction(
		InputConfig,
		FGameplayTag::RequestGameplayTag(FName("InputTag.Look")),
		ETriggerEvent::Triggered,
		this,
		&ThisClass::Input_Look,
		false);

	SetComponentTickEnabled(true);
	return true;
}

void UHeroInputComponent::UninitializePlayerInput(bool bForceRemoveMappings)
{
	AAscenduntPlayerController* PC = CachedPlayerController.Get();
	const bool bOwnsCurrentMappings = PC && PC->IsActiveHeroInputComponent(this);
	const bool bShouldRemoveMappings = bForceRemoveMappings || bOwnsCurrentMappings || !PC;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = CachedInputSubsystem.Get();
	if (!Subsystem)
	{
		Subsystem = GetEnhancedSubsystem();
	}

	if (bShouldRemoveMappings && Subsystem)
	{
		for (const TObjectPtr<const UInputMappingContext>& IMC : ActiveHeroIMCs)
		{
			if (IMC)
			{
				Subsystem->RemoveMappingContext(const_cast<UInputMappingContext*>(IMC.Get()));
			}
		}
	}

	if (PC)
	{
		PC->UnregisterActiveHeroInputComponent(this);
	}

	ActiveHeroIMCs.Reset();
	CachedPawn = nullptr;
	CachedASC = nullptr;
	CachedInputSubsystem = nullptr;
	CachedPlayerController = nullptr;
	SetADSActive(false);
	SetComponentTickEnabled(false);
}

void UHeroInputComponent::RemoveIMC(UInputMappingContext* Context)
{
	if (!Context) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedSubsystem();
	if (!Subsystem) return;

	if (!ActiveHeroIMCs.Contains(Context)) return;

	Subsystem->RemoveMappingContext(Context);
	ActiveHeroIMCs.Remove(Context);
}

void UHeroInputComponent::ApplyIMC(UInputMappingContext* Context)
{
	if (!Context) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedSubsystem();
	if (!Subsystem) return;

	if (ActiveHeroIMCs.Contains(Context)) return;

	const FInputMappingContextAndPriority* Found =
		InputMappings.FindByPredicate([Context](const FInputMappingContextAndPriority& E)
			{
				return E.InputMapping == Context;
			});

	const int32 Priority = Found ? Found->Priority : 0;
	Subsystem->AddMappingContext(Context, Priority);
	ActiveHeroIMCs.Add(Context);

}

void UHeroInputComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if(const AActor* Owner = GetOwner())
	{
		if(const AHeroCharacter* Hero = Cast<AHeroCharacter>(Owner))
		{
			if(UAscenduntAbilitySystemComponent* AscenduntASC = Hero->GetAscenduntAbilitySystemComponent())
			{
				AscenduntASC->AbilityInputTagPressed(InputTag);
			}
		}
	}
}

void UHeroInputComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (const AActor* Owner = GetOwner())
	{
		if (const AHeroCharacter* Hero = Cast<AHeroCharacter>(Owner))
		{
			if (UAscenduntAbilitySystemComponent* AscenduntASC = Hero->GetAscenduntAbilitySystemComponent())
			{
				AscenduntASC->AbilityInputTagReleased(InputTag);
			}
		}
	}
}

void UHeroInputComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;

	if (Controller)
	{
		const FVector2D Value = InputActionValue.Get<FVector2D>();
		const FRotator MovementRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);

		if (Value.X != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);
			Pawn->AddMovementInput(MovementDirection, Value.X);
		}

		if (Value.Y != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			Pawn->AddMovementInput(MovementDirection, Value.Y);
		}
	}
}

void UHeroInputComponent::Input_Look(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = Cast<APawn>(GetOwner());

	if (Pawn)
	{
		const FVector2D Value = InputActionValue.Get<FVector2D>();

		float SensitivityScalar = 1.0f;
		float PitchDirection = 1.0f;
		if (const UAscGameUserSettings* Settings = UAscGameUserSettings::GetAscGameUserSettings())
		{
			SensitivityScalar = Settings->GetMouseSensitivity();
			if (bADSActive)
			{
				SensitivityScalar *= Settings->GetADSSensitivityMultiplier();
			}
			PitchDirection = Settings->IsInvertYAxisEnabled() ? -1.0f : 1.0f;
		}

		SensitivityScalar *= 0.3f;

		if (Value.X != 0.0f)
		{
			Pawn->AddControllerYawInput(Value.X * SensitivityScalar);
		}

		if (Value.Y != 0.0f)
		{
			Pawn->AddControllerPitchInput(Value.Y * SensitivityScalar * PitchDirection);
		}
	}
}

void UHeroInputComponent::Input_Crouch(const FInputActionValue& InputActionValue)
{
}

UEnhancedInputLocalPlayerSubsystem* UHeroInputComponent::GetEnhancedSubsystem() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
		return nullptr;

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
		return nullptr;

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
		return nullptr;

	return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
}
