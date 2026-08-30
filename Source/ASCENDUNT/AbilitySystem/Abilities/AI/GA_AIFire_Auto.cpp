// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_AIFire_Auto.h"

#include "GameFramework/Pawn.h"
#include "ASCENDUNT/Weapon/AscenduntWeapon.h"

bool UGA_AIFire_Auto::BuildViewRayAndMuzzle(
	const FGameplayAbilityActorInfo* ActorInfo,
	AAscenduntWeapon* Weapon,
	FVector& OutViewStart,
	FVector& OutViewDir,
	FVector& OutMuzzleLoc) const
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const APawn* AvatarPawn = Cast<APawn>(Avatar);

	if (!AvatarPawn)
	{
		// AI 가 아닌 곳에 잘못 붙은 경우. 기본 구현에 맡긴다.
		return Super::BuildViewRayAndMuzzle(ActorInfo, Weapon, OutViewStart, OutViewDir, OutMuzzleLoc);
	}

	// 조준선: 눈높이에서 컨트롤러가 바라보는 방향.
	// SetFocus 가 걸려 있으면 AAIController 가 표적을 향해 갱신해 둔 값이다.
	OutViewStart = AvatarPawn->GetPawnViewLocation();
	OutViewDir = AvatarPawn->GetBaseAimRotation().Vector().GetSafeNormal();

	// 총구: 무기 액터의 위치.
	//
	// Weapon->GetMuzzleTransform() 을 부르지 않는다. AI 무기는 WeaponMesh 에
	// 스켈레탈 메시를 넣지 않아(총구 소켓은 캐릭터 스켈레톤에 있다) 그 함수가
	// 발사할 때마다 LogSkinnedMeshComp 경고를 찍기 때문이다.
	//
	// 무기 액터는 캐릭터의 무기 소켓에 SnapToTarget 으로 붙으므로
	// 액터 위치가 곧 그 소켓 위치다. 결과값은 같고 경고만 사라진다.
	OutMuzzleLoc = Weapon ? Weapon->GetActorLocation() : OutViewStart;

	return true;
}
