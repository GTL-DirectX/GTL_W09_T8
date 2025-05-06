#include "FBXManager.h"

#include <fbxsdk.h>
#include <filesystem>

#include "UObject/Object.h"
#include "UObject/ObjectFactory.h"
#include "UserInterface/Console.h"

#include "Rendering/Mesh/SkeletalMesh.h"
#include "Engine/AssetManager.h"
#include "Rendering/Material/Material.h"

#include <fstream>
#include <sstream>
#include <Serialization/Serializer.h>

// 전역 인스턴스 정의
FFBXManager* GFBXManager = nullptr;

USkeletalMesh* FFBXManager::LoadFbx(const FString& FbxFilePath)
{
    // SkeletalMesh가 이미 로드되어 있는지 확인
    if (SkeletalMeshMap.Contains(FbxFilePath))
    {
        return *SkeletalMeshMap.Find(FbxFilePath);
    }

    // BinaryPath

    // RenderData
    FSkeletalMeshRenderData* RenderData = new FSkeletalMeshRenderData();

    // SkeletalMesh
    USkeletalMesh* NewSkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(&UAssetManager::Get()); // AssetManager를 Outer로 설정해서 Asset 총 관리하도록 설정.

    // Binary Check
    if (!LoadSkeletalMeshFromBinary(FbxFilePath, *RenderData))
    {
        // 바이너리 없으면 FBX 로드
        LoadSkeletalMeshRenderData(FbxFilePath, *RenderData);
        SaveSkeletalMeshToBinary(FbxFilePath, *RenderData);
    }

    NewSkeletalMesh->SetRenderData(RenderData);
    SkeletalMeshMap.Add(FbxFilePath, NewSkeletalMesh);

    // Material
    for (auto& MaterialInfo : RenderData->Materials)
    {
        UMaterial* Material = UMaterial::CreateMaterial(MaterialInfo);

       
        NewSkeletalMesh->AddMaterial(Material);
    }

    // 버텍스 및 인덱스 버퍼 생성
    TArray<FStaticMeshVertex> StaticVerts;
    StaticVerts.SetNum(RenderData->Vertices.Num());
    for (int i = 0; i < RenderData->Vertices.Num(); ++i)
    {
        const auto& S = RenderData->Vertices[i];
        auto& D = StaticVerts[i];
        D.X = S.Position.X; D.Y = S.Position.Y; D.Z = S.Position.Z;
        D.R = D.G = D.B = D.A = 1.0f;
        D.NormalX = S.Normal.X;  D.NormalY = S.Normal.Y;  D.NormalZ = S.Normal.Z;
        D.TangentX = S.Tangent.X; D.TangentY = S.Tangent.Y; D.TangentZ = S.Tangent.Z;
        D.U = S.UV.X; D.V = S.UV.Y;
        D.MaterialIndex = 0;
    }

    CreateBuffers(
        GEngineLoop.GraphicDevice.Device,
        StaticVerts,
        RenderData->Indices,
        RenderData->VertexBuffer,
        RenderData->IndexBuffer
    );

    return NewSkeletalMesh;
}

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
    SkeletalMeshMap.Empty();
}

