#include "SkeletalMeshRenderData.h"

#include "UObject/Object.h"

void FSkeletalMeshRenderData::ApplyBoneOffsetAndRebuild(int32 BoneIndex, FVector DeltaLoc, FRotator DeltaRot, FVector DeltaScale)
{                                                        
    if(!LocalBindPose.IsValidIndex(BoneIndex))
    {
        UE_LOG(LogLevel::Error, "Not Valid Bone Index");
        return;
    }
    // 델타 행렬 (Scale -> Rot -> Trans)
    FMatrix Mscale = FMatrix::CreateScaleMatrix(1,1,1);
    FMatrix Mrot   = FMatrix::CreateRotationMatrix(DeltaRot.Roll,DeltaRot.Pitch,DeltaRot.Yaw);
    FMatrix DeltaM = FMatrix::CreateTranslationMatrix(DeltaLoc);
    DeltaM = DeltaM * Mrot * Mscale;
    // 1) 로컬 바인드 포즈 적용
    LocalBindPose[BoneIndex] =  DeltaM * LocalBindPose[BoneIndex]; // 위치 고려 필요
    // 2) 글로벌 ReferencePose 재계산
    UpdateReferencePoseFromLocal();
    // 3) BoneTransforms 동기화
    BoneTransforms = ReferencePose;
    // 4) 버텍스 재계산
    UpdateVerticesFromNewBindPose();
    // 5) 바운딩 & GPU 버퍼
    ComputeBounds(Vertices, BoundingBoxMin, BoundingBoxMax);
    
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
    VertexBuffer->Release();
    IndexBuffer->Release();
    VertexBuffer = nullptr;
    IndexBuffer = nullptr;
    CreateBuffers(
        GEngineLoop.GraphicDevice.Device,
        StaticVerts,
        Indices,
        VertexBuffer,
        IndexBuffer
    );
    // OrigineReferencePose = ReferencePose;
    // OrigineVertices = Vertices;
}

void FSkeletalMeshRenderData::ComputeBounds(const TArray<FSkeletalMeshVertex>& Verts, FVector& OutMin, FVector& OutMax)
{
    if (Verts.Num() == 0) return;
    OutMin = Verts[0].Position;
    OutMax = Verts[0].Position;
    for (const auto& v : Verts)
    {
        OutMin.X = FMath::Min(OutMin.X, v.Position.X);
        OutMin.Y = FMath::Min(OutMin.Y, v.Position.Y);
        OutMin.Z = FMath::Min(OutMin.Z, v.Position.Z);
        OutMax.X = FMath::Max(OutMax.X, v.Position.X);
        OutMax.Y = FMath::Max(OutMax.Y, v.Position.Y);
        OutMax.Z = FMath::Max(OutMax.Z, v.Position.Z);
    }
}

void FSkeletalMeshRenderData::CreateBuffers(ID3D11Device* Device, const TArray<FStaticMeshVertex>& Verts, const TArray<UINT>& Indices,
    ID3D11Buffer*& OutVB, ID3D11Buffer*& OutIB)
{
    // Vertex Buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(FStaticMeshVertex) * Verts.Num();
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = Verts.GetData();
    Device->CreateBuffer(&vbDesc, &vbData, &OutVB);

    // Index Buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(UINT) * Indices.Num();
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = Indices.GetData();
    Device->CreateBuffer(&ibDesc, &ibData, &OutIB);
}
