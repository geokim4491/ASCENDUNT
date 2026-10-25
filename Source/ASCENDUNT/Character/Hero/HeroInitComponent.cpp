// Fill out your copyright notice in the Description page of Project Settings.

#include "HeroInitComponent.h"

#include "ASCENDUNT/AbilitySystem/AscenduntAbilitySystemComponent.h"
#include "ASCENDUNT/AscenduntGameplayTags.h"
#include "ASCENDUNT/Camera/AscenduntCameraComponent.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode.h"
#include "ASCENDUNT/Character/AscenduntHealthComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCameraComponent.h"
#include "ASCENDUNT/Character/Hero/HeroCharacter.h"
#include "ASCENDUNT/Character/Hero/HeroInputComponent.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponEquipmentComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UHeroInitComponent::BeginPlay()
{
    Super::BeginPlay();

    bDeinitialized = false;

    Evaluate();
}

void UHeroInitComponent::NotifyPlayerStateChanged()
{
    Evaluate();
}

void UHeroInitComponent::NotifyControllerChanged()
{
    Evaluate();
}

void UHeroInitComponent::NotifyInputComponentReady()
{
    Evaluate();
}

bool UHeroInitComponent::IsGameplayReady() const
{
    return CurrentState == AscenduntGameplayTags::InitState_GameplayReady;
}

void UHeroInitComponent::Evaluate()
{
    if (bDeinitialized)
    {
        return;
    }

    RefreshDependencies();

    // 초기화 작업은 BeginPlay 이후에만 진행.
    if (!HasBegunPlay() || IsGameplayReady() || !OwnerPawn.IsValid() || !CachedPS.IsValid())
    {
        return;
    }

    // 조건 준비되어있으면, 쭉 진행함.
    if (!CurrentState.IsValid())
    {
        if (!CanEnterSpawned() || !ValidateStaticConfiguration())
        {
            return;
        }

        CurrentState = AscenduntGameplayTags::InitState_Spawned;
    }

    if (CurrentState == AscenduntGameplayTags::InitState_Spawned)
    {
        if (!TryEnterGASInitialized())
        {
            return;
        }
    }

    if (CurrentState == AscenduntGameplayTags::InitState_GASInitialized)
    {
        PrepareGameplayDependencies();

        if (!CanEnterGameplayReady())
        {
            return;
        }

        TryEnterGameplayReady();
    }
}

void UHeroInitComponent::RefreshDependencies()
{
    if (!OwnerPawn.IsValid())
    {
        OwnerPawn = Cast<AHeroCharacter>(GetOwner());
    }

    if (!OwnerPawn.IsValid())
    {
        return;
    }

    AAscenduntPlayerState* CurrentPS = OwnerPawn->GetPlayerState<AAscenduntPlayerState>();
    if (CachedPS.Get() != CurrentPS)
    {
        UnbindFromPlayerState();
        CachedPS = CurrentPS;
        CachedASC = nullptr;
    }

    if (CachedPS.IsValid())
    {
        BindToPlayerState(CachedPS.Get());
    }
    else
    {
        CachedASC = nullptr;
    }

    BindToWeaponEquipment();
}

bool UHeroInitComponent::CanEnterSpawned() const
{
    const UHeroPawnData* PawnData = CachedPS->GetPawnData();
    if (!PawnData)
    {
        return false;
    }

    if (!PawnData->PawnClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[HeroInit] PawnData has no PawnClass. Pawn=%s PawnData=%s"),
            *GetNameSafe(OwnerPawn.Get()),
            *GetNameSafe(PawnData));
        return false;
    }

    // 캐릭터 교체할 때, Pawn이랑 PawnData의 Pawn이 다른 순서로 복제될 수 있어서
    // 일치하지 않으면 나중에 PawnData 변경 이벤트에서 다시 확인함.
    return OwnerPawn->IsA(PawnData->PawnClass);
}

