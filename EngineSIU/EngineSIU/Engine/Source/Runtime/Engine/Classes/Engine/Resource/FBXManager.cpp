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
            for (int i=0;i<OutRenderData.Vertices.Num();i++)
            {
                std::cout << "Vertex "<< i << " Pos : "<<  OutRenderData.Vertices[i].Position.X << " " << OutRenderData.Vertices[i].Position.Y << " " << OutRenderData.Vertices[i].Position.Z << std::endl;
                std::cout << "Vertex "<< i << " Normal : " << OutRenderData.Vertices[i].Normal.X << " " << OutRenderData.Vertices[i].Normal.Y << " " << OutRenderData.Vertices[i].Normal.Z << std::endl;
                std::cout << "Vertex "<< i << " BoneIndex : " <<OutRenderData.Vertices[i].BoneIndices[0] << " " << OutRenderData.Vertices[i].BoneIndices[1] << std::endl;
                std::cout << "Vertex "<< i << " Weight : " <<OutRenderData.Vertices[i].BoneWeights[0] << " " << OutRenderData.Vertices[i].BoneWeights[1] << std::endl;
            }
            for (int i=0;i<OutRenderData.BoneNames.Num();i++)
            {
                std::cout << GetData(OutRenderData.BoneNames[i]) << std::endl;
            }
            for (int i=0;i<OutRenderData.ParentBoneIndices.Num();i++)
            {
                std::cout << OutRenderData.ParentBoneIndices[i] << std::endl;
            }
            break;
        }
        
    }
}

void FFBXManager::PrintStaticMeshData(FbxNode* node)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh) return;

    std::cout << "=== Object Name: " << node->GetName() << " ===\n";
    std::cout << "Display Name: " << node->GetName() << "\n\n";

    // 정점 출력
    // 컨트롤 포인트
    int cpCount = mesh->GetControlPointsCount();
    FbxVector4* cps = mesh->GetControlPoints();
    auto uvLayer      = mesh->GetElementUV(0);
    auto normalLayer  = mesh->GetElementNormal(0);
    auto tangentLayer = mesh->GetElementTangent(0);
    auto binormalLayer= mesh->GetElementBinormal(0);

    // 바운딩
    FbxVector4 bbMin( FLT_MAX,  FLT_MAX,  FLT_MAX);
    FbxVector4 bbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    std::cout << "--- Vertices (" << cpCount << ") ---\n";
    for (int i = 0; i < cpCount; ++i) {
        auto& cp = cps[i];
        std::cout << "V[" << i << "] pos=(" << cp[0] << ", " << cp[1] << ", " << cp[2] << ") ";
        bbMin[0] = std::min(bbMin[0], cp[0]);  bbMax[0] = std::max(bbMax[0], cp[0]);
        bbMin[1] = std::min(bbMin[1], cp[1]);  bbMax[1] = std::max(bbMax[1], cp[1]);
        bbMin[2] = std::min(bbMin[2], cp[2]);  bbMax[2] = std::max(bbMax[2], cp[2]);

        if (uvLayer && uvLayer->GetMappingMode()==FbxGeometryElement::eByControlPoint) {
            int uvIndex = uvLayer->GetIndexArray().GetAt(i);
            auto  uv     = uvLayer->GetDirectArray().GetAt(uvIndex);
            std::cout << "UV=(" << uv[0] << ", " << uv[1] << ") ";
        }
        if (normalLayer && normalLayer->GetMappingMode()==FbxGeometryElement::eByControlPoint) {
            auto n = normalLayer->GetDirectArray().GetAt(i);
            std::cout << "N=(" << n[0] << ", " << n[1] << ", " << n[2] << ") ";
        }
        if (tangentLayer && tangentLayer->GetMappingMode()==FbxGeometryElement::eByControlPoint) {
            auto t = tangentLayer->GetDirectArray().GetAt(i);
            std::cout << "T=(" << t[0] << ", " << t[1] << ", " << t[2] << ") ";
        }
        if (binormalLayer && binormalLayer->GetMappingMode()==FbxGeometryElement::eByControlPoint) {
            auto b = binormalLayer->GetDirectArray().GetAt(i);
            std::cout << "B=(" << b[0] << ", " << b[1] << ", " << b[2] << ") ";
        }
        std::cout << "\n";
    }
    std::cout << "Bounding Box Min = (" << bbMin[0] << ", " << bbMin[1] << ", " << bbMin[2] << ")\n";
    std::cout << "Bounding Box Max = (" << bbMax[0] << ", " << bbMax[1] << ", " << bbMax[2] << ")\n\n";

    // 인덱스 출력
    int polyCount = mesh->GetPolygonCount();
    std::cout << "--- Indices (Polygons: " << polyCount << ", Triangles) ---\n";
    for (int p = 0; p < polyCount; ++p) {
        std::cout << "Tri[" << p << "]: ";
        std::cout << mesh->GetPolygonVertex(p, 0) << " ";
        std::cout << mesh->GetPolygonVertex(p, 1) << " ";
        std::cout << mesh->GetPolygonVertex(p, 2) << "\n";
    }
    std::cout << "\n";

    // 재질 및 서브셋 출력
    int matCount = node->GetMaterialCount();
    std::cout << "--- Materials (" << matCount << ") ---\n";
    for (int i = 0; i < matCount; ++i) {
        auto* m = node->GetMaterial(i);
        std::cout << "Material[" << i << "]: " << m->GetName() << "\n";
    }
    std::cout << "\n";

    auto matLayer = mesh->GetElementMaterial();
    if (matLayer) {
        std::cout << "--- Material Subsets ---\n";
        const auto& idxArray = matLayer->GetIndexArray();
        for (int p = 0; p < polyCount; ++p) {
            int mIdx = idxArray.GetAt(p);
            std::cout << "Poly[" << p << "] uses material " << mIdx << "\n";
        }
        std::cout << "\n";
    }

    // 스킨/본 가중치 출력
    int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
    std::cout << "--- Skinning (Skins: " << skinCount << ") ---\n";
    for (int d = 0; d < skinCount; ++d) {
        auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
        int clusCnt = skin->GetClusterCount();
        std::cout << "Skin Deformer[" << d << "] Clusters: " << clusCnt << "\n";
        for (int c = 0; c < clusCnt; ++c) {
            auto* cluster = skin->GetCluster(c);
            auto* link    = cluster->GetLink();  
            std::cout << "  Cluster[" << c << "] Bone: " 
                      << (link ? link->GetName() : "null") << "\n";
            const int*    cpIndices = cluster->GetControlPointIndices();
            int            cpCount  = cluster->GetControlPointIndicesCount();
            const double* weights   = cluster->GetControlPointWeights();
            for (int i = 0; i < cpCount; ++i) {
                std::cout << "    CP[" << cpIndices[i] << "] weight=" 
                          << weights[i] << "\n";
            }
        }
    }
    std::cout << "\n";
}

