#include "FBXManager.h"

#include <fbxsdk.h>

#include "UObject/Object.h"
#include "UserInterface/Console.h"

// 전역 인스턴스 정의
FFBXManager* GFBXManager = nullptr;

void FFBXManager::Initialize()
{
    SdkManager = FbxManager::Create();
    IOSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(IOSettings);
    Importer = FbxImporter::Create(SdkManager, "");
}

void FFBXManager::Release()
{
    if (Scene)      Scene->Destroy();
    if (Importer)   Importer->Destroy();
    if (IOSettings) IOSettings->Destroy();
    if (SdkManager) SdkManager->Destroy();
}

void FFBXManager::LoadFbx(const FString& FbxFilePath, FSkeletalMeshRenderData& OutRenderData)
{
    // FBX 파일 열기
    if (!Importer->Initialize(*FbxFilePath, -1, SdkManager->GetIOSettings()))
    {
        UE_LOG(LogLevel::Error, "Can not Find Fbx File Path : %s", *FbxFilePath);
        return;
    }

    // 씬 생성 및 임포트
    Scene = FbxScene::Create(SdkManager, "Scene");
    if (!Importer->Import(Scene))
    {
        UE_LOG(LogLevel::Error, "Can not Create FBX Scene Info : %s", *FbxFilePath);
        return;
    }

    // Triangulate (모든 메시를 삼각형으로)
    FbxGeometryConverter geomConverter(SdkManager);
    if (!geomConverter.Triangulate(Scene, true))
    {
        std::cerr << "Triangulation failed" << std::endl;
    }

    // 루트 노드 순회하여 데이터 출력
    FbxNode* root = Scene->GetRootNode();
    if (root)
    {
        for (int i = 0; i < root->GetChildCount(); ++i)
        {
            FbxNode* child =root->GetChild(i);

            // PrintStaticMeshData(root->GetChild(i));
            ExtractSkeletalMeshData(child, OutRenderData);
            std::cout << GetData(OutRenderData.ObjectName) << std::endl;
            // for (int i=0;i<OutRenderData.Vertices.Num();i++)
            // {
            //     std::cout << "Vertex "<< i << " Pos : "<<  OutRenderData.Vertices[i].Position.X << " " << OutRenderData.Vertices[i].Position.Y << " " << OutRenderData.Vertices[i].Position.Z << std::endl;
            //     std::cout << "Vertex "<< i << " Normal : " << OutRenderData.Vertices[i].Normal.X << " " << OutRenderData.Vertices[i].Normal.Y << " " << OutRenderData.Vertices[i].Normal.Z << std::endl;
            //     std::cout << "Vertex "<< i << " TextureUV : " << OutRenderData.Vertices[i].UV.X << " " << OutRenderData.Vertices[i].UV.Y  << std::endl;
            //     std::cout << "Vertex "<< i << " BoneIndex : " <<OutRenderData.Vertices[i].BoneIndices[0] << " " << OutRenderData.Vertices[i].BoneIndices[1] << std::endl;
            //     std::cout << "Vertex "<< i << " Weight : " <<OutRenderData.Vertices[i].BoneWeights[0] << " " << OutRenderData.Vertices[i].BoneWeights[1] << std::endl;
            // }
            // for (int i=0;i<OutRenderData.BoneNames.Num();i++)
            // {
            //     std::cout << GetData(OutRenderData.BoneNames[i]) << std::endl;
            // }
            // for (int i=0;i<OutRenderData.ParentBoneIndices.Num();i++)
            // {
            //     std::cout << OutRenderData.ParentBoneIndices[i];
            // }
            // for (int i=0;i<OutRenderData.Materials.Num();i++)
            // {
            //     std::wcout << OutRenderData.Materials[i].DiffuseTexturePath << std::endl;
            //     std::wcout << OutRenderData.Materials[i].SpecularTexturePath << std::endl;
            //     std::wcout << OutRenderData.Materials[i].AmbientTexturePath<< std::endl;
            //     std::wcout << OutRenderData.Materials[i].BumpTexturePath << std::endl;
            // }
            for (int i=0;i<OutRenderData.ReferencePose.Num();i++)
            {
                std::cout << GetData(OutRenderData.BoneNames[i]) << std::endl;
                OutRenderData.ReferencePose[i].PrintMatirx();
            }
        }
    }
}

