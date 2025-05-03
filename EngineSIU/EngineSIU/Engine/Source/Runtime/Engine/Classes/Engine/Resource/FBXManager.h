// FFBXManager.h
#pragma once
#include "Container/String.h"
#include <fbxsdk.h>

#include "Define.h"

// FBX SDK forward declarations to avoid include issues
class FFBXManager
{
public:
    // 전역 싱글톤 인스턴스 접근
    static FFBXManager& Get()
    {
        static FFBXManager Instance;
        return Instance;
    }

    // SDK 초기화
    void Initialize();

    // FBX 파일 로드 및 씬 셋업 (파일 경로: const char*)
    void LoadFbx(const FString& FbxFilePath);
    void Release();
    // 씬에서 노드 데이터 출력 (예시)
    void PrintStaticMeshData(FbxNode* node);
    void ExtractSkeletalMeshData(FbxNode* node, FSkeletalMeshRenderData& outData);
    FMatrix ConvertToFMatrix(const FbxAMatrix& in);
    void ComputeBounds(
        const TArray<FSkeletalMeshVertex>& Verts,
        FVector& OutMin,
        FVector& OutMax);
    void CreateBuffers(
        ID3D11Device* Device,
        const TArray<FSkeletalMeshVertex>& Verts,
        const TArray<UINT>& Indices,
        ID3D11Buffer*& OutVB,
        ID3D11Buffer*& OutIB);
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
