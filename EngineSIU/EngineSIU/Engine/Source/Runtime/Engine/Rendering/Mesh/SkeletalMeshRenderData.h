#pragma once

#include "Define.h"

// struct FSkeletalMeshRenderSection
// {
//     // int32 BoneIndex; // 이 섹션에서 사용하는 본 인덱스. -1이면 사용하지 않음. 필요한가? BaseIndex가 기본이 될지도. 사용한다면 본 계층 구조에서의 인덱스.
//
//     uint32 BaseIndex; 
//     uint32 NumTriangles;
//     TArray<uint16> BoneMap; // 이 섹션에서 사용하는 본 모음.
//     uint16 MaterialIndex; // 이 섹션에서 사용하는 머티리얼 인덱스.
//     uint32 BaseVertexIndex; // 이 섹션에서 사용하는 버텍스 인덱스.
//
//     FVertexInfo VertexInfo; // 버텍스 정보
//     FIndexInfo IndexInfo; // 인덱스 정보
//
// };
//
// class FSkeletalMeshRenderData
// {
// public:
//     TArray<FSkeletalMeshRenderSection> RenderSections; // 렌더링 섹션 모음.
//
// };
//
#pragma region SkeletalMesh
#define MAX_BONES_PER_VERTEX 4 
struct FSkeletalMeshVertex
{
    FVector Position;
    FVector Normal;
    FVector2D UV;
    FVector Tangent;
    FVector Bitangent;
    int BoneIndices[MAX_BONES_PER_VERTEX];
    float BoneWeights[MAX_BONES_PER_VERTEX];
};
struct FBone
{
    FMatrix skinningMatrix;
};

struct FSkeletalMeshRenderData
{
    // 오브젝트 식별용
    FString ObjectName;
    FString  DisplayName;

    // 버텍스 & 인덱스
    TArray<FSkeletalMeshVertex> Vertices;    // Position, Normal, UV, 그리고 BoneIndices/BoneWeights 포함
    TArray<UINT>               Indices;

    // GPU 업로드용 버퍼
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer  = nullptr;

    // 재질 & 서브셋 (StaticMesh와 동일)
    TArray<FObjMaterialInfo>  Materials;
    TArray<FMaterialSubset>   MaterialSubsets;

    // 바운딩
    FVector BoundingBoxMin;
    FVector BoundingBoxMax;

    // 스켈레톤 정보
    TArray<FString>     BoneNames;        // 본 이름 리스트
    TArray<int>          ParentBoneIndices;// 본 계층 트리 (각 본의 부모 인덱스)
    TArray<FMatrix>      ReferencePose;    // 본의 바인드 포즈 변환 행렬

    // 애니메이션 처리용 (선택)
    // TArray<AnimationClip>  Animations;
};
#pragma endregion