void FFBXManager::ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData)
{
    // 1) 메시 얻기
    FbxMesh* mesh = node->GetMesh();
    if (!mesh) return;

    // 2) 이름 설정
    outData.ObjectName  = node->GetName();
    outData.DisplayName = node->GetName();

    // 3) 컨트롤 포인트 버텍스 초기화
    int cpCount = mesh->GetControlPointsCount();
    outData.Vertices.SetNum(cpCount);
    FbxVector4* cps = mesh->GetControlPoints();
    for (int i = 0; i < cpCount; ++i)
    {
        auto& v = outData.Vertices[i];
        v.Position = FVector(cps[i][0], cps[i][1], cps[i][2]);
        for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
        {
            v.BoneIndices[j] = 0;
            v.BoneWeights[j] = 0.0f;
        }
    }

    // 4) 인덱스 삼각화 및 초기화
    int polyCount = mesh->GetPolygonCount();
    outData.Indices.Reset();
    outData.Indices.Reserve(polyCount * 3);
    for (int p = 0; p < polyCount; ++p)
    {
        for (int k = 0; k < 3; ++k)
        {
            int idx = mesh->GetPolygonVertex(p, k);
            outData.Indices.Add(idx);
        }
    }

    // 5) 본 + 스킨 정보 수집
    for (int d = 0; d < mesh->GetDeformerCount(FbxDeformer::eSkin); ++d)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
        for (int c = 0; c < skin->GetClusterCount(); ++c)
        {
            FbxCluster* cluster = skin->GetCluster(c);
            FbxNode* boneNode = cluster->GetLink();

            int boneIndex = outData.BoneNames.Add(boneNode->GetName());

            FbxNode* parent = boneNode->GetParent();
            int parentIdx = -1;
            if (parent)
            {
                FString parentName = parent->GetName();
                for (int bi = 0; bi < outData.BoneNames.Num(); ++bi)
                {
                    if (outData.BoneNames[bi] == parentName)
                    {
                        parentIdx = bi;
                        break;
                    }
                }
            }
            outData.ParentBoneIndices.Add(parentIdx);

            FbxAMatrix tm;
            cluster->GetTransformLinkMatrix(tm);
            outData.ReferencePose.Add(ConvertToFMatrix(tm));

            int count = cluster->GetControlPointIndicesCount();
            const int* cpsIdx = cluster->GetControlPointIndices();
            const double* weights = cluster->GetControlPointWeights();
            for (int i = 0; i < count; ++i)
            {
                auto& vert = outData.Vertices[cpsIdx[i]];
                float w = static_cast<float>(weights[i]);
                for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
                {
                    if (vert.BoneWeights[j] == 0.0f)
                    {
                        vert.BoneIndices[j] = boneIndex;
                        vert.BoneWeights[j] = w;
                        break;
                    }
                }
            }
        }
    }

    // 6) 재질 & 서브셋 처리
    {
        // 재질 리셋
        outData.Materials.Reset();
        outData.MaterialSubsets.Reset();

        FbxNode* fbxNode = mesh->GetNode();
        int materialCount = fbxNode->GetMaterialCount();

        // 머티리얼 정보 수집
        for (int m = 0; m < materialCount; ++m)
        {
            FObjMaterialInfo matInfo;
            FbxSurfaceMaterial* fbxMat = fbxNode->GetMaterial(m);
            matInfo.MaterialName = fbxMat->GetName();

            auto ReadColor = [&](const char* propName, FVector& outVec)
            {
                FbxProperty prop = fbxMat->FindProperty(propName);
                if (prop.IsValid())
                {
                    FbxDouble3 val = prop.Get<FbxDouble3>();
                    outVec = FVector((float)val[0], (float)val[1], (float)val[2]);
                }
            };
            ReadColor(FbxSurfaceMaterial::sDiffuse,  matInfo.Diffuse);
            ReadColor(FbxSurfaceMaterial::sSpecular, matInfo.Specular);
            ReadColor(FbxSurfaceMaterial::sAmbient,  matInfo.Ambient);
            ReadColor(FbxSurfaceMaterial::sEmissive, matInfo.Emissive);

            matInfo.SpecularScalar     = static_cast<float>(fbxMat->FindProperty(FbxSurfaceMaterial::sShininess).Get<double>());
            matInfo.DensityScalar      = static_cast<float>(fbxMat->FindProperty("Ni").Get<double>());
            matInfo.TransparencyScalar = static_cast<float>(fbxMat->FindProperty(FbxSurfaceMaterial::sTransparencyFactor).Get<double>());
            matInfo.bTransparent       = (matInfo.TransparencyScalar > 0.0f);

            auto LoadTex = [&](const char* propName, FString& outName, FWString& outPath, uint32 flag)
            {
                FbxProperty prop = fbxMat->FindProperty(propName);
                if (!prop.IsValid()) return;
                int count = prop.GetSrcObjectCount<FbxFileTexture>();
                if (count <= 0) return;
                FbxFileTexture* tex = prop.GetSrcObject<FbxFileTexture>(0);
                outName = tex->GetName();
                std::string fp(tex->GetFileName());
                outPath = std::wstring(fp.begin(), fp.end());
                matInfo.TextureFlag |= flag;
            };
            LoadTex(FbxSurfaceMaterial::sDiffuse,  matInfo.DiffuseTextureName,  matInfo.DiffuseTexturePath,  1);
            LoadTex(FbxSurfaceMaterial::sAmbient,  matInfo.AmbientTextureName,  matInfo.AmbientTexturePath,  2);
            LoadTex(FbxSurfaceMaterial::sSpecular, matInfo.SpecularTextureName, matInfo.SpecularTexturePath, 4);
            LoadTex("NormalMap",                   matInfo.BumpTextureName,     matInfo.BumpTexturePath,     8);
            LoadTex(FbxSurfaceMaterial::sTransparencyFactor, matInfo.AlphaTextureName, matInfo.AlphaTexturePath, 16);

            outData.Materials.Add(matInfo);
        }

        // 서브셋 맵 생성
        TMap<int, FMaterialSubset> subsetMap;
        FbxLayerElementMaterial* elemMat = mesh->GetElementMaterial();
        if (elemMat)
        {
            const auto& indexArray = elemMat->GetIndexArray();
            for (int p = 0; p < polyCount; ++p)
            {
                int matIndex = indexArray.GetAt(p);
                if (matIndex < 0 || matIndex >= outData.Materials.Num())
                    matIndex = 0;

                auto& sub = subsetMap.FindOrAdd(matIndex);
                if (sub.IndexCount == 0)
                {
                    sub.IndexStart    = p * 3;
                    sub.MaterialIndex = matIndex;
                    sub.MaterialName  = outData.Materials[matIndex].MaterialName;
                }
                sub.IndexCount += 3;
            }
        }
        else if (outData.Materials.Num() > 0)
        {
            // 단일 머티리얼
            FMaterialSubset sub;
            sub.IndexStart    = 0;
            sub.IndexCount    = polyCount * 3;
            sub.MaterialIndex = 0;
            sub.MaterialName  = outData.Materials[0].MaterialName;
            subsetMap.Add(0, sub);
        }

        for (auto& kv : subsetMap)
        {
            outData.MaterialSubsets.Add(kv.Value);
        }
    }

    // 7) 바운딩 박스 계산
    ComputeBounds(outData.Vertices, outData.BoundingBoxMin, outData.BoundingBoxMax);

    // 8) GPU 버퍼 생성
    TArray<FStaticMeshVertex> StaticVerts;
    StaticVerts.SetNum(outData.Vertices.Num());
    for (int i = 0; i < outData.Vertices.Num(); ++i)
    {
        const auto& S = outData.Vertices[i];
        auto& D = StaticVerts[i];
        D.X = S.Position.X; D.Y = S.Position.Y; D.Z = S.Position.Z;
        D.R = D.G = D.B = D.A = 1.0f;
        D.NormalX  = S.Normal.X;  D.NormalY  = S.Normal.Y;  D.NormalZ  = S.Normal.Z;
        D.TangentX = S.Tangent.X; D.TangentY = S.Tangent.Y; D.TangentZ = S.Tangent.Z;
        D.U = S.UV.X; D.V = S.UV.Y;
        D.MaterialIndex = 0;
    }
    CreateBuffers(
        GEngineLoop.GraphicDevice.Device,
        StaticVerts,
        outData.Indices,
        outData.VertexBuffer,
        outData.IndexBuffer
    );
    
    // 9) Local Bind Pose Matrix 추출
    int32 BoneCount = outData.ReferencePose.Num();
    outData.LocalBindPose.SetNum(BoneCount);

    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FMatrix& GlobalMatrix = outData.ReferencePose[i];
        int32 ParentIndex = outData.ParentBoneIndices[i];

        if (ParentIndex >= 0 && ParentIndex < BoneCount)
        {
            const FMatrix& ParentGlobal = outData.ReferencePose[ParentIndex];
            // 로컬 = Parent⁻¹ * Global
            outData.LocalBindPose[i] = FMatrix::Inverse(ParentGlobal) * GlobalMatrix;
        }
        else
        {
            // 최상위 본(root)은 로컬 = 글로벌
            outData.LocalBindPose[i] = GlobalMatrix;
        }
    }
}



