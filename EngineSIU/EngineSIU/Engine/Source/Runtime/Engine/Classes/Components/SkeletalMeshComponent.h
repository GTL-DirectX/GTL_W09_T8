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
    virtual UObject* Duplicate(UObject* InOuter) override;
    void GetProperties(TMap<FString, FString>& OutProperties) const override;
    void SetProperties(const TMap<FString, FString>& InProperties) override;

    virtual void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    virtual USkeletalMesh* GetSkeletalMesh() const;

private:
    USkeletalMesh* SkeletalMeshAsset = nullptr;
};
