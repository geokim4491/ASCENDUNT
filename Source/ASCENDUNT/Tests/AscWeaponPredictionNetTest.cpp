#include "AscWeaponPredictionNetTest.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_FireControlBase.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Weapon/GA_FireShot.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ASCENDUNT/Weapon/AscenduntProjectile.h"
#include "NiagaraSystem.h"
#include "Components/BoxComponent.h"

namespace
{
	FString FixtureCase()
	{
		FString Value;
		FParse::Value(FCommandLine::Get(), TEXT("WeaponTestCase="), Value);
		return Value;
	}
}

AAscWeaponPredictionTestWeapon::AAscWeaponPredictionTestWeapon()
{
	WeaponData = CreateDefaultSubobject<UWeaponDefinitionData>(TEXT("FixtureWeaponData"));
	WeaponData->WeaponStatsEffect = nullptr;
	if (WeaponData->Modes.IsEmpty()) WeaponData->Modes.AddDefaulted();
	WeaponData->Modes[0].Stats.MagSize = 20;
	WeaponData->Modes[0].Stats.RateOfFire = 10.f;
	WeaponData->Modes[0].Stats.ShotsPerTrigger = 3;
	WeaponData->Modes[0].Stats.AmmoCostPerShot = 1;
	const auto SecondMode = WeaponData->Modes[0];
	WeaponData->Modes.Add(SecondMode);
	WeaponData->Modes[1].Stats.MagSize = 5;
	WeaponData->Modes[0].Stats.FireTraceVFXNiagara = LoadObject<UNiagaraSystem>(nullptr,
		TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer.NS_WeaponFire_Tracer"));
	WeaponData->Modes[0].Stats.bInfiniteMag = FixtureCase() == TEXT("infinite");
	if (FixtureCase() == TEXT("projectile"))
	{
		WeaponData->Modes[0].Stats.FireType = EAscenduntWeaponFireType::Projectile;
		WeaponData->Modes[0].Stats.ProjectileClass = AAscenduntProjectile::StaticClass();
	}
}

AAscWeaponPredictionTestWall::AAscWeaponPredictionTestWall()
{
	bReplicates = true;
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(TEXT("WorldImpactSurface"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(5.f, 5000.f, 5000.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
}

AAscWeaponPredictionTestCharacter::AAscWeaponPredictionTestCharacter()
{
	GetCharacterMovement()->GravityScale = 0.f;
}

UAbilitySystemComponent* AAscWeaponPredictionTestCharacter::GetAbilitySystemComponent() const
{
	const AAscenduntPlayerState* State = GetPlayerState<AAscenduntPlayerState>();
	return State ? State->GetAscenduntAbilitySystemComponent() : nullptr;
}

void AAscWeaponPredictionTestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UAscenduntAbilitySystemComponent* ASC = Cast<UAscenduntAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!ASC) return;
	ASC->InitAbilityActorInfo(GetPlayerState(), this);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetMagSizeAttribute(), 20.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetAmmoCostPerShotAttribute(), 1.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetRateOfFireAttribute(), 10.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetShotsPerTriggerAttribute(), 3.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetReloadTimeAttribute(), .4f);
	if (FixtureCase() == TEXT("infinite"))
	{
		ASC->AddLooseGameplayTag(AscenduntGameplayTags::Data_Weapon_InfiniteMag, 1, EGameplayTagReplicationState::TagOnly);
	}
	if (FixtureCase() == TEXT("reload")) ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetMagSizeAttribute(), 3.f);
	TestWeapon = GetWorld()->SpawnActor<AAscWeaponPredictionTestWeapon>();
	TestWeapon->SetOwner(this);
	TestWeapon->SetInstigator(this);
	TestWeapon->EquipToASC(ASC);
	GrantFixtureAbilities();
}

