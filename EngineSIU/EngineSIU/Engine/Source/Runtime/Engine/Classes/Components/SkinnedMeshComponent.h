#pragma once
#include "Rendering/Mesh/SkinnedAsset.h"
#include "Components/MeshComponent.h"

class USkinnedMeshComponent :public UMeshComponent
{
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)
public:
    USkinnedMeshComponent() = default;

    //virtual UObject* Duplicate(UObject* InOuter) override;

    //void GetProperties(TMap<FString, FString>& OutProperties) const override;
    //void SetProperties(const TMap<FString, FString>& InProperties) override;

    USkinnedAsset* GetSkinnedAsset() const { return SkinnedAsset; }

    // 이후 set하거나 다른 부분은 skeletal mesh 및 skinned mesh에서 어떤 값들을 사용할지 아직 정하지 않았던 것 같아서 비워둠.

protected:
    USkinnedAsset* SkinnedAsset = nullptr;
};

