#pragma once

#include "IRenderPass.h"
#include "Define.h"

#define MAX_POINTLIGHT_PER_TILE 256
#define MAX_SPOTLIGHT_PER_TILE 256

class FDXDShaderManager;
class UWorld;

class UPointLightComponent;
class USpotLightComponent;
class UDirectionalLightComponent;
class UAmbientLightComponent;

struct PointLightPerTile {
    uint32 NumLights;
    uint32 Indices[MAX_POINTLIGHT_PER_TILE];
    uint32 Padding[3];
};

struct SpotLightPerTile {
    uint32 NumLights;
    uint32 Indices[MAX_SPOTLIGHT_PER_TILE];
    uint32 Padding[3];
};

class FUpdateLightBufferPass : public IRenderPass
{
public:
    FUpdateLightBufferPass();
    virtual ~FUpdateLightBufferPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;
    virtual void PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport) override;
    virtual void Render(const std::shared_ptr<FViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    void UpdateLightBuffer() const;

    void SetPointLightData(const TArray<UPointLightComponent*>& InPointLights, TArray<TArray<uint32>> InPointLightPerTiles);
    void SetSpotLightData(const TArray<USpotLightComponent*>& InSpotLights, TArray<TArray<uint32>> InSpotLightPerTiles);
    void SetLightData(const TArray<UPointLightComponent*>& InPointLights, const TArray<USpotLightComponent*>& InSpotLights, ID3D11ShaderResourceView* InPointLightIndexBufferSRV, ID3D11ShaderResourceView* InSpotLightIndexBufferSRV);

    void SetTileConstantBuffer(ID3D11Buffer* InTileConstantBuffer);

    void CreatePointLightBuffer();
    void CreateSpotLightBuffer();

    void CreatePointLightPerTilesBuffer();
    void CreateSpotLightPerTilesBuffer();

    void UpdatePointLightBuffer();
    void UpdateSpotLightBuffer();

    void UpdatePointLightPerTilesBuffer();
    void UpdateSpotLightPerTilesBuffer();

private:
    TArray<USpotLightComponent*> SpotLights;
    TArray<UPointLightComponent*> PointLights;
    TArray<UDirectionalLightComponent*> DirectionalLights;
    TArray<UAmbientLightComponent*> AmbientLights;

    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

    TArray<TArray<uint32>> PointLightPerTiles;
    TArray<PointLightPerTile> GPointLightPerTiles;

    TArray<TArray<uint32>> SpotLightPerTiles;
    TArray<SpotLightPerTile> GSpotLightPerTiles;

    ID3D11Buffer* PointLightBuffer;
    ID3D11ShaderResourceView* PointLightSRV;

    ID3D11Buffer* SpotLightBuffer;
    ID3D11ShaderResourceView* SpotLightSRV;
    
    ID3D11Buffer* PointLightPerTilesBuffer;
    ID3D11ShaderResourceView* PointLightPerTilesSRV;

    ID3D11Buffer* SpotLightPerTilesBuffer;
    ID3D11ShaderResourceView* SpotLightPerTilesSRV;

    ID3D11ShaderResourceView* PointLightIndexBufferSRV;
    ID3D11ShaderResourceView* SpotLightIndexBufferSRV;

    ID3D11Buffer* TileConstantBuffer;

    const uint32 MAX_NUM_POINTLIGHTS = 50000;
    const uint32 MAX_NUM_SPOTLIGHTS = 50000;
    const uint32 MAX_TILE = 10000;
};