void FFBXManager::ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh) return;
    
    // 2) 이름
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
        // 초기화: 본 가중치
        for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
        {
            v.BoneIndices[j] = 0;
            v.BoneWeights[j] = 0.0f;
        }
    }
    
    // 4) 인덱스 삼각화(이미 Triangulate 호출됨 가정)
    int polyCount = mesh->GetPolygonCount();
    outData.Indices.Empty();
    outData.Indices.Reserve(polyCount * 3);
    for (int p = 0; p < polyCount; ++p)
    {
        for (int k = 0; k < 3; ++k)
        {
            outData.Indices.Add(mesh->GetPolygonVertex(p, k));
        }
    }
    
    // 5) 본 + 스킨 정보
    for (int d = 0; d < mesh->GetDeformerCount(FbxDeformer::eSkin); ++d)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
        for (int c = 0; c < skin->GetClusterCount(); ++c)
        {
            FbxCluster* cluster = skin->GetCluster(c);
            FbxNode* boneNode = cluster->GetLink();

            // 본 이름 추가
            int boneIndex = outData.BoneNames.Add(boneNode->GetName());

            // 부모 본 인덱스 찾기
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

            // Reference pose 저장
            FbxAMatrix tm;
            cluster->GetTransformLinkMatrix(tm);
            outData.ReferencePose.Add(ConvertToFMatrix(tm));

            // 가중치 채우기
            int count = cluster->GetControlPointIndicesCount();
            const int* cpsIdx = cluster->GetControlPointIndices();
            const double* weights = cluster->GetControlPointWeights();
            for (int i = 0; i < count; ++i)
            {
                int cpIndex = cpsIdx[i];
                float w = static_cast<float>(weights[i]);
                auto& vert = outData.Vertices[cpIndex];
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

    // 6) 재질 & 서브셋 처리 (생략 또는 사용자 구현)
    //    예: ParseMaterials(mesh, outData.Materials, outData.MaterialSubsets);
    
    // 7) 바운딩 박스 계산
    ComputeBounds(outData.Vertices, outData.BoundingBoxMin, outData.BoundingBoxMax);

    // 8) GPU 버퍼 생성: Skeletal -> Static vertex 포맷 변환 후 업로드
    {
        TArray<FStaticMeshVertex> StaticVerts;
        StaticVerts.SetNum(outData.Vertices.Num());

        for (int i = 0; i < outData.Vertices.Num(); ++i)
        {
            const FSkeletalMeshVertex& Src = outData.Vertices[i];
            FStaticMeshVertex& Dst = StaticVerts[i];

            // 위치
            Dst.X = Src.Position.X;
            Dst.Y = Src.Position.Y;
            Dst.Z = Src.Position.Z;

            // 컬러: 기본 흰색
            Dst.R = 1.0f;
            Dst.G = 1.0f;
            Dst.B = 1.0f;
            Dst.A = 1.0f;

            // 노멀
            Dst.NormalX = Src.Normal.X;
            Dst.NormalY = Src.Normal.Y;
            Dst.NormalZ = Src.Normal.Z;

            // 탄젠트
            Dst.TangentX = Src.Tangent.X;
            Dst.TangentY = Src.Tangent.Y;
            Dst.TangentZ = Src.Tangent.Z;

            // UV
            Dst.U = Src.UV.X;
            Dst.V = Src.UV.Y;

            // 재질 인덱스: 필요시 설정
            Dst.MaterialIndex = 0;
        }

        // 버퍼 생성 호출 (StaticVerts, Indices 사용)
        CreateBuffers(
            GEngineLoop.GraphicDevice.Device,
            StaticVerts,
            outData.Indices,
            outData.VertexBuffer,
            outData.IndexBuffer
        );
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

