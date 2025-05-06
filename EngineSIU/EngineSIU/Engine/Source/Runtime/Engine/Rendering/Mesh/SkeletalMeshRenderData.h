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
    FString ObjectName;
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
    
    TArray<FMatrix>      BoneTransforms;
    TArray<FMatrix>      LocalBindPose;
    // 애니메이션 처리용 (선택)
    // TArray<AnimationClip>  Animations;

    void UpdateReferencePoseFromLocal()
    {
        int32 BoneCount = LocalBindPose.Num();
        ReferencePose.SetNum(BoneCount);
        
        // 루트부터 자식 순으로 순회해야 하므로, 
        // ParentBoneIndices 가 항상 부모 인덱스 < 자식 인덱스 관계를 보장하도록
        // 본들이 정렬되어 있다고 가정합니다.
        for (int32 i = 0; i < BoneCount; ++i)
        {
            const FMatrix& LocalMatrix = LocalBindPose[i];
            int32 ParentIndex = ParentBoneIndices[i];
        
            if (ParentIndex >= 0 && ParentIndex < BoneCount)
            {
                // 부모의 글로벌(Reference) 행렬에 곱해 자식의 글로벌을 구함
                ReferencePose[i] = ReferencePose[ParentIndex] * LocalMatrix;
            }
            else
            {
                // 최상위 본(root)은 로컬과 동일
                ReferencePose[i] = LocalMatrix;
            }
        }

    }

    //혹시 문제가 생길걸 대비해서 만들어둔 부모 자식 인덱스 순서 정렬 함수(위상 정렬) 
    void SortBonesByHierarchy(FSkeletalMeshRenderData& meshData)
    {
        int32 BoneCount = meshData.BoneNames.Num();

        // 1) 자식 리스트 생성
        TArray<TArray<int32>> Children;
        Children.SetNum(BoneCount);
        for (int32 i = 0; i < BoneCount; ++i)
        {
            int32 P = meshData.ParentBoneIndices[i];
            if (P >= 0 && P < BoneCount)
                Children[P].Add(i);
        }

        // 2) 루트부터 너비 우선 탐색(BFS)으로 순서 수집
        TArray<int32> NewOrder;
        NewOrder.Reserve(BoneCount);
        std::queue<int32> Queue;
        for (int32 i = 0; i < BoneCount; ++i)
        {
            if (meshData.ParentBoneIndices[i] < 0)
                Queue.push(i);
        }
        while (!Queue.empty())
        {
            int32 Curr = Queue.front(); Queue.pop();
            NewOrder.Add(Curr);
            for (int32 ChildIdx : Children[Curr])
                Queue.push(ChildIdx);
        }

        // 3) 리맵(old→new) 생성
        TArray<int32> Remap;
        Remap.SetNum(BoneCount);
        for (int32 NewIdx = 0; NewIdx < NewOrder.Num(); ++NewIdx)
        {
            Remap[ NewOrder[NewIdx] ] = NewIdx;
        }

        // 4) 모든 본 관련 배열 재정렬
        auto ReorderArray = [&](auto& ArrayIn, auto& ArrayOut)
        {
            ArrayOut.SetNum(BoneCount);
            for (int32 OldIdx = 0; OldIdx < BoneCount; ++OldIdx)
            {
                int32 NewIdx = Remap[OldIdx];
                ArrayOut[NewIdx] = ArrayIn[OldIdx];
            }
        };

        TArray<FString>           NewNames;
        TArray<int32>             NewParents;
        TArray<FMatrix>           NewRefPose;
        TArray<FMatrix>           NewLocalPose;

        ReorderArray(meshData.BoneNames,       NewNames);
        ReorderArray(meshData.ParentBoneIndices, NewParents);
        ReorderArray(meshData.ReferencePose,   NewRefPose);
        ReorderArray(meshData.LocalBindPose,   NewLocalPose);

        // 5) 부모 인덱스 값도 remap
        for (int32& P : NewParents)
        {
            if (P >= 0) P = Remap[P];
        }

        // 6) 결과를 원본에 복사
        meshData.BoneNames        = NewNames;
        meshData.ParentBoneIndices = NewParents;
        meshData.ReferencePose    = NewRefPose;
        meshData.LocalBindPose    = NewLocalPose;
    }
    

    void UpdateVerticesFromNewBindPose()
    {
        int32 BoneCount = ReferencePose.Num();
        int32 VCount    = Vertices.Num();

        // Delta 행렬 계산
        TArray<FMatrix> Delta; Delta.SetNum(BoneCount);
        for (int32 i = 0; i < BoneCount; ++i)
        {
            Delta[i] = ReferencePose[i] * FMatrix::Inverse(OrigineReferencePose[i]);
        }

        // 각 버텍스에 대해
        for (int32 vi = 0; vi < OrigineVertices.Num(); ++vi)
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