void AAscWeaponPredictionTestCharacter::GrantFixtureAbilities()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	for (const TCHAR* Path : {
		TEXT("/Game/AbilitySystem/GA/Weapon/BPGA_Fire_Auto.BPGA_Fire_Auto_C"),
		TEXT("/Game/AbilitySystem/GA/Weapon/BPGA_Fire_Charge.BPGA_Fire_Charge_C")})
	{
		UClass* Class = LoadClass<UGA_FireControlBase>(nullptr, Path);
		const UGA_FireControlBase* Defaults = Class ? Class->GetDefaultObject<UGA_FireControlBase>() : nullptr;
		if (!Defaults || !Defaults->ShotAbilityClass) continue;
		FGameplayAbilitySpec Control(Class, 1, INDEX_NONE, TestWeapon);
		Control.GetDynamicSpecSourceTags().AddTag(FCString::Strstr(Path, TEXT("Charge"))
			? AscenduntGameplayTags::Ability_Weapon_Melee : AscenduntGameplayTags::Ability_Weapon_Fire);
		ASC->GiveAbility(Control);
		ASC->GiveAbility(FGameplayAbilitySpec(Defaults->ShotAbilityClass, 1, INDEX_NONE, TestWeapon));
	}
	if (UClass* ReloadClass = LoadClass<UGameplayAbility>(nullptr, TEXT("/Game/AbilitySystem/GA/Weapon/BPGA_Reload.BPGA_Reload_C")))
		ASC->GiveAbility(FGameplayAbilitySpec(ReloadClass, 1, INDEX_NONE, TestWeapon));
}

void AAscWeaponPredictionTestCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent()) ASC->InitAbilityActorInfo(GetPlayerState(), this);
}

void AAscWeaponPredictionTestCharacter::OnRep_TestWeapon(AAscenduntWeapon* PreviousWeapon)
{
	if (PreviousWeapon) { PreviousWeapon->ClearPredictedWeaponState(); PreviousWeapon->UnbindAmmoAttributes(); }
	if (TestWeapon) TestWeapon->BindAmmoAttributes();
}

void AAscWeaponPredictionTestCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAscWeaponPredictionTestCharacter, TestWeapon);
}

AAscWeaponPredictionTestController::AAscWeaponPredictionTestController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAscWeaponPredictionTestController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController()) return;
	AAscWeaponPredictionTestCharacter* FixturePawn = Cast<AAscWeaponPredictionTestCharacter>(GetPawn());
	UAscenduntAbilitySystemComponent* ASC = FixturePawn ? Cast<UAscenduntAbilitySystemComponent>(FixturePawn->GetAbilitySystemComponent()) : nullptr;
	if (StartTime < 0. && ++WaitFrames % 120 == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] WAIT Pawn=%s Weapon=%s Owner=%s ASC=%s Specs=%d Ammo=%d Raw=%.0f"),
			*GetNameSafe(FixturePawn), *GetNameSafe(FixturePawn ? FixturePawn->TestWeapon.Get() : nullptr),
			*GetNameSafe(FixturePawn && FixturePawn->TestWeapon ? FixturePawn->TestWeapon->GetOwner() : nullptr), *GetNameSafe(ASC),
			ASC ? ASC->GetActivatableAbilities().Num() : -1,
			FixturePawn && FixturePawn->TestWeapon ? FixturePawn->TestWeapon->GetCurrentAmmoInMag() : -1,
			ASC ? ASC->GetNumericAttribute(UAscenduntWeaponSet::GetCurrentAmmoAttribute()) : -1.f);
	}
	if (!FixturePawn || !FixturePawn->TestWeapon || !ASC || ASC->GetActivatableAbilities().Num() < 5) return;
	if (StartTime < 0.)
	{
		if (FixturePawn->TestWeapon->GetCurrentAmmoInMag() != (FixtureCase() == TEXT("reload") ? 3 : 20)) return;
		if (ReadyTime < 0.) ReadyTime = GetWorld()->GetTimeSeconds();
		if (GetWorld()->GetTimeSeconds() - ReadyTime < 1.) return;
		StartTime = GetWorld()->GetTimeSeconds();
		UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] CLIENT_READY Pawn=%s Weapon=%s"), *GetNameSafe(FixturePawn), *GetNameSafe(FixturePawn->TestWeapon));
		AmmoHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute())
			.AddWeakLambda(this, [this](const FOnAttributeChangeData& Data)
			{
				++AmmoEvents;
				UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] UI Old=%.0f New=%.0f"), Data.OldValue, Data.NewValue);
			});
		Server_StartScenario();
		ASC->AbilityInputTagPressed(FixtureCase() == TEXT("cancel") ? AscenduntGameplayTags::Ability_Weapon_Melee : AscenduntGameplayTags::Ability_Weapon_Fire);
		Stage = 1;
	}
	const double Elapsed = GetWorld()->GetTimeSeconds() - StartTime;
	if (FixtureCase() == TEXT("rapid") && Stage == 1 && Elapsed < 1.6)
	{
		const int32 Phase = FMath::FloorToInt(Elapsed / .05);
		if (Phase != FMath::FloorToInt((Elapsed - DeltaTime) / .05))
		{
			if (Phase % 2 == 0) ASC->AbilityInputTagPressed(AscenduntGameplayTags::Ability_Weapon_Fire);
			else ASC->AbilityInputTagReleased(AscenduntGameplayTags::Ability_Weapon_Fire);
		}
	}
	if (Stage == 1 && Elapsed > (FixtureCase() == TEXT("reload") ? 3.2 : 1.6))
	{
		ASC->AbilityInputTagReleased(AscenduntGameplayTags::Ability_Weapon_Fire);
		if (FixtureCase() == TEXT("cancel")) ASC->AbilityInputTagReleased(AscenduntGameplayTags::Ability_Weapon_Melee);
		Stage = FixtureCase().IsEmpty() || FixtureCase() == TEXT("infinite") || FixtureCase() == TEXT("projectile") ? 2 : 4;
	}
	if (Stage == 2 && Elapsed > 2.3) { ASC->AbilityInputTagPressed(AscenduntGameplayTags::Ability_Weapon_Melee); Stage = 3; }
	if (Stage == 3 && Elapsed > 2.8) { ASC->AbilityInputTagReleased(AscenduntGameplayTags::Ability_Weapon_Melee); Stage = 4; }
	if (Stage == 4 && Elapsed > 5.5)
	{
		Server_ReportResults(FixturePawn->TestWeapon->GetCurrentAmmoInMag(), AmmoEvents);
		UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] CLIENT_SETTLED Ammo=%d UIEvents=%d"), FixturePawn->TestWeapon->GetCurrentAmmoInMag(), AmmoEvents);
		Stage = 5;
	}
	ASC->ProcessAbilityInput(DeltaTime, false);
}

