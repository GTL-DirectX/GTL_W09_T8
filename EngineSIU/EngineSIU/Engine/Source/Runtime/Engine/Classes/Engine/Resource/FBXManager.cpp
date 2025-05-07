#include "FBXManager.h"

#include <fbxsdk.h>
#include <filesystem>

#include "UObject/Object.h"
#include "UObject/ObjectFactory.h"
#include "UserInterface/Console.h"

#include "Rendering/Mesh/SkeletalMesh.h"
#include "Engine/AssetManager.h"
#include "Math/Quat.h"
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

    RenderData->CreateBuffers();

    return NewSkeletalMesh;
}

TArray<USkeletalMesh*> FFBXManager::LoadFbxAll(const FString& FbxFilePath)
{
    TArray<USkeletalMesh*> SkeletalMeshes;
    LoadAllMeshesFromFbx(FbxFilePath, SkeletalMeshes);

    for (auto& SkeletalMesh : SkeletalMeshes)
    {
        SkeletalMeshMap.Add(SkeletalMesh->GetRenderData()->ObjectName, SkeletalMesh);
    }
    return SkeletalMeshes;
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

    // 씬 좌표계 변환
    const FbxAxisSystem EngineAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
    EngineAxisSystem.DeepConvertScene(Scene);

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
        }
    }
}
void FFBXManager::ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData)
{
    outData.ObjectName = FString(node->GetName());
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
    
    int cpCount = mesh->GetControlPointsCount();
    // Vertex 정보를 추출합니다.
    ExtractVertexPosition(outData, mesh, cpCount);
    
    int polyCount = mesh->GetPolygonCount();

    // Index, Normal, UV, Tangent 값등을 추출합니다.
    ExtractVertexInfo(outData, mesh, cpCount, polyCount);

    // 본 계층구조와 가중치, Global Bind Pose를 추출합니다.
    ExtractBoneInfo(outData, mesh);
    
    // Global Bind Pose를 기반으로 Local Bind Pose를 생성합니다.
    CreateLocalbindPose(outData);
    
    outData.OrigineVertices          = outData.Vertices;
    outData.OrigineReferencePose     = outData.ReferencePose;

    // 머터리얼 정보를 추출합니다. 
    ExtractMaterial(outData, mesh, polyCount);

    outData.ComputeBounds();
}

void FFBXManager::CreateLocalbindPose(FSkeletalMeshRenderData& outData)
{
    int32 BoneCount = outData.ReferencePose.Num();
    outData.LocalBindPose.SetNum(BoneCount);
    for(int32 i = 0; i < BoneCount; ++i)
    {
        int32 P = outData.ParentBoneIndices[i];
        const FMatrix& Gc = outData.ReferencePose[i];      // 자식 글로벌
        if(P >= 0)
        {
            const FMatrix& Gp = outData.ReferencePose[P];  // 부모 글로벌

            // 1) 부모 회전, 위치 분리
            FQuat Rp = FQuat(Gp);                  // 부모 글로벌 회전
            FVector Tp(Gp.M[3][0], Gp.M[3][1], Gp.M[3][2]);

            // 2) 자식 회전, 위치 분리
            FQuat Rc = FQuat(Gc);
            FVector Tc(Gc.M[3][0], Gc.M[3][1], Gc.M[3][2]);

            // 3) 로컬 회전 = 부모 회전⁻¹ * 자식 회전
            FQuat Rlocal = Rp.Inverse() * Rc;
            Rlocal = Rlocal.GetSafeNormal();
            // 4) 로컬 이동 = 부모 회전⁻¹ * (자식 위치 - 부모 위치)
            FVector Tlocal = Rp.Inverse().RotateVector(Tc - Tp);

            // 5) LocalBindPose[i] 재조합
            FMatrix RotM = Rlocal.ToMatrix();
            RotM.M[3][0] = Tlocal.X;
            RotM.M[3][1] = Tlocal.Y;
            RotM.M[3][2] = Tlocal.Z;
            RotM.M[3][3] = 1;
            outData.LocalBindPose[i] = RotM;
        }
        else
        {
            // 루트 본은 글로벌이 그대로 로컬
            outData.LocalBindPose[i] = outData.ReferencePose[i];
        }
    }
}

