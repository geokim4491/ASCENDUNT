// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GenericTeamAgentInterface.h"
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"

class UAIPerceptionStimuliSourceComponent;
class UAscVisionMarkerComponent;
class UAscenduntHealthComponent;
class UAscTargetStatusDisplayComponent;
class UAscCharacterFeedbackData;
class USkeletalMeshComponent;
class UMeshComponent;
class UMaterialInterface;

UCLASS()
class ASCENDUNT_API ABaseCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ABaseCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

#if !UE_BUILD_SHIPPING
	/**
	 * 3인칭 몸 방향 정렬 진단용 화살표를 그린다. 콘솔 변수 asc.DebugBodyAim 으로 켠다.
	 *
	 * 원격 폰에도 그려지므로 반드시 "보는 쪽" 클라이언트 화면에서 읽어야 한다.
	 * 어긋남의 원인이 액터 회전인지 RootYawOffset 인지 가르는 용도이며, 진단이 끝나면 통째로 지워도 된다.
	 */
	void DrawBodyAimDebug() const;
#endif

	// ===== Team =====
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void SetGenericTeamId(const FGenericTeamId& NewID) override;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Team")
	uint8 GetTeamId() const { return TeamId; }

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Team")
	void SetTeamId(uint8 NewTeamId) { SetGenericTeamId(FGenericTeamId(NewTeamId)); }

	// ===== Vision =====
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Vision")
	UAscVisionMarkerComponent* GetVisionMarkerComponent() const { return VisionMarkerComponent; }

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Vision")
	USkeletalMeshComponent* GetVisionMesh() const { return GetMesh(); }

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Health")
	virtual UAscenduntHealthComponent* GetAscenduntHealthComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|TargetStatus")
	UAscTargetStatusDisplayComponent* GetTargetStatusDisplayComponent() const { return TargetStatusDisplayComponent; }

	/** 이 캐릭터가 피격될 때 사용할 대상 고유 연출 데이터. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Feedback")
	const UAscCharacterFeedbackData* GetCharacterFeedbackData() const { return CharacterFeedbackData; }

	// 로컬 Vision 표현 적용. 실제 Mesh 구성은 각 캐릭터가 Gather 함수로 제공한다.
	void SetEnemyOverlayEnabled(bool bEnabled);
	void SetVisionBlockerEnabled(bool bEnabled);
	void SetXRayEnabled(bool bEnabled, float HealthNorm = 1.f, float XRayAlpha = 1.f);

	// AI는 둘 다 false, Hero만 true. Vision Subsystem이 불필요한 렌더 상태 변경을 건너뛴다.
	virtual bool SupportsEnemyOverlay() const { return false; }
	virtual bool SupportsXRay() const { return false; }

	// Xray 및 Outline 적용할 원본 메시 Get. 로컬 Blocker CustomDepth에도 사용한다.
	virtual void GatherVisionMeshes(TArray<UMeshComponent*>& OutMeshes) const;

	// Mesh-based X-Ray 렌더링에 사용할 전용 메시 Get.
	virtual void GatherXRayMeshes(TArray<UMeshComponent*>& OutMeshes) const;

	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Vision|XRay")
	USkeletalMeshComponent* GetXRayBodyMesh() const { return XRayBodyMesh; }


	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

protected:
	/**
	 * 시뮬레이티드 프록시용 가속도 복제.
	 *
	 * UCharacterMovementComponent::Acceleration 은 엔진이 복제하지 않아, 다른 플레이어 화면에서는
	 * 속도로부터 만든 근사치가 쓰인다. 라이라 로코모션의 Start / Stop / Pivot 판정이 가속도 기반이라
	 * 그대로 두면 다른 플레이어의 출발·정지·방향전환 모션이 어긋난다.
	 *
	 * 소유 클라이언트는 자기 입력으로 실제 값을 이미 알고 있으므로 COND_SimulatedOnly 로만 보낸다.
	 */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FVector_NetQuantize10 ReplicatedAcceleration;

	UFUNCTION()
	void OnRep_ReplicatedAcceleration();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Ascendunt|Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscenduntHealthComponent> AscenduntHealthComponent;

	// Local-only target presentation shared by player characters and AI.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ascendunt|TargetStatus", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscTargetStatusDisplayComponent> TargetStatusDisplayComponent;

	/**
	 * 캐릭터 종류별 피격 연출 데이터.
	 * 클래스 기본값으로 설정하므로 Hero/AI 모두 클라이언트에서 바로 읽을 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|Feedback")
	TObjectPtr<UAscCharacterFeedbackData> CharacterFeedbackData;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision")
	TObjectPtr<UMaterialInterface> EnemyOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision|XRay")
	TObjectPtr<UMaterialInterface> XRayMeshMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|Vision|XRay")
	TObjectPtr<UMaterialInterface> XRayHiddenMaterial;

	// X-Ray/Enemy Overlay를 사용하는 파생 클래스의 생성자에서만 호출한다.
	// BaseCharacter 생성자에서 만들지 않으므로 AI에는 XRayBodyMesh 자체가 존재하지 않는다.
	void InitializeVisionPresentationSupport();
	void SyncXRayBodyMesh();
	void ApplyXRayMaterialToMesh(UMeshComponent* MeshComponent) const;
	bool HasToken(const FString& UpperName, const FString& Token) const;
	bool ShouldHideXRayMaterialSlot(const FName SlotName, const UMaterialInterface* SourceMaterial) const;
private:
	// Replicated fallback (needed for AI pawns because AIController is server-only)
	UPROPERTY(VisibleAnywhere, Replicated, Category = "Ascendunt|Team")
	uint8 TeamId = 1;

	UPROPERTY()
	UAIPerceptionStimuliSourceComponent* StimuliSourceComp;

	UPROPERTY(VisibleAnywhere, Category = "Ascendunt|Vision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAscVisionMarkerComponent> VisionMarkerComponent;

	UPROPERTY(VisibleAnywhere, Category = "Ascendunt|Vision|XRay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> XRayBodyMesh;

protected:
	// ===== Anti - ESP Test용 코드 =====
	UPROPERTY(ReplicatedUsing = OnRep_DebugReplicationPulse)
	int32 DebugReplicationPulse = 0;

	UFUNCTION()
	void OnRep_DebugReplicationPulse();

	FTimerHandle DebugPulseTimerHandle;

	UFUNCTION()
	void DebugIncreasePulse();
};