void AAscWeaponPredictionTestController::Server_StartScenario_Implementation()
{
	StartTime = GetWorld()->GetTimeSeconds();
	if (const auto* ServerPawn = Cast<AAscWeaponPredictionTestCharacter>(GetPawn()))
		if (UAbilitySystemComponent* ASC = ServerPawn->GetAbilitySystemComponent())
			ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute()).AddWeakLambda(this,
				[this](const FOnAttributeChangeData& Data) { if (Data.NewValue > Data.OldValue) ++ReloadRefills; });
	if (FixtureCase().IsEmpty() || FixtureCase() == TEXT("cancel"))
	{
		GetWorldTimerManager().SetTimer(BlockTimer, this, &ThisClass::BlockShots, 0.55f, false);
		if (FixtureCase().IsEmpty()) GetWorldTimerManager().SetTimer(UnblockTimer, this, &ThisClass::UnblockShots, 1.15f, false);
	}
	if (FixtureCase() == TEXT("swap") || FixtureCase() == TEXT("mode") || FixtureCase() == TEXT("avatar"))
		GetWorldTimerManager().SetTimer(BlockTimer, this, &ThisClass::TransitionWeapon, .55f, false);
	UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] SERVER_START"));
}

void AAscWeaponPredictionTestController::TransitionWeapon()
{
	auto* TestPawn = Cast<AAscWeaponPredictionTestCharacter>(GetPawn());
	auto* ASC = TestPawn ? Cast<UAscenduntAbilitySystemComponent>(TestPawn->GetAbilitySystemComponent()) : nullptr;
	if (!TestPawn || !ASC || !TestPawn->TestWeapon) return;
	if (FixtureCase() == TEXT("avatar"))
	{
		AAscWeaponPredictionTestCharacter* NextPawn = GetWorld()->SpawnActor<AAscWeaponPredictionTestCharacter>(FVector(0.f, 300.f, 0.f), FRotator::ZeroRotator);
		Possess(NextPawn);
		UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] AVATAR_CHANGED Ammo=%d"), NextPawn->TestWeapon->GetCurrentAmmoInMag());
		return;
	}
	if (FixtureCase() == TEXT("mode"))
	{
		ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetMagSizeAttribute(), 5.f);
		TestPawn->TestWeapon->SetWeaponModeByIndex(1);
	}
	else
	{
		AAscenduntWeapon* Previous = TestPawn->TestWeapon;
		ASC->CancelAllAbilities();
		ASC->ClearAllAbilities();
		Previous->UnequipFromASC(ASC);
		Previous->SetOwner(nullptr);
		TestPawn->TestWeapon = GetWorld()->SpawnActor<AAscWeaponPredictionTestWeapon>();
		TestPawn->TestWeapon->SetOwner(TestPawn);
		TestPawn->TestWeapon->SetInstigator(TestPawn);
		TestPawn->TestWeapon->EquipToASC(ASC);
		TestPawn->TestWeapon->SetAmmoInMag_Server(7);
		TestPawn->GrantFixtureAbilities();
	}
	UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] TRANSITION Case=%s Ammo=%d"), *FixtureCase(), TestPawn->TestWeapon->GetCurrentAmmoInMag());
}

