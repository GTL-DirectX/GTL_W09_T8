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

    USkeletalMesh* LoadFbx(const FString& FbxFilePath);

    // SDK 초기화
    void Initialize();
    void Release();
    
    // FBX 파일 로드 및 씬 셋업 (파일 경로: const char*)
    void LoadSkeletalMeshRenderData(const FString& FbxFilePath, FSkeletalMeshRenderData& OutRenderData);
    void ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData);

    void CreateLocalbindPose(FSkeletalMeshRenderData& outData);
    void ExtractMaterial(FSkeletalMeshRenderData& outData, FbxMesh* mesh, int polyCount);
    void ExtractBoneInfo(FSkeletalMeshRenderData& outData, FbxMesh* mesh);
    void ExtractVertexInfo(FSkeletalMeshRenderData& outData, FbxMesh* mesh, int cpCount, int polyCount);
    void ExtractVertexPosition(FSkeletalMeshRenderData& outData, FbxMesh* mesh, int cpCount);
    // 씬에서 노드 데이터 출력 (예시)
    FMatrix ConvertToFMatrix(const FbxAMatrix& in);

public:
    bool SaveSkeletalMeshToBinary(const FString& FilePath, const FSkeletalMeshRenderData& StaticMesh);
    bool LoadSkeletalMeshFromBinary(const FString& FilePath, FSkeletalMeshRenderData& OutStaticMesh);



private:

public:
    void LoadFbxScene(const FString& FbxFilePath, FBX::FImportSceneData& OutSceneData);
    

private:
    // 생성/소멸은 외부 호출을 막기 위해 private
    FFBXManager() = default;
    FFBXManager(const FFBXManager&) = delete;
    FFBXManager& operator=(const FFBXManager&) = delete;
    double finalScaleFactor = 1.0;
    // FBX SDK 주요 객체
    FbxManager*     SdkManager      = nullptr;
    FbxIOSettings*  IOSettings      = nullptr;
    FbxImporter*    Importer        = nullptr;
    FbxScene*       Scene           = nullptr;
};
