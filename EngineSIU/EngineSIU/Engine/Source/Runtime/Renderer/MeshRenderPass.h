#pragma once

#include "IRenderPass.h"
#include "EngineBaseTypes.h"

#include "Define.h"

class USkeletalMeshComponent;
class FShadowManager;
class FDXDShaderManager;
class UWorld;
class UMaterial;
class UStaticMeshComponent;
struct FMaterialSlot;
class FShadowRenderPass;
struct FSkeletalMeshRenderData;

class FMeshRenderPass : public IRenderPass
{
    // IRenderPass을(를) 통해 상속됨
public:
    void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    void InitializeShadowManager(class FShadowManager* InShadowManager);

    void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) override;
    void Render(const std::shared_ptr<FViewportClient>& Viewport) override;
    void ClearRenderArr() override;

    virtual void PrepareRenderState(const std::shared_ptr<FViewportClient>& Viewport);


    // Shader 관련 함수 (생성/해제 등)
    void CreateShader();
    void ReleaseShader();
    void ChangeViewMode(EViewModeIndex ViewModeIndex);
    void UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const;
    void UpdateLitUnlitConstant(int32 isLit) const;

    void RenderAllStaticMeshes(const std::shared_ptr<FViewportClient>& Viewport);
    void RenderAllSkeletalMeshes(const std::shared_ptr<FViewportClient>& Viewport);

    void RenderStaticMesh(FStaticMeshRenderData* RenderData, TArray<FMaterialSlot*> Materials, TArray<UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const;
    void RenderSkeletalMesh(FSkeletalMeshRenderData* RenderData, TArray<FMaterialSlot*> Materials, TArray<UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const;

protected:
    TArray<UStaticMeshComponent*> StaticMeshComponents;
    TArray<USkeletalMeshComponent*> SkeletalMeshComponents;

    // StaticMesh
    ID3D11VertexShader* StaticMesh_VertexShader;
    ID3D11InputLayout* StaticMesh_InputLayout;

    ID3D11PixelShader* StaticMesh_PixelShader;
    ID3D11PixelShader* StaticMesh_DebugDepthShader;
    ID3D11PixelShader* StaticMesh_DebugWorldNormalShader;

    // SkeletalMesh
    ID3D11VertexShader* SkeletalMesh_VertexShader;
    ID3D11InputLayout* SkeletalMesh_InputLayout;

    ID3D11PixelShader* SkeletalMesh_PixelShader;
    ID3D11PixelShader* SkeletalMesh_DebugDepthShader;
    ID3D11PixelShader* SkeletalMesh_DebugWorldNormalShader;

    FGraphicsDevice* Graphics;
    FDXDBufferManager* BufferManager;
    FDXDShaderManager* ShaderManager;

    FShadowManager* ShadowManager;
};
