// Verify menu/play travel without depending on Pawn or PlayerState initialization.

#if WITH_DEV_AUTOMATION_TESTS

#include "ASCENDUNT/System/AscVisionLocalPlayerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace
{
UWorld* CreateVisionTestWorld(const TCHAR* Name)
{
	// GetMapName uses the package name, not the UWorld object's name.
	const FString PackageName = FString::Printf(TEXT("/Temp/%s_%s"), Name,
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UPackage* Package = CreatePackage(*PackageName);
	Package->SetFlags(RF_Transient);
	return UWorld::CreateWorld(EWorldType::Game, false, FName(Name), Package);
}

class FWaitForVisionMaterials : public IAutomationLatentCommand
{
public:
	explicit FWaitForVisionMaterials(TFunction<bool()> InUpdate) : UpdateFunction(MoveTemp(InUpdate)) {}
	virtual bool Update() override { return UpdateFunction(); }

private:
	TFunction<bool()> UpdateFunction;
};

struct FVisionTravelTestState
{
	TStrongObjectPtr<ULocalPlayer> LocalPlayer;
	TStrongObjectPtr<UAscVisionLocalPlayerSubsystem> Vision;
	TArray<UWorld*> Worlds;
	int32 Phase = 0;
	double Deadline = FPlatformTime::Seconds() + 30.0;

	~FVisionTravelTestState()
	{
		for (UWorld* World : Worlds)
		{
			World->DestroyWorld(false);
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAscVisionWorldMaterialsTest,
	"ASCENDUNT.Vision.WorldMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAscVisionWorldMaterialsTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FVisionTravelTestState> State = MakeShared<FVisionTravelTestState>();
	State->LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
	State->Vision.Reset(NewObject<UAscVisionLocalPlayerSubsystem>(State->LocalPlayer.Get()));
	State->Worlds.Add(CreateVisionTestWorld(TEXT("L_MainMenu_VisionTest")));
	State->Worlds.Add(CreateVisionTestWorld(TEXT("VisionPlayTestA")));
	State->Worlds.Add(CreateVisionTestWorld(TEXT("VisionPlayTestB")));
	UAscVisionLocalPlayerSubsystem* Vision = State->Vision.Get();
	UWorld* MenuWorld = State->Worlds[0];
	UWorld* PlayWorld = State->Worlds[1];

	Vision->AttachToWorld(MenuWorld);
	TestFalse(TEXT("Menu never requests X-Ray materials"), Vision->XRayMaterialsLoadHandle.IsValid());
	TestNull(TEXT("Menu has no cached X-Ray material"), Vision->LoadedXRayMaterial.Get());

	Vision->AttachToWorld(PlayWorld);
	TSharedPtr<FStreamableHandle> CanceledRequest = Vision->XRayMaterialsLoadHandle;
	TestTrue(TEXT("Play world preloads without any characters"), CanceledRequest.IsValid());
	Vision->AttachToWorld(PlayWorld);
	TestTrue(TEXT("Repeated attach shares the same request"), Vision->XRayMaterialsLoadHandle == CanceledRequest);

	// Travel away before the first completion is delivered.
	Vision->AttachToWorld(MenuWorld);
	if (CanceledRequest.IsValid())
	{
		TestTrue(TEXT("Leaving play cancels the outstanding request"), CanceledRequest->WasCanceled());
	}
	CanceledRequest.Reset();
	Vision->HandleXRayMaterialsLoaded(PlayWorld);
	TestNull(TEXT("Stale play completion cannot populate the menu cache"), Vision->LoadedXRayMaterial.Get());
	TestFalse(TEXT("Menu has no outstanding request after cancellation"), Vision->XRayMaterialsLoadHandle.IsValid());

	Vision->AttachToWorld(PlayWorld);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForVisionMaterials>([this, State]()
	{
		UAscVisionLocalPlayerSubsystem* CurrentVision = State->Vision.Get();
		if (FPlatformTime::Seconds() > State->Deadline)
		{
			AddError(TEXT("Timed out waiting for shared X-Ray material preload"));
			CurrentVision->DetachFromWorld();
			return true;
		}
		if (!IsValid(CurrentVision->LoadedXRayMaterial) || !IsValid(CurrentVision->LoadedXRayHiddenMaterial))
		{
			return false;
		}

		TestFalse(TEXT("Completed preload releases its handle"), CurrentVision->XRayMaterialsLoadHandle.IsValid());
		if (State->Phase == 0)
		{
			// World cleanup must release the cache even before the next world attaches.
			CurrentVision->HandleWorldCleanup(State->Worlds[1], true, true);
			TestFalse(TEXT("Cleanup unbinds the play world"), CurrentVision->BoundWorld.IsValid());
			TestNull(TEXT("Cleanup releases the primary material"), CurrentVision->LoadedXRayMaterial.Get());
			TestNull(TEXT("Cleanup releases the hidden material"), CurrentVision->LoadedXRayHiddenMaterial.Get());
			CurrentVision->AttachToWorld(State->Worlds[0]);
			TestFalse(TEXT("Returning to menu does not reload"), CurrentVision->XRayMaterialsLoadHandle.IsValid());
			CurrentVision->HandleXRayMaterialsLoaded(State->Worlds[1]);
			TestNull(TEXT("Late old-world completion remains ignored"), CurrentVision->LoadedXRayMaterial.Get());
			CurrentVision->AttachToWorld(State->Worlds[2]);
			TestTrue(TEXT("Next play world starts a new preload"), CurrentVision->XRayMaterialsLoadHandle.IsValid());
			State->Phase = 1;
			State->Deadline = FPlatformTime::Seconds() + 30.0;
			return false;
		}

		TestTrue(TEXT("Second preload belongs to the new world"), CurrentVision->BoundWorld.Get() == State->Worlds[2]);
		CurrentVision->DetachFromWorld();
		TestNull(TEXT("Final detach clears primary material"), CurrentVision->LoadedXRayMaterial.Get());
		TestNull(TEXT("Final detach clears hidden material"), CurrentVision->LoadedXRayHiddenMaterial.Get());
		return true;
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
