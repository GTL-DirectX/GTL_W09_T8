#include "SkeletalMeshRenderData.h"

#include "UObject/Object.h"

void FSkeletalMeshRenderData::UpdateReferencePoseFromLocal()
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
            ReferencePose[i] =  LocalBindPose[i]* ReferencePose[parentIndex] ;
        }
        else
        {
            // 루트 본은 로컬이 곧 글로벌
            ReferencePose[i] = LocalBindPose[i];
        }
    }
}

void FSkeletalMeshRenderData::UpdateVerticesFromNewBindPose()
{
    int32 BoneCount = ReferencePose.Num();
    int32 VCount    = Vertices.Num();
    
    TArray<FMatrix> Delta; Delta.SetNum(BoneCount);
    for (int32 i = 0; i < BoneCount; ++i)
    {
        Delta[i] = FMatrix::Inverse(OrigineReferencePose[i]) * ReferencePose[i]  ;
    }
    
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

void FSkeletalMeshRenderData::ApplyBoneOffsetAndRebuild(int32 BoneIndex, FVector DeltaLoc, FRotator DeltaRot, FVector DeltaScale)
{
    if(!LocalBindPose.IsValidIndex(BoneIndex))
    {
        UE_LOG(LogLevel::Error, "Not Valid Bone Index");
        return;
    }
    FMatrix Mscale = FMatrix::CreateScaleMatrix(DeltaScale.X,DeltaScale.Y,DeltaScale.Z);
    FMatrix Mrot   = FMatrix::CreateRotationMatrix(DeltaRot.Roll,DeltaRot.Pitch,DeltaRot.Yaw);
    FMatrix DeltaM = FMatrix::CreateTranslationMatrix(DeltaLoc);
    DeltaM = DeltaM * Mrot * Mscale;
    LocalBindPose[BoneIndex] =  DeltaM * LocalBindPose[BoneIndex]; 
    UpdateReferencePoseFromLocal();
    UpdateVerticesFromNewBindPose();
    ComputeBounds();
    CreateBuffers();
}

void FSkeletalMeshRenderData::ComputeBounds()
{
    if (Vertices.Num() == 0) return;
    BoundingBoxMin = Vertices[0].Position;
    BoundingBoxMax = Vertices[0].Position;
    for (const auto& v : Vertices)
    {
        BoundingBoxMin.X = FMath::Min(BoundingBoxMin.X, v.Position.X);
        BoundingBoxMin.Y = FMath::Min(BoundingBoxMin.Y, v.Position.Y);
        BoundingBoxMin.Z = FMath::Min(BoundingBoxMin.Z, v.Position.Z);
        BoundingBoxMax.X = FMath::Max(BoundingBoxMax.X, v.Position.X);
        BoundingBoxMax.Y = FMath::Max(BoundingBoxMax.Y, v.Position.Y);
        BoundingBoxMax.Z = FMath::Max(BoundingBoxMax.Z, v.Position.Z);
    }
}

void FSkeletalMeshRenderData::CreateBuffers()
{
    if (VertexBuffer)
    {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }
    if (IndexBuffer)
    {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }
    TArray<FStaticMeshVertex> StaticVerts; StaticVerts.SetNum(Vertices.Num());
    for (int32 i = 0; i < Vertices.Num(); ++i)
    {
        const auto& S = Vertices[i];
        auto& D = StaticVerts[i];
        D.X = S.Position.X; D.Y = S.Position.Y; D.Z = S.Position.Z;
        D.NormalX = S.Normal.X; D.NormalY = S.Normal.Y; D.NormalZ = S.Normal.Z;
        D.TangentX = S.Tangent.X; D.TangentY = S.Tangent.Y; D.TangentZ = S.Tangent.Z;
        D.U = S.UV.X; D.V = S.UV.Y;
        D.R = D.G = D.B = D.A = 1.0f;
        D.MaterialIndex = 0;
    }
    
    // Vertex Buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(FStaticMeshVertex) * StaticVerts.Num();
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = StaticVerts.GetData();
    GEngineLoop.GraphicDevice.Device->CreateBuffer(&vbDesc, &vbData, &VertexBuffer);

    // Index Buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(UINT) * Indices.Num();
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = Indices.GetData();
    GEngineLoop.GraphicDevice.Device->CreateBuffer(&ibDesc, &ibData, &IndexBuffer);
}
