#pragma once

#include "Define.h"
#include "EngineBaseTypes.h"
#include "IRenderPass.h"
#include "D3D11RHI/DXDShaderManager.h"

struct FVector4;
struct FMatrix;
class FShadowManager;
class USkeletalMeshComponent;

class FSkeletalRenderPass : public IRenderPass
{
public:
    FSkeletalRenderPass();
    virtual ~FSkeletalRenderPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    void InitializeShadowManager(class FShadowManager* InShadowManager);
    virtual void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) override;
    virtual void Render(const std::shared_ptr<FViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    
    void ChangeViewMode(EViewModeIndex view_mode);
    virtual void PrepareRenderState(const std::shared_ptr<FViewportClient>& Viewport);

    void UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const;
    void UpdateLitUnlitConstant(int32 isLit) const;

    void RenderPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices) const;
    void RenderAllSkeletalMeshes(const std::shared_ptr<FViewportClient>& Viewport);
    
    void ReleaseShader();
    void CreateShader();

public:
#pragma region SkeletalMesh
    TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
#pragma endregion
    
    ID3D11VertexShader* VertexShader;
    ID3D11InputLayout* InputLayout;
    
    ID3D11PixelShader* PixelShader;
    ID3D11PixelShader* DebugDepthShader;
    ID3D11PixelShader* DebugWorldNormalShader;

    FGraphicsDevice* Graphics;
    FDXDBufferManager* BufferManager;
    FDXDShaderManager* ShaderManager;
    
    FShadowManager* ShadowManager;
};