bool UHeroInitComponent::ValidateStaticConfiguration()
{
    check(OwnerPawn.IsValid());
    check(CachedPS.IsValid());

    bool bValid = true;
    const UHeroPawnData* PawnData = CachedPS->GetPawnData();

    auto RequireObject = [this, &bValid](const auto* Object, const TCHAR* Name)
        {
            if (!Object)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[HeroInit] Missing static dependency '%s'. Pawn=%s"),
                    Name,
                    *GetNameSafe(OwnerPawn.Get()));
                bValid = false;
            }
        };

    RequireObject(PawnData, TEXT("PawnData"));
    RequireObject(OwnerPawn->GetHeroInputComponent(), TEXT("HeroInputComponent"));
    RequireObject(OwnerPawn->GetHeroCameraComponent(), TEXT("HeroCameraComponent"));
    RequireObject(UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get()), TEXT("AscenduntCameraComponent"));
    RequireObject(OwnerPawn->GetAscenduntHealthComponent(), TEXT("AscenduntHealthComponent"));
    RequireObject(OwnerPawn->GetWeaponEquipmentComponent(), TEXT("WeaponEquipmentComponent"));
    RequireObject(OwnerPawn->GetMesh(), TEXT("MainSkeletalMeshComponent"));

    if (PawnData)
    {
        RequireObject(PawnData->InputConfig.Get(), TEXT("PawnData.InputConfig"));
        RequireObject(PawnData->DefaultCameraMode.Get(), TEXT("PawnData.DefaultCameraMode"));
    }

    if (USkeletalMeshComponent* MeshComp = OwnerPawn->GetMesh())
    {
        RequireObject(MeshComp->GetSkeletalMeshAsset(), TEXT("Mesh.SkeletalMeshAsset"));
        RequireObject(MeshComp->GetAnimClass(), TEXT("Mesh.AnimClass"));
    }

    return bValid;
}

bool UHeroInitComponent::TryEnterGASInitialized()
{
    UAscenduntAbilitySystemComponent* ASC = CachedPS->GetAscenduntAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[HeroInit] PlayerState exists but ASC is null. Pawn=%s PS=%s"),
            *GetNameSafe(OwnerPawn.Get()),
            *GetNameSafe(CachedPS.Get()));
        return false;
    }

    CachedASC = ASC;

    const bool bActorInfoMatches =
        ASC->AbilityActorInfo.IsValid() &&
        ASC->GetOwnerActor() == CachedPS.Get() &&
        ASC->GetAvatarActor() == OwnerPawn.Get();

    if (!bActorInfoMatches)
    {
        ASC->InitAbilityActorInfo(CachedPS.Get(), OwnerPawn.Get());
    }

    if (!ASC->AbilityActorInfo.IsValid() ||
        ASC->GetOwnerActor() != CachedPS.Get() ||
        ASC->GetAvatarActor() != OwnerPawn.Get())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[HeroInit] Failed to initialize ASC ActorInfo. Pawn=%s PS=%s Owner=%s Avatar=%s"),
            *GetNameSafe(OwnerPawn.Get()),
            *GetNameSafe(CachedPS.Get()),
            *GetNameSafe(ASC->GetOwnerActor()),
            *GetNameSafe(ASC->GetAvatarActor()));
        return false;
    }

    ASC->AbilityActorInfo->SkeletalMeshComponent = OwnerPawn->GetMesh();

    // Configure on both server and client so predicted activation uses the same rules.
    ASC->SetTagRelationshipMapping(CachedPS->GetPawnData()->TagRelationshipMapping);

    // AbilitySet은 서버에서 부여함.
    if (OwnerPawn->HasAuthority())
    {
        if (!CachedPS->ApplyPawnDataAbilitySets())
        {
            return false;
        }

        CachedPS->InitializeCombatForCurrentPawnData();
    }
    // 클라는 PS에 PawnData가 적용되면 다음으로 진행
    else if (!CachedPS->IsPawnDataApplied())
    {
        // OnRep_AppliedPawnData에서 다시 평가.
        return false;
    }

    if (!bOnSpawnAbilitiesActivatedForAvatar)
    {
        ASC->TryActivateAbilitiesOnSpawn();
        bOnSpawnAbilitiesActivatedForAvatar = true;
    }

    CurrentState = AscenduntGameplayTags::InitState_GASInitialized;

    UE_LOG(LogTemp, Log, TEXT("[HeroInit] %s -> GASInitialized"), *GetNameSafe(OwnerPawn.Get()));

    return true;
}