void AAscWeaponPredictionTestController::BlockShots()
{
	if (const auto* FixturePawn = Cast<AAscWeaponPredictionTestCharacter>(GetPawn()))
		if (UAbilitySystemComponent* ASC = FixturePawn->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(AscenduntGameplayTags::Status_PvE_Downed);
			if (FixtureCase() == TEXT("cancel")) ASC->CancelAllAbilities();
		}
	UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] SERVER_BLOCK"));
}

void AAscWeaponPredictionTestController::UnblockShots()
{
	if (const auto* FixturePawn = Cast<AAscWeaponPredictionTestCharacter>(GetPawn()))
		if (UAbilitySystemComponent* ASC = FixturePawn->GetAbilitySystemComponent())
			ASC->RemoveLooseGameplayTag(AscenduntGameplayTags::Status_PvE_Downed);
	UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] SERVER_UNBLOCK"));
}

void AAscWeaponPredictionTestController::Server_ReportResults_Implementation(int32 ClientAmmo, int32 UIEvents)
{
	const auto* FixturePawn = Cast<AAscWeaponPredictionTestCharacter>(GetPawn());
	const int32 ServerAmmo = FixturePawn && FixturePawn->TestWeapon ? FixturePawn->TestWeapon->GetCurrentAmmoInMag() : -1;
	const bool bAmmoExpected = FixtureCase() == TEXT("infinite") || FixtureCase() == TEXT("cancel") || FixtureCase() == TEXT("avatar") ? ServerAmmo == 20
		: FixtureCase() == TEXT("swap") ? ServerAmmo == 7 : UIEvents >= 5;
	if (ServerAmmo != ClientAmmo || !bAmmoExpected || (FixtureCase() == TEXT("reload") && ReloadRefills < 1))
	{
		UE_LOG(LogTemp, Error, TEXT("[WeaponNetTest] FAIL ServerAmmo=%d ClientAmmo=%d UIEvents=%d"), ServerAmmo, ClientAmmo, UIEvents);
	}
	else { UE_LOG(LogTemp, Display, TEXT("[WeaponNetTest] PASS ServerAmmo=%d ClientAmmo=%d UIEvents=%d Refills=%d Case=%s"), ServerAmmo, ClientAmmo, UIEvents, ReloadRefills, *FixtureCase()); }
}

AAscWeaponPredictionTestGameMode::AAscWeaponPredictionTestGameMode()
{
	DefaultPawnClass = AAscWeaponPredictionTestCharacter::StaticClass();
	PlayerControllerClass = AAscWeaponPredictionTestController::StaticClass();
	PlayerStateClass = AAscenduntPlayerState::StaticClass();
}

void AAscWeaponPredictionTestGameMode::StartPlay()
{
	GetWorld()->SpawnActor<APlayerStart>();
	GetWorld()->SpawnActor<AAscWeaponPredictionTestWall>(FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	Super::StartPlay();
}
