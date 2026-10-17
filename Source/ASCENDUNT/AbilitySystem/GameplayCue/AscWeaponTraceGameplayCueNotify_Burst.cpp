#include "AscWeaponTraceGameplayCueNotify_Burst.h"

#include "ASCENDUNT/Weapon/AscenduntWeapon.h"
#include "ASCENDUNT/Weapon/WeaponDefinitionData.h"

#include "GameplayEffectTypes.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace AscWeaponTraceCue
{
	static const FName TraceStartParameterName(TEXT("User_TraceStart"));
	static const FName TraceEndParameterName(TEXT("User_TraceEnd"));
}

bool UAscWeaponTraceGameplayCueNotify_Burst::ExecuteCueNative(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return false;
	}

	UWorld* World = MyTarget->GetWorld();
	if (!World)
	{
		return false;
	}

	const AAscenduntWeapon* Weapon = Cast<AAscenduntWeapon>(Parameters.GetSourceObject());
	if (!Weapon)
	{
		return false;
	}

	UNiagaraSystem* TraceSystem = Weapon->GetCurrentModeStats().FireTraceVFXNiagara;
	if (!TraceSystem)
	{
		return false;
	}


	// Trail은 순수 연출이므로 이 머신에서 실제로 보이는 무기의 Muzzle을 사용한다.
	// 로컬 1인칭 오너는 WeaponMesh1P, 다른 클라이언트/3인칭은 WeaponMesh가 선택된다.
	const FVector TraceStart = Weapon->GetMuzzleVFXTransform().GetLocation();
	const FVector TraceEnd = Parameters.Location;

	UNiagaraComponent* TraceComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		TraceSystem,
		TraceStart,
		FRotator::ZeroRotator,
		FVector::OneVector,
		/*bAutoDestroy=*/true,
		/*bAutoActivate=*/false,
		ENCPoolMethod::None,
		/*bPreCullCheck=*/true);

	if (!TraceComponent)
	{
		return false;
	}

	TraceComponent->SetVariablePosition(AscWeaponTraceCue::TraceStartParameterName, TraceStart);
	TraceComponent->SetVariablePosition(AscWeaponTraceCue::TraceEndParameterName, TraceEnd);
	TraceComponent->Activate(true);

	return true;
}