void FFBXManager::LoadSkeletalMeshRenderData(const FString& FbxFilePath, FSkeletalMeshRenderData& OutRenderData)
{
    if (!std::filesystem::exists(FbxFilePath.ToWideString()))
    {
        UE_LOG(LogLevel::Error, TEXT("FBX File Not Found"));
        return;
    }

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
    OutRenderData.FilePath = FbxFilePath;
    // 루트 노드 순회하여 데이터 출력
    FbxNode* root = Scene->GetRootNode();
    if (root)
    {
        for (int i = 0; i < root->GetChildCount(); ++i)
        {
            FbxNode* child =root->GetChild(i);

            ExtractSkeletalMeshData(child, OutRenderData);
            std::cout << GetData(OutRenderData.FilePath) << std::endl;

            for (int i=0;i<OutRenderData.BoneNames.Num();i++)
            {
                std::cout << "Currnet Bone " <<  i <<" : " << GetData(OutRenderData.BoneNames[i]) << std::endl;
                if (OutRenderData.ParentBoneIndices[i] != -1)
                    std::cout << "Parent Idx : " << OutRenderData.ParentBoneIndices[i] << ", Parent Name : " << GetData(OutRenderData.BoneNames[OutRenderData.ParentBoneIndices[i]]) << std::endl;
                else
                    std::cout << "Parent Idx " << OutRenderData.ParentBoneIndices[i] << "  Root" <<std::endl;
            }
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
                OutRenderData.LocalBindPose[i].PrintMatirx();
            }
        }
    }
}
void FFBXManager::ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData)
{
    // 0) 좌표계 판단
    
    // FBX의 좌표계
    FbxAxisSystem sourceAxisSystem = node->GetScene()->GetGlobalSettings().GetAxisSystem();
    FbxSystemUnit sourceUnit = node->GetScene()->GetGlobalSettings().GetSystemUnit();

    // 이 엔진의 좌표계 정의
    const FbxAxisSystem EngineAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded); // Z-up, X-fwd(ParityOdd), LH
    const float EngineUnitScaleFactor = 0.01f; // 엔진 단위가 미터라고 가정 (cm -> m)

    FMatrix conversionMatrix = GetConversionMatrix(sourceAxisSystem, EngineAxisSystem);
    float conversionMatrixDet = conversionMatrix.Determinant3x3();
    double finalScaleFactor = sourceUnit.GetScaleFactor() * EngineUnitScaleFactor;
    // 1) 메시 얻기
    FbxMesh* mesh = node->GetMesh();
    if (!mesh)
    {
        for (int i = 0; i < node->GetChildCount(); i++)
        {
            ExtractSkeletalMeshData(node->GetChild(i), outData);
        }
        return;
    }

    outData.ObjectName = FString(node->GetName());
    // 2) 컨트롤 포인트 버텍스 초기화
    int cpCount = mesh->GetControlPointsCount();
    outData.Vertices.SetNum(cpCount);
    FbxVector4* cps = mesh->GetControlPoints();
    for (int i = 0; i < cpCount; ++i)
    {
        auto& v = outData.Vertices[i];
        FVector sourcePosition = FVector(cps[i][0], cps[i][1], cps[i][2]);
        v.Position = conversionMatrix.TransformPosition(sourcePosition) * finalScaleFactor; // 변환 행렬을 사용하여 변환
        for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
            v.BoneIndices[j] = v.BoneWeights[j] = 0;
    }

    // 3) 인덱스 삼각화 및 초기화
    int polyCount = mesh->GetPolygonCount();
    outData.Indices.Reset();
    outData.Indices.Reserve(polyCount * 3);

    // 4) 클러스터 정보 미리 수집 (본 노드, TransformLinkMatrix, 가중치)
    TSet<FbxNode*>            BoneNodeSet;
    TMap<FbxNode*, FMatrix>   ClusterBindPose;
    TMap<FbxNode*, TArray<std::pair<int, float>>>  BoneWeightsMap; 
      // boneNode -> array of (controlPointIdx, weight)

    // UV Element 가져오기 (첫 번째 UV 세트)
    FbxLayerElementUV* uvElement = mesh->GetLayer(0)->GetUVs(); // mesh->GetElementUV(0) 과 동일할 수 있음
    bool hasUVs = (uvElement != nullptr);

    // Normal Element 가져오기 (첫 번째 Normal 세트)
    FbxLayerElementNormal* normalElement = mesh->GetLayer(0)->GetNormals();
    bool hasNormals = (normalElement != nullptr);

    // TODO: Tangent Element 가져오기 (필요시)
    // FbxLayerElementTangent* tangentElement = mesh->GetLayer(0)->GetTangents();
    // bool hasTangents = (tangentElement != nullptr);
    for (int p = 0; p < polyCount; ++p) // 폴리곤 순회
    {
        if (mesh->GetPolygonSize(p) != 3)
        {
            // 삼각화되지 않은 폴리곤 처리 (경고 또는 예외)
            // 여기서는 간단히 건너뛰거나 삼각화 필요
            continue;
        }

        int triangleIndices[3]; // 현재 삼각형의 컨트롤 포인트 인덱스 저장
        bool validTriangle = true; // 삼각형 유효성 플래그

        for (int k = 0; k < 3; ++k) // 폴리곤의 각 정점 순회 (0, 1, 2)
        {
            int cpIndex = mesh->GetPolygonVertex(p, k); // 컨트롤 포인트 인덱스
            if (cpIndex < 0 || cpIndex >= cpCount)
            {
                validTriangle = false;
                continue; // 유효하지 않은 인덱스
            }

            // 인덱스 버퍼 채우기
            //outData.Indices.Add(cpIndex);

            // --- UV 추출 ---
            if (hasUVs)
            {
                int uvIndex = -1;
                // UV가 컨트롤 포인트별인지 폴리곤 정점별인지 확인
                if (uvElement->GetMappingMode() == FbxLayerElement::eByControlPoint)
                {
                    // 참조 방식 확인 (Direct 또는 IndexToDirect)
                    if (uvElement->GetReferenceMode() == FbxLayerElement::eDirect)
                        uvIndex = cpIndex;
                    else if (uvElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        uvIndex = uvElement->GetIndexArray().GetAt(cpIndex);
                }
                else if (uvElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int polygonVertexIndex = p * 3 + k; // 현재 폴리곤 정점의 인덱스
                    // 참조 방식 확인
                    if (uvElement->GetReferenceMode() == FbxLayerElement::eDirect)
                        uvIndex = polygonVertexIndex;
                    else if (uvElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        uvIndex = uvElement->GetIndexArray().GetAt(polygonVertexIndex);
                }

                if (uvIndex != -1)
                {
                    FbxVector2 uv = uvElement->GetDirectArray().GetAt(uvIndex);
                    // FBX UV는 Y가 반전될 수 있으므로 엔진에 맞게 조정 (texture->SamplerState 설정과 관련)
                    // 예: outData.Vertices[cpIndex].UV = FVector2D(uv[0], 1.0 - uv[1]);
                    //outData.Vertices[cpIndex].UV = FVector2D(uv[0], uv[1]);
                    outData.Vertices[cpIndex].UV = FVector2D(uv[0], 1.0 - uv[1]);
                    // 참고: 동일한 컨트롤 포인트가 다른 UV를 가질 경우 마지막 값으로 덮어쓰게 됨.
                    // 완벽한 처리를 위해서는 정점 분리(duplication) 필요.
                }
            }

            // --- 노멀 추출 (UV와 유사한 로직) ---
            if (hasNormals)
            {
                int normalIndex = -1;
                if (normalElement->GetMappingMode() == FbxLayerElement::eByControlPoint)
                {
                    if (normalElement->GetReferenceMode() == FbxLayerElement::eDirect)
                        normalIndex = cpIndex;
                    else if (normalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        normalIndex = normalElement->GetIndexArray().GetAt(cpIndex);
                }
                else if (normalElement->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
                {
                    int polygonVertexIndex = p * 3 + k;
                    if (normalElement->GetReferenceMode() == FbxLayerElement::eDirect)
                        normalIndex = polygonVertexIndex;
                    else if (normalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                        normalIndex = normalElement->GetIndexArray().GetAt(polygonVertexIndex);
                }

                if (normalIndex != -1)
                {
                    FbxVector4 normal = normalElement->GetDirectArray().GetAt(normalIndex);
                    // 좌표계 변환 필요시 수행
                    FVector sourceNormal = FVector(normal[0], normal[1], normal[2]);

                    outData.Vertices[cpIndex].Normal = conversionMatrix.TransformPosition(sourceNormal);
                    // 참고: UV와 마찬가지로 덮어쓰기 문제 가능성 있음
                }
            }

            // TODO: --- 탄젠트 추출 (필요하고 FBX에 데이터가 있다면) ---
            // if (hasTangents) { ... }
        }

        if (validTriangle)
        {
            // 삼각형 인덱스 추가
            // Determinant < 0이면, 좌표계 뒤집힌 것
            if (conversionMatrixDet < 0.0f)
            {
                outData.Indices.Add(mesh->GetPolygonVertex(p, 0));
                outData.Indices.Add(mesh->GetPolygonVertex(p, 2));
                outData.Indices.Add(mesh->GetPolygonVertex(p, 1));
            }
            else
            {
                outData.Indices.Add(mesh->GetPolygonVertex(p, 0));
                outData.Indices.Add(mesh->GetPolygonVertex(p, 1));
                outData.Indices.Add(mesh->GetPolygonVertex(p, 2));
            }
        }
    }


    // 5) 본 + 스킨 정보 수집
    for (int d = 0; d < mesh->GetDeformerCount(FbxDeformer::eSkin); ++d)
    {
        auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
        for (int c = 0; c < skin->GetClusterCount(); ++c)
        {
            auto* cluster = skin->GetCluster(c);
            auto* boneNode = cluster->GetLink();
            if (!boneNode || !boneNode->GetSkeleton()) continue;

            BoneNodeSet.Add(boneNode);

            // bind-pose (Global)
            FbxAMatrix fbxMat;
            cluster->GetTransformLinkMatrix(fbxMat);
            ClusterBindPose.Add(boneNode, ConvertToFMatrix(fbxMat));

            // 가중치 모으기
            int    count   = cluster->GetControlPointIndicesCount();
            const int* cpsIdx    = cluster->GetControlPointIndices();
            const double* weights = cluster->GetControlPointWeights();
            auto& arr = BoneWeightsMap.FindOrAdd(boneNode);
            for (int i = 0; i < count; ++i)
                arr.Emplace(cpsIdx[i], static_cast<float>(weights[i]));
        }
    }

    // 5) 본 노드별 depth 계산 (루트에서 얼마나 떨어져 있는지)
    TMap<FbxNode*, int> BoneDepth;
    std::function<int(FbxNode*)> ComputeDepth = [&](FbxNode* nd)->int {
        if (!nd || !nd->GetParent() || !nd->GetParent()->GetSkeleton())
            return 0;
        if (BoneDepth.Contains(nd)) return BoneDepth[nd];
        int d = 1 + ComputeDepth(nd->GetParent());
        return BoneDepth.Add(nd, d), d;
    };
    for (auto* bn : BoneNodeSet) ComputeDepth(bn);

    // 6) depth 오름차순 정렬된 배열 생성
    TArray<FbxNode*> SortedBones = BoneNodeSet.Array();
    SortedBones.Sort([&](FbxNode* A, FbxNode* B) {
        return BoneDepth[A] < BoneDepth[B];
    });

    // 7) 정렬된 순서대로 Bone 정보 채우기
    outData.BoneNames.Reset();
    outData.ParentBoneIndices.Reset();
    outData.ReferencePose.Reset();

    for (int i = 0; i < SortedBones.Num(); ++i)
    {
        FbxNode* boneNode = SortedBones[i];
        outData.BoneNames.Add(boneNode->GetName());

        // 부모 인덱스 찾기
        FbxNode* parent = boneNode->GetParent();
        int parentIdx = -1;
        if (parent && BoneNodeSet.Contains(parent))
        {
            parentIdx = SortedBones.Find(parent);
        }
        outData.ParentBoneIndices.Add(parentIdx);

        // bind-pose matrix
        outData.ReferencePose.Add(ClusterBindPose[boneNode]);
    }

    // 8) 이제 Vertex 쪽에 BoneIndices/BoneWeights 채우기
    //    (원본 Vertices 기준으로, SortedBones 순서의 가중치 적용)
    for (int bi = 0; bi < SortedBones.Num(); ++bi)
    {
        FbxNode* boneNode = SortedBones[bi];
        auto& arr = BoneWeightsMap[boneNode];
        for (auto& [cpIdx, w] : arr)
        {
            auto& vert = outData.Vertices[cpIdx];
            for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
            {
                if (vert.BoneWeights[j] == 0.0f)
                {
                    vert.BoneIndices[j] = bi;
                    vert.BoneWeights[j] = w;
                    break;
                }
            }
        }
    }

    // 9) (이전 로직) 재질 & 서브셋 처리 …
        
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

        LoadTex(FbxSurfaceMaterial::sDiffuse,               matInfo.DiffuseTextureName,     matInfo.DiffuseTexturePath,     1 << 1);
        LoadTex("NormalMap",                                matInfo.BumpTextureName,        matInfo.BumpTexturePath,        1 << 2);
        LoadTex(FbxSurfaceMaterial::sSpecular,              matInfo.SpecularTextureName,    matInfo.SpecularTexturePath,    1 << 3);
        LoadTex(FbxSurfaceMaterial::sAmbient,               matInfo.AmbientTextureName,     matInfo.AmbientTexturePath,     1 << 4);
        LoadTex(FbxSurfaceMaterial::sTransparencyFactor,    matInfo.AlphaTextureName,       matInfo.AlphaTexturePath,       1 << 5);
        
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
        

    // 7) 바운딩 박스 계산
    ComputeBounds(outData.Vertices, outData.BoundingBoxMin, outData.BoundingBoxMax);

    // 8) GPU 버퍼 생성 -> USkeletalMesh 생성 시점에 함


    // 10) LocalBindPose 계산
    int32 BoneCount = outData.ReferencePose.Num();
    outData.LocalBindPose.SetNum(BoneCount);
    for (int32 i = 0; i < BoneCount; ++i)
    {
        const FMatrix& G = outData.ReferencePose[i];
        int32 P = outData.ParentBoneIndices[i];
        if (P >= 0)
            outData.LocalBindPose[i] = FMatrix::Inverse(outData.ReferencePose[P]) * G;
        else
            outData.LocalBindPose[i] = G;
    }

    // 11) 원본 데이터 보관
    outData.OrigineVertices          = outData.Vertices;
    outData.OrigineReferencePose     = outData.ReferencePose;
    outData.BoneTransforms           = outData.ReferencePose;
    // (끝)
}
// void FFBXManager::ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData)
// {
//     // 1) 메시 얻기
//     FbxMesh* mesh = node->GetMesh();
//     if (!mesh) return;
//     
//     // 3) 컨트롤 포인트 버텍스 초기화
//     int cpCount = mesh->GetControlPointsCount();
//     outData.Vertices.SetNum(cpCount);
//     FbxVector4* cps = mesh->GetControlPoints();
//     for (int i = 0; i < cpCount; ++i)
//     {
//         auto& v = outData.Vertices[i];
//         v.Position = FVector(cps[i][0], cps[i][1], cps[i][2]);
//         for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
//         {
//             v.BoneIndices[j] = 0;
//             v.BoneWeights[j] = 0.0f;
//         }
//     }
//
//     // 4) 인덱스 삼각화 및 초기화
//     int polyCount = mesh->GetPolygonCount();
//     outData.Indices.Reset();
//     outData.Indices.Reserve(polyCount * 3);
//     for (int p = 0; p < polyCount; ++p)
//     {
//         for (int k = 0; k < 3; ++k)
//         {
//             int idx = mesh->GetPolygonVertex(p, k);
//             outData.Indices.Add(idx);
//         }
//     }
//
//     // 5) 본 + 스킨 정보 수집
//     for (int d = 0; d < mesh->GetDeformerCount(FbxDeformer::eSkin); ++d)
//     {
//         FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
//         for (int c = 0; c < skin->GetClusterCount(); ++c)
//         {
//             FbxCluster* cluster = skin->GetCluster(c);
//             FbxNode* boneNode = cluster->GetLink();
//
//             int boneIndex = outData.BoneNames.Add(boneNode->GetName());
//
//             FbxNode* parent = boneNode->GetParent();
//             int parentIdx = -1;
//             if (parent)
//             {
//                 FString parentName = parent->GetName();
//                 for (int bi = 0; bi < outData.BoneNames.Num(); ++bi)
//                 {
//                     if (outData.BoneNames[bi] == parentName)
//                     {
//                         parentIdx = bi;
//                         break;
//                     }
//                 }
//             }
//             outData.ParentBoneIndices.Add(parentIdx);
//
//             FbxAMatrix tm;
//             cluster->GetTransformLinkMatrix(tm);
//             outData.ReferencePose.Add(ConvertToFMatrix(tm));
//
//             int count = cluster->GetControlPointIndicesCount();
//             const int* cpsIdx = cluster->GetControlPointIndices();
//             const double* weights = cluster->GetControlPointWeights();
//             for (int i = 0; i < count; ++i)
//             {
//                 auto& vert = outData.Vertices[cpsIdx[i]];
//                 float w = static_cast<float>(weights[i]);
//                 for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
//                 {
//                     if (vert.BoneWeights[j] == 0.0f)
//                     {
//                         vert.BoneIndices[j] = boneIndex;
//                         vert.BoneWeights[j] = w;
//                         break;
//                     }
//                 }
//             }
//         }
//     }
//
//     // 6) 재질 & 서브셋 처리
//     {
//         // 재질 리셋
//         outData.Materials.Reset();
//         outData.MaterialSubsets.Reset();
//
//         FbxNode* fbxNode = mesh->GetNode();
//         int materialCount = fbxNode->GetMaterialCount();
//
//         // 머티리얼 정보 수집
//         for (int m = 0; m < materialCount; ++m)
//         {
//             FObjMaterialInfo matInfo;
//             FbxSurfaceMaterial* fbxMat = fbxNode->GetMaterial(m);
//             matInfo.MaterialName = fbxMat->GetName();
//
//             auto ReadColor = [&](const char* propName, FVector& outVec)
//             {
//                 FbxProperty prop = fbxMat->FindProperty(propName);
//                 if (prop.IsValid())
//                 {
//                     FbxDouble3 val = prop.Get<FbxDouble3>();
//                     outVec = FVector((float)val[0], (float)val[1], (float)val[2]);
//                 }
//             };
//             ReadColor(FbxSurfaceMaterial::sDiffuse,  matInfo.Diffuse);
//             ReadColor(FbxSurfaceMaterial::sSpecular, matInfo.Specular);
//             ReadColor(FbxSurfaceMaterial::sAmbient,  matInfo.Ambient);
//             ReadColor(FbxSurfaceMaterial::sEmissive, matInfo.Emissive);
//
//             matInfo.SpecularScalar     = static_cast<float>(fbxMat->FindProperty(FbxSurfaceMaterial::sShininess).Get<double>());
//             matInfo.DensityScalar      = static_cast<float>(fbxMat->FindProperty("Ni").Get<double>());
//             matInfo.TransparencyScalar = static_cast<float>(fbxMat->FindProperty(FbxSurfaceMaterial::sTransparencyFactor).Get<double>());
//             matInfo.bTransparent       = (matInfo.TransparencyScalar > 0.0f);
//
//             auto LoadTex = [&](const char* propName, FString& outName, FWString& outPath, uint32 flag)
//             {
//                 FbxProperty prop = fbxMat->FindProperty(propName);
//                 if (!prop.IsValid()) return;
//                 int count = prop.GetSrcObjectCount<FbxFileTexture>();
//                 if (count <= 0) return;
//                 FbxFileTexture* tex = prop.GetSrcObject<FbxFileTexture>(0);
//                 outName = tex->GetName();
//                 std::string fp(tex->GetFileName());
//                 outPath = std::wstring(fp.begin(), fp.end());
//                 matInfo.TextureFlag |= flag;
//             };
//             LoadTex(FbxSurfaceMaterial::sDiffuse,  matInfo.DiffuseTextureName,  matInfo.DiffuseTexturePath,  1);
//             LoadTex(FbxSurfaceMaterial::sAmbient,  matInfo.AmbientTextureName,  matInfo.AmbientTexturePath,  2);
//             LoadTex(FbxSurfaceMaterial::sSpecular, matInfo.SpecularTextureName, matInfo.SpecularTexturePath, 4);
//             LoadTex("NormalMap",                   matInfo.BumpTextureName,     matInfo.BumpTexturePath,     8);
//             LoadTex(FbxSurfaceMaterial::sTransparencyFactor, matInfo.AlphaTextureName, matInfo.AlphaTexturePath, 16);
//
//             outData.Materials.Add(matInfo);
//         }
//
//         // 서브셋 맵 생성
//         TMap<int, FMaterialSubset> subsetMap;
//         FbxLayerElementMaterial* elemMat = mesh->GetElementMaterial();
//         if (elemMat)
//         {
//             const auto& indexArray = elemMat->GetIndexArray();
//             for (int p = 0; p < polyCount; ++p)
//             {
//                 int matIndex = indexArray.GetAt(p);
//                 if (matIndex < 0 || matIndex >= outData.Materials.Num())
//                     matIndex = 0;
//
//                 auto& sub = subsetMap.FindOrAdd(matIndex);
//                 if (sub.IndexCount == 0)
//                 {
//                     sub.IndexStart    = p * 3;
//                     sub.MaterialIndex = matIndex;
//                     sub.MaterialName  = outData.Materials[matIndex].MaterialName;
//                 }
//                 sub.IndexCount += 3;
//             }
//         }
//         else if (outData.Materials.Num() > 0)
//         {
//             // 단일 머티리얼
//             FMaterialSubset sub;
//             sub.IndexStart    = 0;
//             sub.IndexCount    = polyCount * 3;
//             sub.MaterialIndex = 0;
//             sub.MaterialName  = outData.Materials[0].MaterialName;
//             subsetMap.Add(0, sub);
//         }
//
//         for (auto& kv : subsetMap)
//         {
//             outData.MaterialSubsets.Add(kv.Value);
//         }
//     }
//
//     // 7) 바운딩 박스 계산
//     ComputeBounds(outData.Vertices, outData.BoundingBoxMin, outData.BoundingBoxMax);
//
//     // 8) GPU 버퍼 생성
//     TArray<FStaticMeshVertex> StaticVerts;
//     StaticVerts.SetNum(outData.Vertices.Num());
//     for (int i = 0; i < outData.Vertices.Num(); ++i)
//     {
//         const auto& S = outData.Vertices[i];
//         auto& D = StaticVerts[i];
//         D.X = S.Position.X; D.Y = S.Position.Y; D.Z = S.Position.Z;
//         D.R = D.G = D.B = D.A = 1.0f;
//         D.NormalX  = S.Normal.X;  D.NormalY  = S.Normal.Y;  D.NormalZ  = S.Normal.Z;
//         D.TangentX = S.Tangent.X; D.TangentY = S.Tangent.Y; D.TangentZ = S.Tangent.Z;
//         D.U = S.UV.X; D.V = S.UV.Y;
//         D.MaterialIndex = 0;
//     }
//     CreateBuffers(
//         GEngineLoop.GraphicDevice.Device,
//         StaticVerts,
//         outData.Indices,
//         outData.VertexBuffer,
//         outData.IndexBuffer
//     );
//     
//     // 9) Local Bind Pose Matrix 추출
//     int32 BoneCount = outData.ReferencePose.Num();
//     outData.LocalBindPose.SetNum(BoneCount);
//
//     for (int32 i = 0; i < BoneCount; ++i)
//     {
//         const FMatrix& GlobalMatrix = outData.ReferencePose[i];
//         int32 ParentIndex = outData.ParentBoneIndices[i];
//
//         if (ParentIndex >= 0 && ParentIndex < BoneCount)
//         {
//             const FMatrix& ParentGlobal = outData.ReferencePose[ParentIndex];
//             // 로컬 = Parent⁻¹ * Global
//             outData.LocalBindPose[i] = FMatrix::Inverse(ParentGlobal) * GlobalMatrix;
//         }
//         else
//         {
//             // 최상위 본(root)은 로컬 = 글로벌
//             outData.LocalBindPose[i] = GlobalMatrix;
//         }
//     }
//
//     outData.OrigineVertices = outData.Vertices;
//     outData.OrigineReferencePose = outData.ReferencePose;
// }



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

bool FFBXManager::SaveSkeletalMeshToBinary(const FString& FilePath, const FSkeletalMeshRenderData& StaticMesh)
{
    FString BinaryPath = (FilePath + ".bin");

    std::ofstream File(BinaryPath.ToWideString(), std::ios::binary);
    if (!File.is_open())
    {
        UE_LOG(LogLevel::Error, "Failed to open file for writing: %s", FilePath);
        return false;
    }

    Serializer::WriteFString(File, StaticMesh.ObjectName);

    uint32 VertexCount = StaticMesh.Vertices.Num();
    File.write(reinterpret_cast<const char*>(&VertexCount), sizeof(VertexCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.Vertices.GetData()), VertexCount * sizeof(FSkeletalMeshVertex));

    uint32 OrigineVertexCount = StaticMesh.OrigineVertices.Num();
    File.write(reinterpret_cast<const char*>(&OrigineVertexCount), sizeof(OrigineVertexCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.OrigineVertices.GetData()), OrigineVertexCount * sizeof(FSkeletalMeshVertex));

    uint32 IndexCount = StaticMesh.Indices.Num();
    File.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.Indices.GetData()), IndexCount * sizeof(uint32));

    uint32 MaterialCount = StaticMesh.Materials.Num();
    File.write(reinterpret_cast<const char*>(&MaterialCount), sizeof(MaterialCount));
    for (const FObjMaterialInfo& Material : StaticMesh.Materials)
    {
        Serializer::WriteFString(File, Material.MaterialName);
        File.write(reinterpret_cast<const char*>(&Material.TextureFlag), sizeof(Material.TextureFlag));
        //File.write(reinterpret_cast<const char*>(&Material.bHasNormalMap), sizeof(Material.bHasNormalMap));
        File.write(reinterpret_cast<const char*>(&Material.bTransparent), sizeof(Material.bTransparent));
        File.write(reinterpret_cast<const char*>(&Material.Diffuse), sizeof(Material.Diffuse));
        File.write(reinterpret_cast<const char*>(&Material.Specular), sizeof(Material.Specular));
        File.write(reinterpret_cast<const char*>(&Material.Ambient), sizeof(Material.Ambient));
        File.write(reinterpret_cast<const char*>(&Material.Emissive), sizeof(Material.Emissive));

        File.write(reinterpret_cast<const char*>(&Material.SpecularScalar), sizeof(Material.SpecularScalar));
        File.write(reinterpret_cast<const char*>(&Material.DensityScalar), sizeof(Material.DensityScalar));
        File.write(reinterpret_cast<const char*>(&Material.TransparencyScalar), sizeof(Material.TransparencyScalar));
        File.write(reinterpret_cast<const char*>(&Material.BumpMultiplier), sizeof(Material.BumpMultiplier));
        File.write(reinterpret_cast<const char*>(&Material.IlluminanceModel), sizeof(Material.IlluminanceModel));

        Serializer::WriteFString(File, Material.DiffuseTextureName);
        Serializer::WriteFWString(File, Material.DiffuseTexturePath);

        Serializer::WriteFString(File, Material.AmbientTextureName);
        Serializer::WriteFWString(File, Material.AmbientTexturePath);

        Serializer::WriteFString(File, Material.SpecularTextureName);
        Serializer::WriteFWString(File, Material.SpecularTexturePath);

        Serializer::WriteFString(File, Material.BumpTextureName);
        Serializer::WriteFWString(File, Material.BumpTexturePath);

        Serializer::WriteFString(File, Material.AlphaTextureName);
        Serializer::WriteFWString(File, Material.AlphaTexturePath);
    }

    uint32 SubsetCount = StaticMesh.MaterialSubsets.Num();
    File.write(reinterpret_cast<const char*>(&SubsetCount), sizeof(SubsetCount));
    for (const FMaterialSubset& Subset : StaticMesh.MaterialSubsets)
    {
        Serializer::WriteFString(File, Subset.MaterialName);
        File.write(reinterpret_cast<const char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.write(reinterpret_cast<const char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.write(reinterpret_cast<const char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
    }

    uint32 BoneNamesCount = StaticMesh.BoneNames.Num();
    File.write(reinterpret_cast<const char*>(&BoneNamesCount), sizeof(BoneNamesCount));
    for (const FString& BoneName : StaticMesh.BoneNames)
    {
        Serializer::WriteFString(File, BoneName);
    }

    uint32 ParentBoneIndicesCount = StaticMesh.ParentBoneIndices.Num();
    File.write(reinterpret_cast<const char*>(&ParentBoneIndicesCount), sizeof(ParentBoneIndicesCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.ParentBoneIndices.GetData()), ParentBoneIndicesCount * sizeof(int));

    uint32 ReferencePoseCount = StaticMesh.ReferencePose.Num();
    File.write(reinterpret_cast<const char*>(&ReferencePoseCount), sizeof(ReferencePoseCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.ReferencePose.GetData()), ReferencePoseCount * sizeof(FMatrix));

    uint32 OrigineReferencePoseCount = StaticMesh.OrigineReferencePose.Num();
    File.write(reinterpret_cast<const char*>(&OrigineReferencePoseCount), sizeof(OrigineReferencePoseCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.OrigineReferencePose.GetData()), OrigineReferencePoseCount * sizeof(FMatrix));

    uint32 BoneTransformsCount = StaticMesh.BoneTransforms.Num();
    File.write(reinterpret_cast<const char*>(&BoneTransformsCount), sizeof(BoneTransformsCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.BoneTransforms.GetData()), BoneTransformsCount * sizeof(FMatrix));

    uint32 LocalBindPoseCount = StaticMesh.LocalBindPose.Num();
    File.write(reinterpret_cast<const char*>(&LocalBindPoseCount), sizeof(LocalBindPoseCount));
    File.write(reinterpret_cast<const char*>(StaticMesh.LocalBindPose.GetData()), LocalBindPoseCount * sizeof(FMatrix));

    File.close();

    return true;
}

bool FFBXManager::LoadSkeletalMeshFromBinary(const FString& FilePath, FSkeletalMeshRenderData& OutStaticMesh)
{
    FString BinaryPath = (FilePath + ".bin");

    std::ifstream File(BinaryPath.ToWideString(), std::ios::binary);

    if (!File.is_open())
    {
        UE_LOG(LogLevel::Error, "Failed to open file for reading: %s", FilePath);
        return false;
    }

    OutStaticMesh.FilePath = FilePath;
    // 1) 오브젝트 이름 읽기
    Serializer::ReadFString(File, OutStaticMesh.ObjectName);

    // 2) 버텍스 수 읽기
    uint32 VertexCount = 0;
    File.read(reinterpret_cast<char*>(&VertexCount), sizeof(VertexCount));
    OutStaticMesh.Vertices.SetNum(VertexCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.Vertices.GetData()), VertexCount * sizeof(FSkeletalMeshVertex));

    uint32 OrigineVertexCount = 0;
    File.read(reinterpret_cast<char*>(&OrigineVertexCount), sizeof(OrigineVertexCount));
    OutStaticMesh.OrigineVertices.SetNum(OrigineVertexCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.OrigineVertices.GetData()), OrigineVertexCount * sizeof(FSkeletalMeshVertex));

    // 3) 인덱스 수 읽기
    uint32 IndexCount = 0;
    File.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
    OutStaticMesh.Indices.SetNum(IndexCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.Indices.GetData()), IndexCount * sizeof(uint32));

    // 4) 머티리얼
    TArray<FWString> Textures;

    uint32 MaterialCount = 0;
    File.read(reinterpret_cast<char*>(&MaterialCount), sizeof(MaterialCount));
    OutStaticMesh.Materials.SetNum(MaterialCount);
    for (FObjMaterialInfo& Material : OutStaticMesh.Materials)
    {
        Serializer::ReadFString(File, Material.MaterialName);
        File.read(reinterpret_cast<char*>(&Material.TextureFlag), sizeof(Material.TextureFlag));
        File.read(reinterpret_cast<char*>(&Material.bTransparent), sizeof(Material.bTransparent));
        File.read(reinterpret_cast<char*>(&Material.Diffuse), sizeof(Material.Diffuse));
        File.read(reinterpret_cast<char*>(&Material.Specular), sizeof(Material.Specular));
        File.read(reinterpret_cast<char*>(&Material.Ambient), sizeof(Material.Ambient));
        File.read(reinterpret_cast<char*>(&Material.Emissive), sizeof(Material.Emissive));

        File.read(reinterpret_cast<char*>(&Material.SpecularScalar), sizeof(Material.SpecularScalar));
        File.read(reinterpret_cast<char*>(&Material.DensityScalar), sizeof(Material.DensityScalar));
        File.read(reinterpret_cast<char*>(&Material.TransparencyScalar), sizeof(Material.TransparencyScalar));
        File.read(reinterpret_cast<char*>(&Material.BumpMultiplier), sizeof(Material.BumpMultiplier));
        File.read(reinterpret_cast<char*>(&Material.IlluminanceModel), sizeof(Material.IlluminanceModel));

        Serializer::ReadFString(File, Material.DiffuseTextureName);
        Serializer::ReadFWString(File, Material.DiffuseTexturePath);

        Serializer::ReadFString(File, Material.AmbientTextureName);
        Serializer::ReadFWString(File, Material.AmbientTexturePath);

        Serializer::ReadFString(File, Material.SpecularTextureName);
        Serializer::ReadFWString(File, Material.SpecularTexturePath);

        Serializer::ReadFString(File, Material.BumpTextureName);
        Serializer::ReadFWString(File, Material.BumpTexturePath);

        Serializer::ReadFString(File, Material.AlphaTextureName);
        Serializer::ReadFWString(File, Material.AlphaTexturePath);

        if (!Material.DiffuseTexturePath.empty())
        {
            Textures.AddUnique(Material.DiffuseTexturePath);
        }
        if (!Material.AmbientTexturePath.empty())
        {
            Textures.AddUnique(Material.AmbientTexturePath);
        }
        if (!Material.SpecularTexturePath.empty())
        {
            Textures.AddUnique(Material.SpecularTexturePath);
        }
        if (!Material.BumpTexturePath.empty())
        {
            Textures.AddUnique(Material.BumpTexturePath);
        }
        if (!Material.AlphaTexturePath.empty())
        {
            Textures.AddUnique(Material.AlphaTexturePath);
        }
    }

    // Material Subset
    uint32 SubsetCount = 0;
    File.read(reinterpret_cast<char*>(&SubsetCount), sizeof(SubsetCount));
    OutStaticMesh.MaterialSubsets.SetNum(SubsetCount);
    for (FMaterialSubset& Subset : OutStaticMesh.MaterialSubsets)
    {
        Serializer::ReadFString(File, Subset.MaterialName);
        File.read(reinterpret_cast<char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.read(reinterpret_cast<char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.read(reinterpret_cast<char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
    }

    // 본 이름 인덱스
    uint32 BoneNamesCount = 0;
    File.read(reinterpret_cast<char*>(&BoneNamesCount), sizeof(BoneNamesCount));
    OutStaticMesh.BoneNames.SetNum(BoneNamesCount);
    for (FString& BoneName : OutStaticMesh.BoneNames)
    {
        Serializer::ReadFString(File, BoneName);
    }

    // 본 계층 트리
    uint32 ParentBoneIndicesCount = 0;
    File.read(reinterpret_cast<char*>(&ParentBoneIndicesCount), sizeof(ParentBoneIndicesCount));
    OutStaticMesh.ParentBoneIndices.SetNum(ParentBoneIndicesCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.ParentBoneIndices.GetData()), ParentBoneIndicesCount * sizeof(int));

    // 본 레퍼런스 변환 행렬
    uint32 ReferencePoseCount = 0;
    File.read(reinterpret_cast<char*>(&ReferencePoseCount), sizeof(ReferencePoseCount));
    OutStaticMesh.ReferencePose.SetNum(ReferencePoseCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.ReferencePose.GetData()), ReferencePoseCount * sizeof(FMatrix));

    // 원본 본 변환 행렬
    uint32 OrigineReferencePoseCount = 0;
    File.read(reinterpret_cast<char*>(&OrigineReferencePoseCount), sizeof(OrigineReferencePoseCount));
    OutStaticMesh.OrigineReferencePose.SetNum(OrigineReferencePoseCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.OrigineReferencePose.GetData()), OrigineReferencePoseCount * sizeof(FMatrix));

    // 본 변환행렬
    uint32 BoneTransformsCount = 0;
    File.read(reinterpret_cast<char*>(&BoneTransformsCount), sizeof(BoneTransformsCount));
    OutStaticMesh.BoneTransforms.SetNum(BoneTransformsCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.BoneTransforms.GetData()), BoneTransformsCount * sizeof(FMatrix));

    // 로컬 바인드 포즈
    uint32 LocalBindPoseCount = 0;
    File.read(reinterpret_cast<char*>(&LocalBindPoseCount), sizeof(LocalBindPoseCount));
    OutStaticMesh.LocalBindPose.SetNum(LocalBindPoseCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.LocalBindPose.GetData()), LocalBindPoseCount * sizeof(FMatrix));

    File.close();
    return true;
}

