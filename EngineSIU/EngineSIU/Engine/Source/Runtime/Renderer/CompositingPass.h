#pragma once
#include <memory>

#include "IRenderPass.h"
#include "D3D11RHI/DXDShaderManager.h"

class FCompositingPass : public IRenderPass
{
public:
    FCompositingPass();
    virtual ~FCompositingPass();
    
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;
    
    virtual void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) override;

    virtual void Render(const std::shared_ptr<FViewportClient>& Viewport) override;

    virtual void ClearRenderArr() override;
    
private:
    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

    ID3D11SamplerState* Sampler;

    ID3D11Buffer* ViewModeBuffer;
};
