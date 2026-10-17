// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "AscenduntGameplayCueNotify_Burst.generated.h"

/*
* 라우터에서 ReplicatedToRelevant를 선택한 경우, Audience로 "누가 볼 것인가"를 세밀하게 조정할 수 있다.
* Audience에선 수신한 클라이언트에서 GC를 실행할지 말지 결정한다.
* 보통 ReplicatedToRelevant로 받고 타 깃 한 명 제외하는 경우에 많이 사용함.
*/
UENUM(BlueprintType)
enum class EAscenduntCueAudience : uint8
{
	/** relevancy 되는 모두(기본) */
	AllRelevant UMETA(DisplayName = "All Relevant"),

	/** 타깃제외 모든 클라에서 실행 */
	AllExceptTarget UMETA(DisplayName = "All Except Target"),

	/** 가해자제외 모든 클라에서 실행 */
	AllExceptInstigator UMETA(DisplayName = "All Except Instigator"),

	// AlliesOnly(로컬 플레이어 기준 아군만 추가 예정)

	// EnemiesOnly(로컬 플레이어 기준 적군만 추가 예정)
	// 복제로 전부 뿌린 뒤, 필터링은 네트워크 낭비가 있으므로 해당 방식으로 구현하지 않을 수도 있음
};

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class ASCENDUNT_API UAscenduntGameplayCueNotify_Burst : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()
	
public:
	/** "누가 볼 것인가" 정책 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|GCN")
	EAscenduntCueAudience Audience = EAscenduntCueAudience::AllRelevant;

	/** 데디 서버에서는 VFX/SFX 재생 안 함 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ascendunt|GCN")
	bool bSkipOnDedicatedServer = true;
	
protected:
	/** 필터링을 강제하기 위해 final 처리 */
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override final;

	/**
	 * C++ GameplayCue 구현 훅.
	 * 공통 Audience 필터를 통과한 뒤 BP 이벤트보다 먼저 호출된다.
	 */
	virtual bool ExecuteCueNative(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	/** 추가적인 BP 전용 연출이 필요한 경우에만 구현한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ascendunt|GCN")
	bool K2_ExecuteCue(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

private:
	bool ShouldPlay(AActor* MyTarget, const FGameplayCueParameters& Parameters) const;

	APawn* GetLocalPawn(UWorld* World) const;
	APawn* GetTargetPawn(AActor* MyTarget) const;
	APawn* GetInstigatorPawn(const FGameplayCueParameters& Parameters) const;
};
