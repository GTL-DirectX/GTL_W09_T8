#pragma once
#include <memory>

class FViewportResource;
class UWorld;
class FDXDBufferManager;
class FGraphicsDevice;
class FDXDShaderManager;
class FViewportClient;

class IRenderPass {
public:
    virtual ~IRenderPass() {}
    
    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) = 0;

    virtual void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) = 0;
    
    virtual void Render(const std::shared_ptr<FViewportClient>& Viewport) = 0;

    virtual void ClearRenderArr() = 0;
};
