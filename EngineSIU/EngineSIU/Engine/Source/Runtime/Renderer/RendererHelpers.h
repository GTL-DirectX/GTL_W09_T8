#pragma once
#include "Launch/EngineLoop.h"

enum class EShaderSRVSlot : int8
{
    SRV_SpotLight = 50,
    SRV_DirectionalLight = 51,
    SRV_PointLight = 52,
    SRV_SceneDepth = 99,
    SRV_Scene = 100,
    SRV_PostProcess = 101,
    SRV_EditorOverlay = 102,
    SRV_Fog = 103,
    SRV_Debug = 104,
    SRV_Fade = 105,
    SRV_Viewport = 120,

    SRV_MAX = 127,
};

namespace MaterialUtils
{
    inline void UpdateMaterial(FDXDBufferManager* BufferManager, FGraphicsDevice* Graphics, const FObjMaterialInfo& MaterialInfo)
    {
        FMaterialConstants Data;
        Data.DiffuseColor = MaterialInfo.Diffuse;
        Data.TransparencyScalar = MaterialInfo.TransparencyScalar;
        
        Data.SpecularColor = MaterialInfo.Specular;
        Data.SpecularScalar = MaterialInfo.SpecularScalar;
        
        Data.EmissiveColor = MaterialInfo.Emissive;
        Data.DensityScalar = MaterialInfo.DensityScalar;
        
        Data.AmbientColor = MaterialInfo.Ambient;
        Data.TextureFlag = MaterialInfo.TextureFlag;

        BufferManager->UpdateConstantBuffer(TEXT("FMaterialConstants"), Data);
        
        // Update Textures
        if (MaterialInfo.TextureFlag & (1 << 1)) {
            std::shared_ptr<FTexture> texture = FEngineLoop::ResourceManager.GetTexture(MaterialInfo.DiffuseTexturePath);
            if (texture)
            {
                Graphics->DeviceContext->PSSetShaderResources(0, 1, &texture->TextureSRV);
                Graphics->DeviceContext->PSSetSamplers(0, 1, &texture->SamplerState);
            }
        }
        else
        {
            ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
            ID3D11SamplerState* nullSampler[1] = { nullptr };
            Graphics->DeviceContext->PSSetShaderResources(0, 1, nullSRV);
            Graphics->DeviceContext->PSSetSamplers(0, 1, nullSampler);

        }
        
        if (MaterialInfo.TextureFlag & (1 << 2))
        {
            std::shared_ptr<FTexture> texture = FEngineLoop::ResourceManager.GetTexture(MaterialInfo.BumpTexturePath);
            if (texture)
            {
                Graphics->DeviceContext->PSSetShaderResources(1, 1, &texture->TextureSRV);
                Graphics->DeviceContext->PSSetSamplers(1, 1, &texture->SamplerState);
            }
        }
        else {
            ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
            ID3D11SamplerState* nullSampler[1] = { nullptr };
            Graphics->DeviceContext->PSSetShaderResources(1, 1, nullSRV);
            Graphics->DeviceContext->PSSetSamplers(1, 1, nullSampler);
        }
    }
}
