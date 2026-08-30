// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayLocalSound.generated.h"

class USoundBase;

/**
 * 로컬 컨트롤 중인 Pawn에서만 사운드를 재생하는 AnimNotify
 *
 * 사용 예:
 * - 1인칭 리로드 기계음
 * - 내 캐릭터 전용 장전/무기 조작음
 * - 내 화면에서만 들려야 하는 포일리 사운드
 */
UCLASS(meta = (DisplayName = "Play Local Sound"))
class ASCENDUNT_API UAnimNotify_PlayLocalSound : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	UAnimNotify_PlayLocalSound();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	// 재생할 사운드
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound = nullptr;

	// Mesh에 붙여서 재생할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAttachToMesh = true;

	// Attach 재생 시 사용할 소켓/본 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bAttachToMesh"))
	FName AttachPointName = NAME_None;

	// 볼륨 배수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	// 피치 배수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;

	// Owner가 Pawn이 아닐 경우에도 재생 허용할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAllowNonPawnOwner = false;

	// 로컬 컨트롤 체크를 끄고 싶을 때 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bRequireLocallyControlled = true;
	
	
};