void FFBXManager::ExtractMaterial(FSkeletalMeshRenderData& outData, FbxMesh* mesh, int polyCount)
{
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
}

void FFBXManager::ExtractBoneInfo(FSkeletalMeshRenderData& outData, FbxMesh* mesh)
{
    TSet<FbxNode*>            BoneNodeSet;
    TMap<FbxNode*, FMatrix>   ClusterBindPose;
    TMap<FbxNode*, TArray<std::pair<int, float>>>  BoneWeightsMap; 
    for (int d = 0; d < mesh->GetDeformerCount(FbxDeformer::eSkin); ++d)
    {
        auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
        for (int c = 0; c < skin->GetClusterCount(); ++c)
        {
            auto* cluster = skin->GetCluster(c);
            auto* boneNode = cluster->GetLink();
            if (!boneNode || !boneNode->GetSkeleton()) continue;

            BoneNodeSet.Add(boneNode);

            // 수정 후 (Skinning Matrix용 TransformMatrix 고려)
            FbxAMatrix meshGlobal;
            cluster->GetTransformMatrix(meshGlobal);
            FMatrix invMeshGlobal = FMatrix::Inverse(ConvertToFMatrix(meshGlobal));
            FbxAMatrix a;
            a = a.Transpose();
            FbxAMatrix linkGlobal;
            cluster->GetTransformLinkMatrix(linkGlobal);
            FMatrix boneGlobal = ConvertToFMatrix(linkGlobal);

            ClusterBindPose[boneNode] = boneGlobal;
            
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
        // FMatrix ConvMatInv = FMatrix::Inverse(conversionMatrix);
        outData.ReferencePose.Add( ClusterBindPose[boneNode]);
    }

    // 8) Vertex 쪽에 BoneIndices/BoneWeights 채우기
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
}

void FFBXManager::ExtractVertexInfo(FSkeletalMeshRenderData& outData, FbxMesh* mesh, int cpCount, int polyCount)
{
    outData.Indices.Reset();
    outData.Indices.Reserve(polyCount * 3);

    // 4) 클러스터 정보 미리 수집 (본 노드, TransformLinkMatrix, 가중치)

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

                    outData.Vertices[cpIndex].Normal = sourceNormal;

                    // 참고: UV와 마찬가지로 덮어쓰기 문제 가능성 있음
                }
            }
            // TODO: --- 탄젠트 추출 (필요하고 FBX에 데이터가 있다면) ---
            // if (hasTangents) { ... }
            outData.Indices.Add(mesh->GetPolygonVertex(p, k));
        }
    }
}

void FFBXManager::ExtractVertexPosition(FSkeletalMeshRenderData& outData, FbxMesh* mesh, int cpCount)
{
    outData.Vertices.SetNum(cpCount);
    FbxVector4* cps = mesh->GetControlPoints();
    for (int i = 0; i < cpCount; ++i)
    {
        auto& v = outData.Vertices[i];
        FVector sourcePosition = FVector(cps[i][0], cps[i][1], cps[i][2]);
        v.Position = sourcePosition; // 변환 행렬을 사용하여 변환
        for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
            v.BoneIndices[j] = v.BoneWeights[j] = 0;
    }
}



