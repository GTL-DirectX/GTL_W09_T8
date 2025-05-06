#pragma once

#include <queue>

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
    FString FilePath;

    // 버텍스 & 인덱스
    TArray<FSkeletalMeshVertex> Vertices;    // Position, Normal, UV, 그리고 BoneIndices/BoneWeights 포함
    TArray<FSkeletalMeshVertex> OrigineVertices;
    
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
    TArray<FMatrix>      OrigineReferencePose; 
    TArray<FMatrix>     SkinningMatrix;
    
    TArray<FMatrix>      BoneTransforms;
    TArray<FMatrix>      LocalBindPose;
    // 애니메이션 처리용 (선택)
    // TArray<AnimationClip>  Animations;

    void UpdateReferencePoseFromLoacl()
    {
        const int32 BoneCount = LocalBindPose.Num();
        ReferencePose.SetNum(BoneCount);
        OrigineReferencePose.SetNum(BoneCount); // 원본 백업

        for (int32 i = 0; i < BoneCount; ++i)
        {
            int parentIndex = ParentBoneIndices[i];

            if (parentIndex >= 0)
            {
                // 부모의 글로벌 포즈에 현재 로컬을 곱함
                ReferencePose[i] =
                     ReferencePose[parentIndex] * LocalBindPose[i];
            }
            else
            {
                // 루트 본은 로컬이 곧 글로벌
                ReferencePose[i] = LocalBindPose[i];
            }
        }
    }
    
    void UpdateVerticesFromNewBindPose()
    {
        int32 BoneCount = ReferencePose.Num();
        int32 VCount    = Vertices.Num();

        // Delta 행렬 계산
        TArray<FMatrix> Delta; Delta.SetNum(BoneCount);
        for (int32 i = 0; i < BoneCount; ++i)
        {
            Delta[i] = ReferencePose[i] * FMatrix::Inverse(SkinningMatrix[i]);
        }

        // 각 버텍스에 대해
        for (int32 vi = 0; vi < VCount; ++vi)
        {
            const auto& src = OrigineVertices[vi];
            auto&       dst = Vertices[vi];

            // 1) 가중치 합 계산 및 정규화
            float WeightSum = 0.0f;
            for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
                WeightSum += src.BoneWeights[j];
            
            // 2) 가중치 정규화 (합이 0이 아니면)
            float InverseSum = (WeightSum > KINDA_SMALL_NUMBER) ? (1.0f / WeightSum) : 0.0f;

            FVector P(0), N(0), T(0), B(0);
            
            // 3) 스키닝 적용
            for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
            {
                float w = src.BoneWeights[j] * InverseSum;
                if (w <= 0.0f) continue;
                int bi = src.BoneIndices[j];
                const FMatrix& M = Delta[bi];

                P += M.TransformPosition(src.Position) * w;
                N += FMatrix::TransformVector(src.Normal, M) * w;
                T += FMatrix::TransformVector(src.Tangent,M) * w;
                B += FMatrix::TransformVector(src.Bitangent,M) * w;
            }

            // 4) 결과 저장
            dst.Position  = P;
            dst.Normal    = N.GetSafeNormal();
            dst.Tangent   = T.GetSafeNormal();
            dst.Bitangent = B.GetSafeNormal();
            dst.UV        = src.UV;
            
            // BoneIndices/Weights는 원본 그대로 유지
            for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
            {
                dst.BoneIndices[j] = src.BoneIndices[j];
                dst.BoneWeights[j] = src.BoneWeights[j];
            }
        }
    }
    void ApplyBoneOffsetAndRebuild(
    int32                   BoneIndex,
    FVector DeltaLoc, FRotator DeltaRot, FVector DeltaScale);

    void ComputeBounds(
    const TArray<FSkeletalMeshVertex>& Verts,
    FVector& OutMin,
    FVector& OutMax);
    void CreateBuffers(
        ID3D11Device* Device,
        const TArray<FStaticMeshVertex>& Verts,
        const TArray<UINT>& Indices,
        ID3D11Buffer*& OutVB,
        ID3D11Buffer*& OutIB);
};
#pragma endregion
