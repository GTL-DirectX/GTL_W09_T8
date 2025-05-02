#pragma once

#include "HAL/PlatformType.h"
#include "Container/Array.h"

#include "Rendering/Types/Buffers.h"

struct FSkeletalMeshRenderSection
{
    // int32 BoneIndex; // 이 섹션에서 사용하는 본 인덱스. -1이면 사용하지 않음. 필요한가? BaseIndex가 기본이 될지도. 사용한다면 본 계층 구조에서의 인덱스.

    uint32 BaseIndex; 
    uint32 NumTriangles;
    TArray<uint16> BoneMap; // 이 섹션에서 사용하는 본 모음.
    uint16 MaterialIndex; // 이 섹션에서 사용하는 머티리얼 인덱스.
    uint32 BaseVertexIndex; // 이 섹션에서 사용하는 버텍스 인덱스.

    FVertexInfo VertexInfo; // 버텍스 정보
    FIndexInfo IndexInfo; // 인덱스 정보

};

class FSkeletalMeshRenderData
{
public:
    TArray<FSkeletalMeshRenderSection> RenderSections; // 렌더링 섹션 모음.

};