FMatrix FFBXManager::ConvertToFMatrix(const FbxAMatrix& in)
{
    // FBX 행렬 요소를 직접 Unreal FMatrix에 할당
    FMatrix out;
    out.M[0][0] = in.Get(0,0); out.M[0][1] = in.Get(0,1); out.M[0][2] = in.Get(0,2); out.M[0][3] = in.Get(0,3);
    out.M[1][0] = in.Get(1,0); out.M[1][1] = in.Get(1,1); out.M[1][2] = in.Get(1,2); out.M[1][3] = in.Get(1,3);
    out.M[2][0] = in.Get(2,0); out.M[2][1] = in.Get(2,1); out.M[2][2] = in.Get(2,2); out.M[2][3] = in.Get(2,3);
    out.M[3][0] = in.Get(3,0) * static_cast<float>(finalScaleFactor); out.M[3][1] = in.Get(3,1)* static_cast<float>(finalScaleFactor); out.M[3][2] = in.Get(3,2)* static_cast<float>(finalScaleFactor); out.M[3][3] = in.Get(3,3);
    return out;
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

    // 로컬 바인드 포즈
    uint32 LocalBindPoseCount = 0;
    File.read(reinterpret_cast<char*>(&LocalBindPoseCount), sizeof(LocalBindPoseCount));
    OutStaticMesh.LocalBindPose.SetNum(LocalBindPoseCount);
    File.read(reinterpret_cast<char*>(OutStaticMesh.LocalBindPose.GetData()), LocalBindPoseCount * sizeof(FMatrix));

    OutStaticMesh.ComputeBounds();

    File.close();
    return true;
}

bool FFBXManager::LoadAllMeshesFromFbx(const FString& FbxFilePath, TArray<USkeletalMesh*>& OutSkeletalMeshes)
{
    OutSkeletalMeshes.Empty();

    if (!std::filesystem::exists(FbxFilePath.ToWideString()))
    {
        UE_LOG(LogLevel::Error, TEXT("FFBXManager: FBX File Not Found: %s"), *FbxFilePath);
        return false;
    }

    // FBX 파일 열기 및 씬 임포트
    if (!Importer->Initialize(*FbxFilePath, -1, SdkManager->GetIOSettings()))
    {
        UE_LOG(LogLevel::Error, TEXT("FFBXManager: Cannot Initialize Importer for Fbx File Path : %s"), *FbxFilePath);
        return false;
    }

    Scene = FbxScene::Create(SdkManager, "TempSceneForAllMeshes"); // 임시 씬 이름
    if (!Importer->Import(Scene))
    {
        UE_LOG(LogLevel::Error, TEXT("FFBXManager: Cannot Import FBX Scene Info : %s"), *FbxFilePath);
        Importer->Destroy(); // 실패 시 Importer 정리
        Importer = FbxImporter::Create(SdkManager, ""); // 다음 사용을 위해 재생성
        return false;
    }

    // 씬 좌표계 변환 및 삼각화
    const FbxAxisSystem EngineAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
    EngineAxisSystem.DeepConvertScene(Scene);

    FbxGeometryConverter geomConverter(SdkManager);
    if (!geomConverter.Triangulate(Scene, true))
    {
        UE_LOG(LogLevel::Warning, TEXT("FFBXManager: Triangulation failed for scene: %s"), *FbxFilePath);
        // 실패해도 계속 진행할 수 있지만, 결과가 예상과 다를 수 있음
    }

    // 루트 노드부터 시작하여 모든 메시 노드를 재귀적으로 처리
    FbxNode* RootNode = Scene->GetRootNode();
    if (RootNode)
    {
        ProcessNodeRecursiveForMeshes(RootNode, FbxFilePath, OutSkeletalMeshes);
    }

    // 사용한 Scene 객체 정리
    if (Scene)
    {
        Scene->Destroy();
        Scene = nullptr;
    }
    // Importer는 Release()에서 정리되거나, 각 Load 호출 후 정리하는 정책에 따름
    // 여기서는 각 호출마다 Initialize/Destroy 쌍을 사용한다고 가정하고 Importer를 정리할 수 있습니다.
    // Importer->Destroy();
    // Importer = FbxImporter::Create(SdkManager, "");


    return !OutSkeletalMeshes.IsEmpty();
}

USkeletalMesh* FFBXManager::GetSkeletalMesh(const FString& FbxFilePath)
{
    if (SkeletalMeshMap.Contains(FbxFilePath))
    {
        return SkeletalMeshMap[FbxFilePath];
    }
    LoadFbx(FbxFilePath);
}

