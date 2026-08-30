// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntCameraComponent.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "AscenduntCameraMode.h"

#if !UE_BUILD_SHIPPING
// 1인칭 렌더 파라미터 실시간 조정용. 음수면 컴포넌트 프로퍼티 값을 그대로 쓴다.
//   asc.FP.Camera.FOV 105     씬 FOV(보통 90)보다 크면 1인칭 대상이 작아진다
//   asc.FP.Camera.Scale 0.6   1인칭 대상을 카메라 쪽으로 축소한다. 팔이 화면을 덜 먹는다
//   asc.FP.Camera.Dump 1      현재 값을 로그로 출력
static float GAscFPCameraFOV = -1.f;
static float GAscFPCameraScale = -1.f;
static int32 GAscFPCameraDump = 0;

static FAutoConsoleVariableRef CVarAscFPCameraFOV(TEXT("asc.FP.Camera.FOV"), GAscFPCameraFOV, TEXT("1인칭 전용 FOV. 음수면 BP 값 사용."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPCameraScale(TEXT("asc.FP.Camera.Scale"), GAscFPCameraScale, TEXT("1인칭 전용 스케일. 음수면 BP 값 사용."), ECVF_Cheat);
static FAutoConsoleVariableRef CVarAscFPCameraDump(TEXT("asc.FP.Camera.Dump"), GAscFPCameraDump, TEXT("1이면 현재 1인칭 렌더 값을 로그로 출력."), ECVF_Cheat);
#endif

UAscenduntCameraComponent::UAscenduntCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraModeStack = nullptr;
	FieldOfViewOffset = 0.0f;

	// ===== First Person Rendering (UE 5.5+) =====
	// FirstPersonPrimitiveType == FirstPerson 으로 태그된 프리미티브에만 적용된다.
	// 태그되지 않은 프리미티브에는 아무 영향이 없고, 유일한 태그 대상인 Mesh1P 는
	// 3인칭일 때 어차피 숨겨지므로 이 플래그는 런타임 토글 없이 상시 켜둔다.
	//
	// 기본값은 중립(씬 FOV와 동일, 스케일 1)으로 둔다. 풀바디 1인칭이라 몸 전체가 대상이고,
	// 팔만 있는 메시처럼 확대/축소하면 다리와 몸통까지 왜곡되기 때문이다.
	// 그래도 태그 자체가 벽 뚫림을 막아주므로 중립값에서도 의미가 있다.
	//
	// 튜닝 시: FirstPersonFieldOfView 를 씬 FOV(90)보다 낮추면 보정 계수가
	// tan(90/2) / tan(FOV/2) 로 커져 1인칭 대상이 크게 보인다. 무기를 강조하고 싶을 때 쓴다.
	bEnableFirstPersonFieldOfView = true;
	bEnableFirstPersonScale = true;
	FirstPersonFieldOfView = 90.0f;
	FirstPersonScale = 1.0f;
}

void UAscenduntCameraComponent::SetADSFieldOfView(bool bEnabled, float InTargetFOV)
{
	bADSFieldOfViewEnabled = bEnabled;
	if (bEnabled)
	{
		ADSFieldOfView = FMath::Clamp(InTargetFOV, 5.0f, 170.0f);
	}
}

void UAscenduntCameraComponent::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("AscenduntCameraComponent: %s"), *GetNameSafe(GetTargetActor())));

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Location: %s"), *GetComponentLocation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Rotation: %s"), *GetComponentRotation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   FOV: %f"), FieldOfView));

	check(CameraModeStack);
	CameraModeStack->DrawDebug(Canvas);
}

void UAscenduntCameraComponent::GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const
{
	check(CameraModeStack);
	CameraModeStack->GetBlendInfo(/*out*/ OutWeightOfTopLayer, /*out*/ OutTagOfTopLayer);
}

void UAscenduntCameraComponent::OnRegister()
{
	Super::OnRegister();

	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<UAscenduntCameraModeStack>(this);
		check(CameraModeStack);
	}
}

void UAscenduntCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	UpdateCameraModes();

	FAscenduntCameraModeView CameraModeView;
	CameraModeStack->EvaluateStack(DeltaTime, CameraModeView);

	// ADS is intentionally layered after the camera-mode stack. Skills/death camera modes still
	// determine the base view, while the local weapon ability only blends the final scene FOV.
	ADSBlendAlpha = FMath::FInterpTo(
		ADSBlendAlpha,
		bADSFieldOfViewEnabled ? 1.0f : 0.0f,
		DeltaTime,
		ADSBlendSpeed);
	CameraModeView.FieldOfView = FMath::Lerp(CameraModeView.FieldOfView, ADSFieldOfView, ADSBlendAlpha);

	// Keep player controller in sync with the latest view.
	if (APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		if (APlayerController* PC = TargetPawn->GetController<APlayerController>())
		{
			// A listen-server spectator may view another player's Pawn. Never overwrite
			// the remote player's server-authoritative ControlRotation from our local camera.
			if (PC->IsLocalController())
			{
				PC->SetControlRotation(CameraModeView.ControlRotation);
			}
		}
	}

	// Apply any offset that was added to the field of view.
	CameraModeView.FieldOfView += FieldOfViewOffset;
	FieldOfViewOffset = 0.0f;

	// Keep camera component in sync with the latest view.
	SetWorldLocationAndRotation(CameraModeView.Location, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	// Fill in desired view.
	DesiredView.Location = CameraModeView.Location;
	DesiredView.Rotation = CameraModeView.Rotation;
	DesiredView.FOV = CameraModeView.FieldOfView;
	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;

	// UCameraComponent::GetCameraView 를 호출하지 않고 DesiredView 를 직접 채우는 구조라,
	// 엔진 원본(CameraComponent.cpp)이 채워주는 1인칭 렌더링 파라미터가 여기서 누락된다.
	// 이 세 줄이 없으면 FirstPersonFieldOfView / FirstPersonScale 을 아무리 설정해도
	// bUseFirstPersonParameters 가 false 로 남아 렌더러에 전달되지 않는다. (경고도 에러도 없음)
	float UseFirstPersonFOV = FirstPersonFieldOfView;
	float UseFirstPersonScale = FirstPersonScale;

#if !UE_BUILD_SHIPPING
	// 콘솔 오버라이드. 음수면 BP 값을 그대로 쓴다.
	if (GAscFPCameraFOV > 0.f)
	{
		UseFirstPersonFOV = GAscFPCameraFOV;
	}
	if (GAscFPCameraScale > 0.f)
	{
		UseFirstPersonScale = GAscFPCameraScale;
	}

	if (GAscFPCameraDump != 0)
	{
		GAscFPCameraDump = 0;
		UE_LOG(LogTemp, Warning,
			TEXT("[FP-CAM] CameraComponent 에 적을 값  First Person Field Of View = %.1f   First Person Scale = %.2f   (씬 FOV = %.1f)"),
			UseFirstPersonFOV, UseFirstPersonScale, DesiredView.FOV);
	}
#endif

	DesiredView.FirstPersonFOV = bEnableFirstPersonFieldOfView ? UseFirstPersonFOV : DesiredView.FOV;
	DesiredView.FirstPersonScale = bEnableFirstPersonScale ? UseFirstPersonScale : 1.0f;
	DesiredView.bUseFirstPersonParameters = bEnableFirstPersonFieldOfView || bEnableFirstPersonScale;

	// See if the CameraActor wants to override the PostProcess settings used.
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}

	// Downed grayscale is applied after the component's normal post-process copy so it
	// overrides only global saturation without replacing unrelated camera PP settings.
	if (bDownedGrayscaleEnabled)
	{
		DesiredView.PostProcessBlendWeight = 1.0f;
		DesiredView.PostProcessSettings.bOverride_ColorSaturation = true;
		DesiredView.PostProcessSettings.ColorSaturation = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	if (IsXRHeadTrackedCamera())
	{
		// In XR much of the camera behavior above is irrellevant, but the post process settings are not.
		Super::GetCameraView(DeltaTime, DesiredView);
	}
}

void UAscenduntCameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	if (CameraModeStack->IsStackActivate())
	{
		if (DetermineCameraModeDelegate.IsBound())
		{
			if (const TSubclassOf<UAscenduntCameraMode> CameraMode = DetermineCameraModeDelegate.Execute())
			{
				CameraModeStack->PushCameraMode(CameraMode);
			}
		}
	}
}