FMatrix FFBXManager::ConvertToFMatrix(const FbxAMatrix& in)
{
    // FBX 행렬 요소를 직접 Unreal FMatrix에 할당
    FMatrix out;
    out.M[0][0] = in.Get(0,0); out.M[0][1] = in.Get(0,1); out.M[0][2] = in.Get(0,2); out.M[0][3] = in.Get(0,3);
    out.M[1][0] = in.Get(1,0); out.M[1][1] = in.Get(1,1); out.M[1][2] = in.Get(1,2); out.M[1][3] = in.Get(1,3);
    out.M[2][0] = in.Get(2,0); out.M[2][1] = in.Get(2,1); out.M[2][2] = in.Get(2,2); out.M[2][3] = in.Get(2,3);
    out.M[3][0] = in.Get(3,0); out.M[3][1] = in.Get(3,1); out.M[3][2] = in.Get(3,2); out.M[3][3] = in.Get(3,3);
    return out;
}

void FFBXManager::ComputeBounds(const TArray<FSkeletalMeshVertex>& Verts, FVector& OutMin, FVector& OutMax)
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

void FFBXManager::CreateBuffers(ID3D11Device* Device, const TArray<FStaticMeshVertex>& Verts, const TArray<UINT>& Indices, ID3D11Buffer*& OutVB,
    ID3D11Buffer*& OutIB)
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