USkeletalMesh* FFBXManager::CreateSkeletalMeshFromNode(FbxNode* InNode, const FString& InFbxFilePath)
{
    if (!InNode || !InNode->GetMesh())
    {
        return nullptr;
    }

    // 각 메시마다 고유한 이름을 생성 (예: 파일경로_노드이름)
    // 이는 SkeletalMeshMap 캐싱 및 바이너리 파일 이름 생성에 사용될 수 있습니다.
    // 여기서는 단순화를 위해 캐싱 및 바이너리 로드/저장은 생략하고 항상 FBX에서 직접 로드합니다.
    // 필요하다면, 고유 식별자를 만들어 기존 캐싱/바이너리 시스템을 활용할 수 있습니다.
    FString MeshNodeName = FString(InNode->GetName());
    if (MeshNodeName.IsEmpty())
    {
        static int unidentifiedMeshCounter = 0;
        MeshNodeName = FString::Printf(TEXT("UnnamedMesh_%d"), unidentifiedMeshCounter++);
    }

    // 1. RenderData 생성
    FSkeletalMeshRenderData* RenderData = new FSkeletalMeshRenderData(); // 항상 새로 생성
    RenderData->FilePath = InFbxFilePath; // 원본 파일 경로 저장

    // 2. 현재 노드로부터 SkeletalMeshData 추출
    // ExtractSkeletalMeshData는 내부적으로 outData를 채웁니다.
    ExtractSkeletalMeshData(InNode, *RenderData); // InNode의 데이터를 RenderData에 채움

    // 추출된 데이터가 유효하지 않으면 (예: 메시 데이터가 없거나, 본 정보가 없는 스태틱 메시 등) 정리하고 nullptr 반환
    if (RenderData->Vertices.IsEmpty() && RenderData->BoneNames.IsEmpty()) // 간단한 유효성 검사
    {
        delete RenderData;
        UE_LOG(LogLevel::Warning, TEXT("FFBXManager: No valid mesh data extracted from node: %s in file: %s"), *MeshNodeName, *InFbxFilePath);
        return nullptr;
    }

    // (선택적) 바이너리 저장 로직 (각 메시에 대해 개별 바이너리 파일 저장)
    // FString BinaryMeshPath = UniqueMeshIdentifier; // 또는 다른 고유 경로
    // SaveSkeletalMeshToBinary(BinaryMeshPath, *RenderData);


    // 3. SkeletalMesh 객체 생성
    USkeletalMesh* NewSkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(&UAssetManager::Get());
    NewSkeletalMesh->SetRenderData(RenderData);
    // NewSkeletalMesh->SetPathName(UniqueMeshIdentifier); // 에셋 경로 설정 (필요시)

    // 4. 머티리얼 정보 설정
    for (const auto& MaterialInfo : RenderData->Materials)
    {
        UMaterial* Material = UMaterial::CreateMaterial(MaterialInfo); // UMaterial::CreateMaterial이 FObjMaterialInfo를 받는다고 가정
        NewSkeletalMesh->AddMaterial(Material);
    }

    // 5. 버퍼 생성
    RenderData->CreateBuffers(); // GPU 버퍼 생성

    // (선택적) SkeletalMeshMap에 캐싱 (고유 식별자 사용)

    return NewSkeletalMesh;
}

void FFBXManager::LoadFbxScene(const FString& FbxFilePath, FBX::FImportSceneData& OutSceneData)
{
}

void FFBXManager::ProcessNodeRecursiveForMeshes(FbxNode* InNode, const FString& InFbxFilePath, TArray<USkeletalMesh*>& OutSkeletalMeshes)
{
    if (!InNode)
    {
        return;
    }

    // 현재 노드가 메시 속성을 가지고 있는지 확인
    if (InNode->GetMesh())
    {
        USkeletalMesh* NewMesh = CreateSkeletalMeshFromNode(InNode, InFbxFilePath);
        if (NewMesh)
        {
            OutSkeletalMeshes.Add(NewMesh);
        }
    }

    // 자식 노드들에 대해 재귀적으로 동일 작업 수행
    for (int i = 0; i < InNode->GetChildCount(); ++i)
    {
        ProcessNodeRecursiveForMeshes(InNode->GetChild(i), InFbxFilePath, OutSkeletalMeshes);
    }
}