void UHeroInitComponent::PrepareGameplayDependencies()
{
    if (!bAnimationInitialized)
    {
        bAnimationInitialized = OwnerPawn->TryInitializeAnimationForGameplay();
    }

    if (!bHealthCompInitialized)
    {
        if (UAscenduntHealthComponent* HealthComp = OwnerPawn->GetAscenduntHealthComponent())
        {
            bHealthCompInitialized = HealthComp->InitializeWithAbilitySystem(CachedASC.Get());
            if (bHealthCompInitialized)
            {
                OwnerPawn->BindHealthEvents();
            }
        }
    }

    if (OwnerPawn->HasAuthority())
    {
        if (UWeaponEquipmentComponent* EquipmentComp = OwnerPawn->GetWeaponEquipmentComponent())
        {
            EquipmentComp->ServerInitializeDefaultLoadout();
        }
    }
}

bool UHeroInitComponent::CanEnterGameplayReady() const
{
    if (!bAnimationInitialized || !bHealthCompInitialized)
    {
        return false;
    }

    const UWeaponEquipmentComponent* EquipmentComp = OwnerPawn->GetWeaponEquipmentComponent();
    if (!EquipmentComp || !EquipmentComp->IsWeaponLoadoutReady())
    {
        return false;
    }

    // 로컬이면 입력, 카메라까지 초기화.
    // 여기선 준비만 하고 초기화는 GameplayReady에서 한번에 함.
    // 하나씩 준비되는 것처럼 보이게 안하고 한번에 준비된 상태를 보이게 하려고 이렇게 함.
    if (!RequiresLocalPlayerInitialization())
    {
        return true;
    }

    if (!OwnerPawn->IsLocallyControlled() || !OwnerPawn->InputComponent)
    {
        return false;
    }

    const UHeroInputComponent* InputComp = OwnerPawn->GetHeroInputComponent();
    if (!InputComp || !InputComp->CanInitializePlayerInput(OwnerPawn->InputComponent))
    {
        return false;
    }

    return CanInitializeLocalCamera();
}

bool UHeroInitComponent::CanInitializeLocalCamera() const
{
    if (!OwnerPawn.IsValid() || !CachedPS.IsValid())
    {
        return false;
    }

    const UHeroPawnData* PawnData = CachedPS->GetPawnData();
    return
        OwnerPawn->IsLocallyControlled() &&
        PawnData && PawnData->DefaultCameraMode &&
        OwnerPawn->GetHeroCameraComponent() &&
        UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get());
}

bool UHeroInitComponent::InitializeLocalCamera()
{
    if (!CanInitializeLocalCamera())
    {
        return false;
    }

    UAscenduntCameraComponent* CameraComponent =
        UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get());
    UHeroCameraComponent* HeroCamera = OwnerPawn->GetHeroCameraComponent();

    CameraComponent->DetermineCameraModeDelegate.BindUObject(
        HeroCamera,
        &UHeroCameraComponent::DetermineCameraMode);

    return CameraComponent->DetermineCameraModeDelegate.IsBound();
}

