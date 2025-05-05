#pragma once

#include "SkinnedMeshComponent.h"

#include "Rendering/Mesh/SkeletalMeshRenderData.h"

class USkeletalMesh;
class USkeletalMeshCompnent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshCompnent, USkinnedMeshComponent)

public:
    USkeletalMeshCompnent();
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

    USkeletalMesh* SkeletalMesh = nullptr;
};
