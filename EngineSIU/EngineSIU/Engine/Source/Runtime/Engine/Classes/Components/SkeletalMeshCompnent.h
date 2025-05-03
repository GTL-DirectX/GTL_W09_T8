#pragma once
#include "SkinnedMeshComponent.h"

class USkeletalMeshCompnent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshCompnent, USkinnedMeshComponent)
public:
    USkeletalMeshCompnent();

    FSkeletalMeshRenderData test;
};