bool UHeroInitComponent::TryEnterGameplayReady()
{
    if (!OwnerPawn.IsValid())
    {
        return false;
    }

    const bool bNeedsLocalInitialization = RequiresLocalPlayerInitialization();

    OwnerPawn->InitializeGameplayBindings();

    if (bNeedsLocalInitialization)
    {
        if (!InitializeLocalCamera())
        {
            OwnerPawn->UninitializeGameplayBindings();
            return false;
        }

        UHeroInputComponent* InputComp = OwnerPawn->GetHeroInputComponent();
        if (!InputComp || !InputComp->InitializePlayerInput(OwnerPawn->InputComponent))
        {
            if (UAscenduntCameraComponent* CameraComponent =
                UAscenduntCameraComponent::FindCameraComponent(OwnerPawn.Get()))
            {
                CameraComponent->DetermineCameraModeDelegate.Unbind();
            }
            OwnerPawn->UninitializeGameplayBindings();
            return false;
        }
    }

    CurrentState = AscenduntGameplayTags::InitState_GameplayReady;

    UE_LOG(LogTemp, Log, TEXT("[HeroInit] %s -> GameplayReady"), *GetNameSafe(OwnerPawn.Get()));

    OnGameplayReady.Broadcast(OwnerPawn.Get());
    return true;
}

