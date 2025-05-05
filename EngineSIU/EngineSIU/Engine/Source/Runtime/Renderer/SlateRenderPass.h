#pragma once
#include "IRenderPass.h"

#include "Define.h"

struct FSlateTransform
{
    FVector2D Scale;
    FVector2D Offset;
};

class FSlateRenderPass : public IRenderPass
{
public:
    FSlateRenderPass();
    virtual ~FSlateRenderPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    
    virtual void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) override;

    virtual void Render(const std::shared_ptr<FViewportClient>& Viewport) override;

    virtual void ClearRenderArr() override;

    void CreateShader();
    void CreateBuffer();
    void CreateSampler();

private:
    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

    ID3D11SamplerState* Sampler;
};
