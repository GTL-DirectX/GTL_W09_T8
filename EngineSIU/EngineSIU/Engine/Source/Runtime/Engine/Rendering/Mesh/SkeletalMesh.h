#pragma once

#include "SkinnedAsset.h"

class FSkeletalMeshRenderData;

/*
    SkinnedAsset은 시간에 따라 변형되는 메시 -> SkeletalMesh가 아닐 수도 있음
    ex) Cloth, Hair, Skin 등

    SkeletalMesh는 본 계층 + Skin 가중치를 가진 메시.
*/

class USkeletalMesh : public USkinnedAsset
{   
    DECLARE_CLASS(USkeletalMesh, USkinnedAsset)

public:
    USkeletalMesh() = default;

    virtual bool IsSkeletalMesh() const override { return true; } // 이 섹션이 스켈레탈 메쉬인지 확인하는 함수

private:
    FSkeletalMeshRenderData* RenderData = nullptr;
    
};

