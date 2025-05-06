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
    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;
    virtual int CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const override;
    
    virtual void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    virtual USkeletalMesh* GetSkeletalMesh() const;

    int32 transboneidx = 0;
private:
    USkeletalMesh* SkeletalMeshAsset = nullptr;
};