bool UHeroInitComponent::RequiresLocalPlayerInitialization() const
{
    if (!OwnerPawn.IsValid() || OwnerPawn->GetNetMode() != NM_Client)
    {
        return false;
    }

    if (OwnerPawn->GetLocalRole() == ROLE_AutonomousProxy || OwnerPawn->IsLocallyControlled())
    {
        return true;
    }

    // ROLE이나 Controller 연결이 늦으면, 소유 Pawn도 아직 로컬로 판별되지 않을 수 있음.
    // 로컬 PC의 PS와 비교해서 로컬인지 판정.
    if (CachedPS.IsValid())
    {
        if (const UWorld* World = OwnerPawn->GetWorld())
        {
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                const APlayerController* PC = It->Get();
                if (PC && PC->IsLocalController() &&
                    PC->GetPlayerState<AAscenduntPlayerState>() == CachedPS.Get())
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void UHeroInitComponent::BindToPlayerState(AAscenduntPlayerState* PS)
{
    if (!PS) return;

    if (!PawnDataAvailHandle.IsValid())
    {
        PawnDataAvailHandle = PS->OnPawnDataAvailable.AddUObject(
            this,
            &UHeroInitComponent::Evaluate);
    }

    if (!PawnDataAppliedHandle.IsValid())
    {
        PawnDataAppliedHandle = PS->OnPawnDataApplied.AddUObject(
            this,
            &UHeroInitComponent::Evaluate);
    }
}

void UHeroInitComponent::UnbindFromPlayerState()
{
    if (AAscenduntPlayerState* PS = CachedPS.Get())
    {
        if (PawnDataAvailHandle.IsValid())
        {
            PS->OnPawnDataAvailable.Remove(PawnDataAvailHandle);
        }
        if (PawnDataAppliedHandle.IsValid())
        {
            PS->OnPawnDataApplied.Remove(PawnDataAppliedHandle);
        }
    }

    PawnDataAvailHandle.Reset();
    PawnDataAppliedHandle.Reset();
}

void UHeroInitComponent::BindToWeaponEquipment()
{
    if (OwnerPawn->GetNetMode() != NM_Client)
    {
        return;
    }

    UWeaponEquipmentComponent* EquipmentComp = OwnerPawn->GetWeaponEquipmentComponent();
    if (!EquipmentComp)
    {
        return;
    }

    if (!CurrentWeaponChangedHandle.IsValid())
    {
        CurrentWeaponChangedHandle = EquipmentComp->OnCurrentWeaponChanged.AddUObject(
            this,
            &UHeroInitComponent::HandleCurrentWeaponChanged);
    }

    if (!WeaponInventoryChangedHandle.IsValid())
    {
        WeaponInventoryChangedHandle = EquipmentComp->OnWeaponInventoryChanged.AddUObject(
            this,
            &UHeroInitComponent::Evaluate);
    }
}

void UHeroInitComponent::UnbindFromWeaponEquipment()
{
    AHeroCharacter* Pawn = OwnerPawn.Get();
    if (!Pawn)
    {
        Pawn = Cast<AHeroCharacter>(GetOwner());
    }

    if (Pawn)
    {
        if (UWeaponEquipmentComponent* EquipmentComp = Pawn->GetWeaponEquipmentComponent())
        {
            if (CurrentWeaponChangedHandle.IsValid())
            {
                EquipmentComp->OnCurrentWeaponChanged.Remove(CurrentWeaponChangedHandle);
            }
            if (WeaponInventoryChangedHandle.IsValid())
            {
                EquipmentComp->OnWeaponInventoryChanged.Remove(WeaponInventoryChangedHandle);
            }
        }
    }

    CurrentWeaponChangedHandle.Reset();
    WeaponInventoryChangedHandle.Reset();
}

void UHeroInitComponent::HandleCurrentWeaponChanged(AAscenduntWeapon* NewWeapon, AAscenduntWeapon* PrevWeapon)
{
    Evaluate();
}

void UHeroInitComponent::DeinitializeForPawnReplacement(bool bPreserveHeroState)
{
    DeinitializePawn(bPreserveHeroState);
}

void UHeroInitComponent::Shutdown()
{
    DeinitializePawn(/*bPreserveHeroState*/ false);
}

void UHeroInitComponent::DeinitializePawn(bool bPreserveHeroState)
{
    if (bDeinitialized)
    {
        return;
    }

    bDeinitialized = true;

    AHeroCharacter* Pawn = OwnerPawn.Get();
    if (!Pawn)
    {
        Pawn = Cast<AHeroCharacter>(GetOwner());
        OwnerPawn = Pawn;
    }

    AAscenduntPlayerState* PS = CachedPS.Get();
    if (!PS && Pawn)
    {
        PS = Pawn->GetPlayerState<AAscenduntPlayerState>();
        CachedPS = PS;
    }

    UAscenduntAbilitySystemComponent* ASC = CachedASC.Get();
    if (!ASC && PS)
    {
        ASC = PS->GetAscenduntAbilitySystemComponent();
        CachedASC = ASC;
    }

    if (ASC)
    {
        ASC->CancelAllAbilities();
        ASC->ClearAbilityInput();
    }

    if (Pawn)
    {
        Pawn->UninitializeGameplayBindings();

        if (UHeroInputComponent* InputComp = Pawn->GetHeroInputComponent())
        {
            InputComp->UninitializePlayerInput();
        }

        if (UAscenduntCameraComponent* CameraComponent =
            UAscenduntCameraComponent::FindCameraComponent(Pawn))
        {
            CameraComponent->DetermineCameraModeDelegate.Unbind();
        }

        Pawn->UnbindHealthEvents();

        if (UAscenduntHealthComponent* HealthComp = Pawn->GetAscenduntHealthComponent())
        {
            HealthComp->UninitializeFromAbilitySystem();
        }
    }

    if (Pawn && Pawn->HasAuthority())
    {
        if (UWeaponEquipmentComponent* EquipmentComp = Pawn->GetWeaponEquipmentComponent())
        {
            EquipmentComp->ServerClearWeaponsFromASC(/*bDestroyWeapons*/ true);
        }

        if (PS && !bPreserveHeroState)
        {
            PS->RemovePawnDataAbilitySets();
        }
    }

    if (ASC && Pawn && ASC->GetAvatarActor() == Pawn)
    {
        ASC->ClearActorInfo();
    }

    UnbindFromPlayerState();
    UnbindFromWeaponEquipment();
    ResetRuntimeState();
}

void UHeroInitComponent::ResetRuntimeState()
{
    CachedPS = nullptr;
    CachedASC = nullptr;
    OwnerPawn = nullptr;

    bAnimationInitialized = false;
    bHealthCompInitialized = false;
    bOnSpawnAbilitiesActivatedForAvatar = false;

    CurrentState = FGameplayTag::EmptyTag;
}