void FFBXManager::ProcessNodeRecursively(FbxNode* node, FBX::FImportSceneData& OutSceneData, int32 parentNodeIndex, const FMatrix& parentWorldTransform, const FMatrix& conversionMatrix, bool bFlipWinding)
{
    if (!node)
        return;

    using namespace FBX;
    // 1. 현재 Node에 대한 Node Info를 만든다
    FNodeInfo currentNodeInfo;
    currentNodeInfo.Name = FString(node->GetName());
    currentNodeInfo.ParentIndex = parentNodeIndex;
    currentNodeInfo.TempFbxNodePtr = node;

    // 엔진 Space에서의 로컬, 월드 트랜스폼을 구함
    currentNodeInfo.LocalTransform = GetNodeLocalTransformConverted(node, conversionMatrix);
    currentNodeInfo.WorldTransform = parentWorldTransform * currentNodeInfo.LocalTransform;

    // 노드를 하이라키에 먼저 넣고, 인덱스 받아옴
    int currentNodeIndex = OutSceneData.NodeHierarchy.Add(currentNodeInfo);
    OutSceneData.FbxNodeToIndexMap.Add(node, currentNodeIndex);

    // 2. 노드들의 Attribute 처리
    FbxNodeAttribute* attribute = node->GetNodeAttribute();
    int attributeDataIndex = -1;
    FbxNodeAttribute::EType attributeType = FbxNodeAttribute::eUnknown;

    if (attribute)
    {
        attributeType = attribute->GetAttributeType();
        OutSceneData.NodeHierarchy[currentNodeIndex].AttributeType = attributeType; // Update type in stored node info

        switch (attributeType)
        {
        case FbxNodeAttribute::eMesh:
        {
            FbxMesh* mesh = static_cast<FbxMesh*>(attribute);
            FMeshData meshData;
            ExtractMeshData(node, mesh, meshData, currentNodeInfo.WorldTransform, conversionMatrix, bFlipWinding);
            if (!meshData.Vertices.IsEmpty()) 
            { // Check if data was actually extracted
                attributeDataIndex = OutSceneData.MeshDatas.Add(meshData);
            }
            break;
        }
        case FbxNodeAttribute::eSkeleton:
        {
            // 스켈레톤 처리는 종종 메쉬 노드에서 발견되는 Skin/Cluster 정보와 밀접하게 연결됨.
            // 여기서는 단순히 이 노드가 스켈레톤 속성을 가졌다는 사실과 변환 정보 정도만 기록할 수 있음.
            // 실제 본(bone) 계층 구조 구축은 나중에 FbxSkin/FbxCluster 처리 단계 또는 ExtractMeshData 내에서 수행될 수 있음.
            // 현재로서는, 참조가 필요한 경우 별도의 본 리스트에 추가한다고 가정.
            FbxSkeleton* skeleton = static_cast<FbxSkeleton*>(attribute);
            // 예시: ExtractSkeletonData(node, skeleton, OutSceneData, currentNodeIndex);
            // 이 함수는 OutSceneData.SkeletonBones에 정보를 추가하고 나중에 부모 링크 등을 업데이트 할 수 있음.
            // 단순화를 위해, 여기서는 직접 처리를 건너뛰고 스키닝 정보에 의존할 수도 있음.
            UE_LOG(LogLevel::Display, "노드 %s는 스켈레톤입니다", *currentNodeInfo.Name);
            break;
        }
        case FbxNodeAttribute::eLight: // 라이트 속성인 경우
        {
            FbxLight* light = static_cast<FbxLight*>(attribute);
            FLightData lightData;
            ExtractLightData(node, light, lightData, currentNodeInfo.WorldTransform, conversionMatrix);
            attributeDataIndex = OutSceneData.LightDatas.Add(lightData); // 라이트 데이터 배열에 추가하고 인덱스 저장
            break;
        }
        case FbxNodeAttribute::eCamera: // 카메라 속성인 경우
        {
            FbxCamera* camera = static_cast<FbxCamera*>(attribute);
            FCameraData cameraData;
            ExtractCameraData(node, camera, cameraData, currentNodeInfo.WorldTransform, conversionMatrix);
            attributeDataIndex = OutSceneData.CameraDatas.Add(cameraData); // 카메라 데이터 배열에 추가하고 인덱스 저장
            break;
        }
        case FbxNodeAttribute::eNull: // Null 속성인 경우 (더미 노드)
        {
            UE_LOG(LogLevel::Warning, "노드 %s는 Null입니다", *currentNodeInfo.Name);
            // 종종 지오메트리/스켈레톤 속성 없이 그룹 노드나 본 조인트(뼈대 마디)로 사용됨
            break;
        }
        // 필요한 경우 다른 타입(eLODGroup, eMarker 등)에 대한 케이스 추가
        default: // 처리되지 않은 다른 속성 타입
            UE_LOG(LogLevel::Warning, "노드 %s는 처리되지 않은 속성 타입을 가집니다: %d", *currentNodeInfo.Name, attributeType);
            break;
        }
    }

    // 3. 자식 노드 재귀처리
    for (int i = 0; i < node->GetChildCount(); ++i) 
    {
        // 자식 노드에 대해 재귀 호출
        ProcessNodeRecursively(
            node->GetChild(i),          
            OutSceneData,               
            currentNodeIndex,           
            currentNodeInfo.WorldTransform, 
            conversionMatrix,           
            bFlipWinding               
        );
    }

}

