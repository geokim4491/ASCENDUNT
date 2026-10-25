#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "ASCENDUNT/Character/AI/BaseAICharacter.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntWeaponSet.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntHealthSet.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"
#include "ASCENDUNT/Tests/AscWeaponPredictionNetTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscAIWeaponInitializationTest, "ASCENDUNT.WeaponPrediction.AIWeaponInitialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscAIWeaponInitializationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const auto Cleanup = [World]() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    ABaseAICharacter* AI = World->SpawnActor<ABaseAICharacter>();
    UAscenduntAbilitySystemComponent* ASC = AI->GetAscenduntAbilitySystemComponent();
    if (!ASC->HasBeenInitialized()) ASC->InitializeComponent();
    ASC->InitAbilityActorInfo(AI, AI);
    const UAscenduntWeaponSet* PersistentSet = ASC->GetSet<UAscenduntWeaponSet>();
    if (!TestNotNull(TEXT("AI has WeaponSet before any data grant or weapon equip"), PersistentSet))
    {
        Cleanup();
        return false;
    }
    TestTrue(TEXT("AI WeaponSet is a native default subobject"), PersistentSet->HasAnyFlags(RF_DefaultSubObject));
    UAscenduntAbilitySet* AbilitySet = NewObject<UAscenduntAbilitySet>();
    FArrayProperty* GrantsProperty = FindFProperty<FArrayProperty>(UAscenduntAbilitySet::StaticClass(), TEXT("GrantedAttributes"));
    auto& Grants = *GrantsProperty->ContainerPtrToValuePtr<TArray<FAscenduntAbilitySet_AttributeSet>>(AbilitySet);
    FAscenduntAbilitySet_AttributeSet WeaponGrant;
    WeaponGrant.AttributeSet = UAscenduntWeaponSet::StaticClass();
    Grants.Add(WeaponGrant);
    FAscenduntAbilitySet_AttributeSet HealthGrant;
    HealthGrant.AttributeSet = UAscenduntHealthSet::StaticClass();
    Grants.Add(HealthGrant);
    const auto CountWeaponSets = [ASC]()
    {
        int32 Count = 0;
        for (const UAttributeSet* Set : ASC->GetSpawnedAttributes()) if (Set && Set->IsA<UAscenduntWeaponSet>()) ++Count;
        return Count;
    };
    FAscenduntAbilitySet_GrantedHandles Handles;
    AbilitySet->GiveToAbilitySystem(ASC, &Handles, AI);
    TestEqual(TEXT("A data grant reuses the native Set instead of duplicating it"), CountWeaponSets(), 1);
    TestNotNull(TEXT("Non-native health Set is still granted"), ASC->GetSet<UAscenduntHealthSet>());
    ASC->SetNumericAttributeBase(UAscenduntWeaponSet::GetMagSizeAttribute(), 12.f);
    UWeaponEquipmentComponent* Equipment = AI->GetWeaponEquipmentComponent();
    FAscenduntWeaponSlot Slot;
    Slot.WeaponClass = AAscWeaponPredictionTestWeapon::StaticClass();
    Equipment->DefaultWeaponSlots.Add(Slot);
    Equipment->ServerInitializeDefaultLoadout();
    AAscenduntWeapon* Weapon = Equipment->GetCurrentWeapon();
    if (TestNotNull(TEXT("Native AI equipment path equips the weapon"), Weapon))
    {
        TestEqual(TEXT("Equipping initializes CurrentAmmo on the AI ASC"), Weapon->GetCurrentAmmoInMag(), 12);
        Weapon->ConsumeAmmo_Server(1);
        TestEqual(TEXT("AI ammo consumption uses its Attribute"), Weapon->GetCurrentAmmoInMag(), 11);
        Equipment->ServerReleaseWeaponsForPool();
        Handles.TakeFromAbilitySystem(ASC);
        TestTrue(TEXT("Grant revocation preserves the native WeaponSet"), ASC->GetSet<UAscenduntWeaponSet>() == PersistentSet);
        TestNull(TEXT("Grant revocation still removes its dynamic health Set"), ASC->GetSet<UAscenduntHealthSet>());
        AbilitySet->GiveToAbilitySystem(ASC, &Handles, AI);
        Equipment->ServerInitializeDefaultLoadout();
        TestEqual(TEXT("Pooled re-equip refills ammo without a missing Set"), Weapon->GetCurrentAmmoInMag(), 12);
        TestEqual(TEXT("Pooled regrant keeps one WeaponSet"), CountWeaponSets(), 1);
        Equipment->ServerClearWeaponsFromASC(true);
    }
    Handles.TakeFromAbilitySystem(ASC);
    Cleanup();
    return true;
}

#endif
