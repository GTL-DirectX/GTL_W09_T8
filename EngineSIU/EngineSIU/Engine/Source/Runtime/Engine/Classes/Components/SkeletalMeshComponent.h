#pragma once
#include "SkinnedMeshComponent.h"
#include "Rendering/Mesh/SkeletalMeshRenderData.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    USkeletalMeshComponent()=default;

    FSkeletalMeshRenderData test;
};