void FFBXManager::ExtractMeshData(FbxNode* node, FbxMesh* mesh, FBX::FMeshData& outMeshData, const FMatrix& nodeWorldTransform, const FMatrix& conversionMatrix, bool bFlipWinding)
{
}

void FFBXManager::ExtractSkeletonData(FbxNode* node, FbxSkeleton* skeleton, FBX::FImportSceneData& OutSceneData, int32 nodeIndex)
{
}

void FFBXManager::ExtractLightData(FbxNode* node, FbxLight* light, FBX::FLightData& outLightData, const FMatrix& nodeWorldTransform, const FMatrix& conversionMatrix)
{
}

void FFBXManager::ExtractCameraData(FbxNode* node, FbxCamera* camera, FBX::FCameraData& outCameraData, const FMatrix& nodeWorldTransform, const FMatrix& conversionMatrix)
{
}

FMatrix FFBXManager::GetNodeLocalTransformConverted(FbxNode* node, const FMatrix& conversionMatrix)
{
    FbxAMatrix fbxLocalMatrix = node->EvaluateLocalTransform();
    FMatrix engineLocalMatrix = ConvertFbxMatrixToEngineMatrix(fbxLocalMatrix);

    // Apply coordinate system conversion: M_engine = C * M_fbx * C_inv
    // If C is orthogonal (rotation/reflection only), C_inv = C_transpose
    // For simplicity assuming conversionMatrix.Inverse() exists and handles it.
    // Note: This conversion might need refinement based on how C affects transforms vs vectors.
    // Often, just converting the Translation, Rotation, Scale components separately is more robust.
    // T = C * T_fbx
    // R = C * R_fbx * C_inv
    // S = S_fbx (scaling needs careful handling with non-uniform scaling and C)
    // For now, using the matrix multiplication approach (verify results):
    return conversionMatrix * engineLocalMatrix * FMatrix::Inverse(conversionMatrix); // Or Transpose() if appropriate
}

