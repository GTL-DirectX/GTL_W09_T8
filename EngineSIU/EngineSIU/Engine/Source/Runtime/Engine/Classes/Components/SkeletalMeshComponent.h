#pragma once
#include "SkinnedMeshComponent.h"
#include "Rendering/Mesh/SkeletalMeshRenderData.h"

class USkeletalMesh;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    USkeletalMeshComponent();

public:
    virtual void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    virtual USkeletalMesh* GetSkeletalMesh() const;

private:
    USkeletalMesh* SkeletalMeshAsset;
};
