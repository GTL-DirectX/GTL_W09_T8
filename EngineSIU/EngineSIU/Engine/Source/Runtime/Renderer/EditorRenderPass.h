#pragma once
#define _TCHAR_DEFINED
#include <d3d11.h>

#include "IRenderPass.h"
#include "RenderResources.h"
#include "D3D11RHI/DXDBufferManager.h"

class FDXDBufferManager;
class UWorld;
class FDXDShaderManager;

class FEditorRenderPass : public IRenderPass
{
public:
    void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    void Render(const std::shared_ptr<FViewportClient>& Viewport) override;
    void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) override;
    void ClearRenderArr() override;

private:
    FDXDBufferManager* BufferManager = nullptr;
    FGraphicsDevice* Graphics = nullptr;
    FDXDShaderManager* ShaderManager = nullptr;
    
    FRenderResourcesDebug Resources = FRenderResourcesDebug();

private:
    void CreateShaders();
    void CreateBuffers();
    void CreateConstantBuffers();
    
    void LazyLoad();

    void BindRenderTarget(const std::shared_ptr<FViewportClient>& Viewport) const;
    void BindShaderResource(const std::wstring& VertexKey, const std::wstring& PixelKey, D3D_PRIMITIVE_TOPOLOGY Topology) const;
    void BindBuffers(const FDebugPrimitiveData& InPrimitiveData) const;
    
    void RenderPointlightInstanced(uint64 ShowFlag);
    void RenderSpotlightInstanced(uint64 ShowFlag);
    void RenderArrowInstanced();
    void RenderBoxInstanced(uint64 ShowFlag);
    void RenderSphereInstanced(uint64 ShowFlag);
    void RenderCapsuleInstanced(uint64 ShowFlag);

    // Grid
    // void RenderGrid(std::shared_ptr<FEditorViewportClient> ActiveViewport);

    // Icon
    void RenderIcons(const UWorld* World, std::shared_ptr<FEditorViewportClient> ActiveViewport);   // 사용 X
    void UpdateTextureIcon(IconType type);

    static constexpr UINT32 ConstantBufferSizeBox = 8;
    static constexpr UINT32 ConstantBufferSizeSphere = 8;
    static constexpr UINT32 ConstantBufferSizeCone = 100;
    static constexpr UINT32 ConstantBufferSizeArrow = 100;
    static constexpr UINT32 ConstantBufferSizeCapsule = 8;
};

