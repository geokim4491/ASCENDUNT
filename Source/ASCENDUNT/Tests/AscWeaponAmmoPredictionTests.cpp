#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/AbilitySystem/Effects/AscWeaponAmmoCost.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"
#include "GameplayPrediction.h"
#include "ASCENDUNT/AbilitySystem/Abilities/AI/GA_AIFire_Auto.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscWeaponAmmoPredictionTest, "ASCENDUNT.WeaponPrediction.Ammo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscWeaponAmmoPredictionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AAscenduntPlayerState* State = World->SpawnActor<AAscenduntPlayerState>();
	UAscenduntAbilitySystemComponent* ASC = State->GetAscenduntAbilitySystemComponent();
	ASC->InitAbilityActorInfo(State, State);
	if (!ASC->GetSet<UAscenduntWeaponSet>()) ASC->AddAttributeSetSubobject(NewObject<UAscenduntWeaponSet>(State));
	AAscenduntWeapon* Weapon = World->SpawnActor<AAscenduntWeapon>();
	Weapon->WeaponData = NewObject<UWeaponDefinitionData>(Weapon);
	Weapon->WeaponData->Modes.AddDefaulted();
	Weapon->WeaponData->Modes[0].Stats.MagSize = 10;
	Weapon->SetOwner(State);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetMagSizeAttribute(), 10.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetCurrentAmmoAttribute(), 10.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetAmmoCostPerShotAttribute(), 1.f);
	int32 UIEvents = 0;
	const FDelegateHandle Delegate = ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute())
		.AddLambda([&UIEvents](const FOnAttributeChangeData&) { ++UIEvents; });
	const auto ApplyCost = [ASC, Weapon](FPredictionKey Key = FPredictionKey())
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(Weapon);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UAscWeaponAmmoCost::StaticClass(), 1.f, Context);
		return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get(), Key);
	};
	ApplyCost();
	TestEqual(TEXT("Authoritative cost"), Weapon->GetCurrentAmmoInMag(), 9);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetAmmoCostPerShotAttribute(), 3.f);
	ApplyCost();
	TestEqual(TEXT("Configured multi-ammo trigger cost"), Weapon->GetCurrentAmmoInMag(), 6);
	Weapon->WeaponData->Modes[0].Stats.bInfiniteMag = true;
	ApplyCost();
	TestEqual(TEXT("Infinite magazine has no cost"), Weapon->GetCurrentAmmoInMag(), 6);
	Weapon->WeaponData->Modes[0].Stats.bInfiniteMag = false;
	ASC->AddLooseGameplayTag(AscenduntGameplayTags::Data_Weapon_InfiniteMag);
	ApplyCost();
	TestEqual(TEXT("GAS-granted infinite ammo also skips cost"), Weapon->GetCurrentAmmoInMag(), 6);
	ASC->RemoveLooseGameplayTag(AscenduntGameplayTags::Data_Weapon_InfiniteMag);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetCurrentAmmoAttribute(), 10.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetAmmoCostPerShotAttribute(), 1.f);
	Weapon->ConsumeAmmo_Server(2);
	TestEqual(TEXT("Server-only skills use the same GAS ammo store"), Weapon->GetCurrentAmmoInMag(), 8);
	Weapon->AdjustAmmo_Server(1);
	TestEqual(TEXT("Server ammo adjustment updates the shared attribute"), Weapon->GetCurrentAmmoInMag(), 9);
	TestTrue(TEXT("HUD subscriptions observe authoritative ammo changes"), UIEvents >= 5);
	ASC->GetGameplayAttributeValueChangeDelegate(UAscenduntWeaponSet::GetCurrentAmmoAttribute()).Remove(Delegate);
	State->SetRole(ROLE_Authority);
	ASC->InitAbilityActorInfo(State, State);
	Weapon->SetAmmoInMag_Server(10);
	TestEqual(TEXT("Authoritative reload updates the same attribute"), Weapon->GetCurrentAmmoInMag(), 10);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetMagSizeAttribute(), 4.f);
	Weapon->SetAmmoInMag_Server(10);
	TestEqual(TEXT("New magazine size clamps ammo"), Weapon->GetCurrentAmmoInMag(), 4);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetRateOfFireAttribute(), 10.f);
	ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetShotsPerTriggerAttribute(), 3.f);
	const FGameplayAbilitySpecHandle AIHandle = ASC->GiveAbility(FGameplayAbilitySpec(UGA_AIFire_Auto::StaticClass(), 1, INDEX_NONE, Weapon));
	TestTrue(TEXT("AI can activate its independent authoritative loop"), ASC->TryActivateAbility(AIHandle));
	TestEqual(TEXT("AI trigger consumes cost once across three pellets"), Weapon->GetCurrentAmmoInMag(), 3);
	ASC->CancelAbilityHandle(AIHandle);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