FMatrix FFBXManager::ConvertFbxMatrixToEngineMatrix(const FbxAMatrix& fbxMatrix)
{
    FMatrix engineMatrix;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            // Assuming Engine Matrix is Column-Major, FBX is Row-Major
            engineMatrix.M[j][i] = static_cast<float>(fbxMatrix[i][j]);
        }
    }
    return engineMatrix;
}

FMatrix FFBXManager::GetConversionMatrix(const FbxAxisSystem& sourceAxisSystem, const FbxAxisSystem& targetAxisSystem)
{
    FbxAMatrix sourceMatrix;
    FbxAMatrix targetMatrix;

    // FBX 좌표계 변환 행렬 생성
    BuildBasisMatrix(sourceAxisSystem, sourceMatrix);
    BuildBasisMatrix(targetAxisSystem, targetMatrix);

    FbxAMatrix conversionMatrix = targetMatrix.Inverse() * sourceMatrix;
    FMatrix result;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            result.M[i][j] = conversionMatrix.Get(j, i);// 전치
        }
    }
    return result;
}

void FFBXManager::BuildBasisMatrix(const FbxAxisSystem& system, FbxAMatrix& outMatrix)
{
    outMatrix.SetIdentity();

    int upAxisSign;
    FbxAxisSystem::EUpVector upAxis = system.GetUpVector(upAxisSign);

    FbxAxisSystem::ECoordSystem coordSystem = system.GetCoorSystem();

    FbxVector4 axisX(1, 0, 0);
    FbxVector4 axisY(0, 1, 0);
    FbxVector4 axisZ(0, 0, 1);

    FbxVector4 upVec;
    FbxVector4 rightVec;
    FbxVector4 forwardVec;

    if (upAxis == FbxAxisSystem::eXAxis)      upVec = axisX * upAxisSign;
    else if (upAxis == FbxAxisSystem::eYAxis) upVec = axisY * upAxisSign;
    else /* e ZAxis */                        upVec = axisZ * upAxisSign;

    if (upAxis == FbxAxisSystem::eYAxis) // Y-Up (Maya, OpenGL 방식 가정)
    {
        rightVec = axisX;
        forwardVec = axisZ;
    }
    else if (upAxis == FbxAxisSystem::eZAxis) // Z-Up (Max, Maya Z-up 방식 가정)
    {
        forwardVec = axisX;
        rightVec = axisY;
    }
    else // X-Up (덜 일반적)
    {
        rightVec = axisY;
        forwardVec = axisZ;
    }

    // 3. 왼손 좌표계(Left Handed)인 경우 Right 벡터 반전
    if (coordSystem == FbxAxisSystem::eLeftHanded)
    {
        rightVec = rightVec * -1.0;
    }

    FbxAMatrix invBasisMatrix;
    invBasisMatrix.SetRow(0, FbxVector4(axisX.DotProduct(rightVec), axisX.DotProduct(upVec), axisX.DotProduct(forwardVec)));
    invBasisMatrix.SetRow(1, FbxVector4(axisY.DotProduct(rightVec), axisY.DotProduct(upVec), axisY.DotProduct(forwardVec)));
    invBasisMatrix.SetRow(2, FbxVector4(axisZ.DotProduct(rightVec), axisZ.DotProduct(upVec), axisZ.DotProduct(forwardVec)));
    invBasisMatrix.SetRow(3, FbxVector4(0, 0, 0, 1)); // Translation 없음
    outMatrix = invBasisMatrix.Inverse(); // 더 안전하게 Inverse 사용
}

void FFBXManager::LoadFbxScene(const FString& FbxFilePath, FBX::FImportSceneData& OutSceneData)
{
}