void FFBXManager::UpdateAndSkinMesh(FSkeletalMeshRenderData& MeshData, ID3D11DeviceContext* Context)
{
    const int32 VertCount = MeshData.Vertices.Num();
    const int32 BoneCount = MeshData.ReferencePose.Num();

    // 1) 본별 스키닝 매트릭스 계산
    TArray<FMatrix> SkinMatrices;
    SkinMatrices.SetNum(BoneCount);
    for (int b = 0; b < BoneCount; ++b)
    {
        const FMatrix& Bind    = MeshData.ReferencePose[b];
        const FMatrix& Current = MeshData.BoneTransforms[b];
        SkinMatrices[b] = Current * FMatrix::Inverse(Bind);
    }

    // 2) CPU 스키닝: 업데이트할 버텍스 데이터 임시 보관
    struct SVertex { FVector Pos, Normal; FVector2D UV; };
    std::vector<SVertex> Skinned(VertCount);

    for (int i = 0; i < VertCount; ++i)
    {
        const auto& Src = MeshData.Vertices[i];
        FVector skPos(0,0,0);
        FVector skNorm(0,0,0);

        for (int b = 0; b < MAX_BONES_PER_VERTEX; ++b)
        {
            int bi = Src.BoneIndices[b];
            float w = Src.BoneWeights[b];
            if (bi < 0 || w <= KINDA_SMALL_NUMBER) continue;

            const FMatrix& M = SkinMatrices[bi];
            skPos   += M.TransformPosition(Src.Position) * w;
            skNorm  += FMatrix::TransformVector(Src.Normal,M).GetSafeNormal() * w;
        }

        Skinned[i].Pos    = skPos;
        Skinned[i].Normal = skNorm.GetSafeNormal();
        Skinned[i].UV     = Src.UV;
    }

    // 3) D3D11 버텍스 버퍼 업데이트 (DYNAMIC+WRITE_DISCARD)
    D3D11_MAPPED_SUBRESOURCE Mapped;
    HRESULT hr = Context->Map(
        MeshData.VertexBuffer, 
        0, 
        D3D11_MAP_WRITE_DISCARD, 
        0, 
        &Mapped
    );
    if (SUCCEEDED(hr))
    {
        // FStaticMeshVertex 와 메모리 레이아웃이 동일하다고 가정
        FStaticMeshVertex* Dest = reinterpret_cast<FStaticMeshVertex*>(Mapped.pData);
        for (int i = 0; i < VertCount; ++i)
        {
            Dest[i].X = Skinned[i].Pos.X;
            Dest[i].Y = Skinned[i].Pos.Y;
            Dest[i].Z = Skinned[i].Pos.Z;
            Dest[i].NormalX = Skinned[i].Normal.X;
            Dest[i].NormalY = Skinned[i].Normal.Y;
            Dest[i].NormalZ = Skinned[i].Normal.Z;
            Dest[i].U = Skinned[i].UV.X;
            Dest[i].V = Skinned[i].UV.Y;
            // MaterialIndex, Color 등은 변하지 않으므로 그대로 유지
        }
        Context->Unmap(MeshData.VertexBuffer, 0);
    }
    else
    {
        // 맵 실패 시 로깅
        OutputDebugStringA("Failed to map skeletal vertex buffer for skinning update\\n");
    }
}

