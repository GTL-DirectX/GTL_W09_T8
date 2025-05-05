// FFBXManager.h
#pragma once
#include "Container/String.h"

#include "Define.h"
#include "Container/Map.h"

#include "Rendering/Mesh/SkeletalMeshRenderData.h"
#include "Rendering/Mesh/SkeletalMesh.h"
#include "FBXData.h"

// FBX SDK forward declarations to avoid include issues
class FFBXManager
{
   
private:
    TMap<FString, USkeletalMesh*> SkeletalMeshMap; // SkeletalMesh 이름과 포인터를 저장하는 맵

public:
    // 전역 싱글톤 인스턴스 접근
    static FFBXManager& Get()
    {
        static FFBXManager Instance;
        return Instance;
    }

    USkeletalMesh* LoadSkeletalMesh(const FString& FbxFilePath);

    // SDK 초기화
    void Initialize();

    // FBX 파일 로드 및 씬 셋업 (파일 경로: const char*)
    void LoadFbx(const FString& FbxFilePath, FSkeletalMeshRenderData& OutRenderData);
    void Release();
    // 씬에서 노드 데이터 출력 (예시)
    void ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData);
    FMatrix ConvertToFMatrix(const FbxAMatrix& in);
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
    void UpdateAndSkinMesh(FSkeletalMeshRenderData& MeshData,
                                        ID3D11DeviceContext* Context);



private:
    void ProcessNodeRecursively(
        FbxNode* node,
        FBX::FImportSceneData& OutSceneData,
        int32 parentNodeIndex, // Index of the parent in OutSceneData.NodeHierarchy
        const FMatrix& parentWorldTransform, // Parent's world transform (Engine Space)
        const FMatrix& conversionMatrix,
        bool bFlipWinding
    );

    void ExtractMeshData(FbxNode* node, FbxMesh* mesh, FBX::FMeshData& outMeshData, const FMatrix& nodeWorldTransform, const FMatrix& conversionMatrix, bool bFlipWinding);
    void ExtractSkeletonData(FbxNode* node, FbxSkeleton* skeleton, FBX::FImportSceneData& OutSceneData, int32 nodeIndex); // Needs access to the main data struct
    void ExtractLightData(FbxNode* node, FbxLight* light, FBX::FLightData& outLightData, const FMatrix& nodeWorldTransform, const FMatrix& conversionMatrix);
    void ExtractCameraData(FbxNode* node, FbxCamera* camera, FBX::FCameraData& outCameraData, const FMatrix& nodeWorldTransform, const FMatrix& conversionMatrix);
    FMatrix GetNodeLocalTransformConverted(FbxNode* node, const FMatrix& conversionMatrix);
    FMatrix ConvertFbxMatrixToEngineMatrix(const FbxAMatrix& fbxMatrix); // Handles potential transpose

    FMatrix GetConversionMatrix(const FbxAxisSystem& sourceAxisSystem, const FbxAxisSystem& targetAxisSystem);

    void BuildBasisMatrix(const FbxAxisSystem& system, FbxAMatrix& outMatrix);

public:
    void LoadFbxScene(const FString& FbxFilePath, FBX::FImportSceneData& OutSceneData);
    

private:
    // 생성/소멸은 외부 호출을 막기 위해 private
    FFBXManager() = default;
    FFBXManager(const FFBXManager&) = delete;
    FFBXManager& operator=(const FFBXManager&) = delete;

    // FBX SDK 주요 객체
    FbxManager*     SdkManager      = nullptr;
    FbxIOSettings*  IOSettings      = nullptr;
    FbxImporter*    Importer        = nullptr;
    FbxScene*       Scene           = nullptr;
};
