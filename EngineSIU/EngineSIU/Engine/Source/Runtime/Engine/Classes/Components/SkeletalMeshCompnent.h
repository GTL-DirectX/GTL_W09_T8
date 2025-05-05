#pragma once

#include "SkinnedMeshComponent.h"

class USkeletalMesh;
class USkeletalMeshCompnent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshCompnent, USkinnedMeshComponent)
public:
    USkeletalMeshCompnent();

public:
    virtual void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    virtual USkeletalMesh* GetSkeletalMesh() const;

private:
    USkeletalMesh* SkeletalMeshAsset;
    
};
