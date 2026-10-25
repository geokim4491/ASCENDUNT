#pragma once

#include "Components/MeshComponent.h"

namespace AscXRayOverlay
{
// Vision owns the overlay on 3P meshes. Never change source visibility, base materials or CustomDepth.
inline void SetMaterials(UMeshComponent* Mesh, UMaterialInterface* Material,
	const TArray<TObjectPtr<UMaterialInterface>>& SlotMaterials = {})
{
	if (!IsValid(Mesh))
	{
		return;
	}

	const bool bSlotsChanged = Mesh->MaterialSlotsOverlayMaterial != SlotMaterials;
	if (bSlotsChanged)
	{
		Mesh->MaterialSlotsOverlayMaterial = SlotMaterials;
	}

	if (Mesh->OverlayMaterial != Material)
	{
		Mesh->SetOverlayMaterial(Material);
	}
	else if (bSlotsChanged)
	{
		Mesh->PrecachePSOs();
		Mesh->MarkRenderStateDirty();
	}
}
}
