// Overlay must not mutate the original primitive's rendering or gameplay state.
#if WITH_DEV_AUTOMATION_TESTS

#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

namespace
{
UWorld* CreateOverlayTestWorld()
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Values);
}

USkeletalMesh* CreateOverlayTestMesh(UObject* Outer, UMaterialInterface* BaseMaterial)
{
	USkeletalMesh* Mesh = NewObject<USkeletalMesh>(Outer);
	for (const TCHAR* SlotName : { TEXT("Body"), TEXT("Eye"), TEXT("Mouth"), TEXT("Eyebrow") })
	{
		FSkeletalMaterial Slot;
		Slot.MaterialInterface = BaseMaterial;
		Slot.MaterialSlotName = FName(SlotName);
		Mesh->GetMaterials().Add(Slot);
	}
	return Mesh;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscVisionBodyOverlayTest,
	"ASCENDUNT.Vision.Overlay.BodyState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscVisionBodyOverlayTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateOverlayTestWorld();
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AHeroCharacter* Hero = World->SpawnActor<AHeroCharacter>();
	if (!TestNotNull(TEXT("Hero spawned"), Hero)) return false;
	USkeletalMeshComponent* Body = Hero->GetMesh();
	UMaterialInterface* BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	UMaterialInterface* XRay = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_XRayMesh_Ally.M_XRayMesh_Ally"));
	UMaterialInterface* Hidden = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_XRayHidden.M_XRayHidden"));
	if (!TestNotNull(TEXT("Existing X-Ray material"), XRay) ||
		!TestNotNull(TEXT("Existing excluded-slot material"), Hidden)) return false;

	Body->SetSkeletalMesh(CreateOverlayTestMesh(Hero, BaseMaterial));
	Hero->SetVisionBlockerEnabled(true);
	const int32 ComponentCount = Hero->GetComponents().Num();
	const bool bVisible = Body->IsVisible();
	const bool bHidden = Body->bHiddenInGame;
	const bool bCastShadow = Body->CastShadow;

	// The request can arrive before the asynchronous material preload completes.
	Hero->SetXRayEnabled(true);
	TestNull(TEXT("No overlay before materials arrive"), Body->GetOverlayMaterial());
	Hero->SetXRayMaterials(XRay, Hidden);
	Hero->SetXRayEnabled(true, 0.25f, 0.75f);
	TestTrue(TEXT("Overlay uses original body"), Hero->GetXRayBodyMesh() == Body);
	TestEqual(TEXT("No duplicate component allocated"), Hero->GetComponents().Num(), ComponentCount);
	TestTrue(TEXT("Global overlay applied"), Body->GetOverlayMaterial() == XRay);
	const auto& Slots = Body->GetComponentMaterialSlotsOverlayMaterial();
	if (TestEqual(TEXT("All material slots covered"), Slots.Num(), 4))
	{
		TestTrue(TEXT("Body receives X-Ray"), Slots[0] == XRay);
		TestTrue(TEXT("Eye excluded"), Slots[1] == Hidden);
		TestTrue(TEXT("Mouth excluded"), Slots[2] == Hidden);
		TestTrue(TEXT("Eyebrow not mistaken for Eye"), Slots[3] == XRay);
	}
	TestEqual(TEXT("Health reaches original primitive"), Body->GetCustomPrimitiveData().Data[0], 0.25f);
	TestEqual(TEXT("Alpha reaches original primitive"), Body->GetCustomPrimitiveData().Data[1], 0.75f);
	TestNull(TEXT("1P mesh never receives X-Ray"), Hero->GetMesh1P()->GetOverlayMaterial());

	Hero->SetXRayEnabled(false);
	TestNull(TEXT("Disable clears global overlay"), Body->GetOverlayMaterial());
	TestTrue(TEXT("Disable clears slot overrides"), Body->GetComponentMaterialSlotsOverlayMaterial().IsEmpty());
	TestTrue(TEXT("CustomDepth blocker remains enabled"), Body->bRenderCustomDepth);
	TestEqual(TEXT("Original visibility preserved"), Body->IsVisible(), bVisible);
	TestEqual(TEXT("Original hidden state preserved"), bool(Body->bHiddenInGame), bHidden);
	TestEqual(TEXT("Original shadow flag preserved"), bool(Body->CastShadow), bCastShadow);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestTrue(TEXT("Original material preserved"), Body->GetMaterial(Index) == BaseMaterial);
	}

	Body->SetHiddenInGame(true);
	Hero->SetXRayEnabled(true);
	TestTrue(TEXT("Overlay cannot unhide a hidden source"), Body->bHiddenInGame);
	Hero->SetXRayEnabled(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscVisionWeaponOverlayTest,
	"ASCENDUNT.Vision.Overlay.WeaponState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscVisionWeaponOverlayTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateOverlayTestWorld();
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AAscenduntWeapon* Weapon = World->SpawnActor<AAscenduntWeapon>();
	if (!TestNotNull(TEXT("Weapon spawned"), Weapon)) return false;
	UMaterialInterface* XRay = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_XRayMesh_Ally.M_XRayMesh_Ally"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("X-Ray material"), XRay) || !TestNotNull(TEXT("Static test mesh"), Cube)) return false;
	UMaterialInterface* BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	Weapon->StaticWeaponMesh->SetStaticMesh(Cube);
	Weapon->StaticWeaponMesh->SetMaterial(0, BaseMaterial);
	Weapon->StaticWeaponMesh->SetRenderCustomDepth(true);
	const int32 ComponentCount = Weapon->GetComponents().Num();
	Weapon->SetXRayMaterial(XRay);
	Weapon->SetXRayMeshEnabled(true, -0.5f, 2.f);
	TestTrue(TEXT("Static weapon uses source overlay"), Weapon->StaticWeaponMesh->GetOverlayMaterial() == XRay);
	TestEqual(TEXT("Health clamped"), Weapon->StaticWeaponMesh->GetCustomPrimitiveData().Data[0], 0.f);
	TestEqual(TEXT("Alpha clamped"), Weapon->StaticWeaponMesh->GetCustomPrimitiveData().Data[1], 1.f);
	TestTrue(TEXT("CustomDepth preserved"), Weapon->StaticWeaponMesh->bRenderCustomDepth);

	// When a skeletal source becomes available it takes precedence over the static mesh.
	Weapon->WeaponMesh->SetSkeletalMesh(CreateOverlayTestMesh(Weapon, BaseMaterial));
	Weapon->SetXRayMeshEnabled(true);
	TestTrue(TEXT("Skeletal weapon uses source overlay"), Weapon->WeaponMesh->GetOverlayMaterial() == XRay);
	TestNull(TEXT("Inactive static source has no stale overlay"), Weapon->StaticWeaponMesh->GetOverlayMaterial());
	TestTrue(TEXT("Inactive static slot overrides cleared"), Weapon->StaticWeaponMesh->GetComponentMaterialSlotsOverlayMaterial().IsEmpty());
	TestEqual(TEXT("No duplicate weapon components allocated"), Weapon->GetComponents().Num(), ComponentCount);
	TestNull(TEXT("1P weapon never receives overlay"), Weapon->GetWeaponMesh1P()->GetOverlayMaterial());

	Weapon->SetXRayMeshEnabled(false);
	TestNull(TEXT("Holster clears skeletal overlay"), Weapon->WeaponMesh->GetOverlayMaterial());
	TestTrue(TEXT("Base static material unchanged"), Weapon->StaticWeaponMesh->GetMaterial(0) == BaseMaterial);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
