#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

// 렌더링 용 구조체? 아직 사용처 모름.
struct FSkinnedVertex
{
    FVector Position; // Vertex의 위치
    FVector Normal; // Vertex의 법선 벡터
    FVector2D UV;
    FVector Tangent;
    FColor Color;
};

class USkinnedAsset : public UObject
{
    DECLARE_CLASS(USkinnedAsset, UObject)

public:
    USkinnedAsset() = default;

    virtual bool IsSkeletalMesh() const { return false; } // 이 섹션이 스켈레탈 메쉬인지 확인하는 함수

};

