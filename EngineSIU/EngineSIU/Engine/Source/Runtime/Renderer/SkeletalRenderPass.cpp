#include "SkeletalRenderPass.h"

#include <Rendering/Mesh/SkeletalMesh.h>

#include "Define.h"
#include "ShadowManager.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkeletalMeshComponent.h"
#include "ViewportClient.h"
#include "Engine/EditorEngine.h"
#include "ShowFlag.h"
#include "Rendering/Mesh/SkeletalMeshRenderData.h"
#include "Editor/LevelEditor/SLevelEditor.h"
#include "Editor/UnrealEd/EditorViewportClient.h"

FSkeletalRenderPass::FSkeletalRenderPass()
    : VertexShader(nullptr)
    , PixelShader(nullptr)
    , InputLayout(nullptr)
    , BufferManager(nullptr)
    , Graphics(nullptr)
    , ShaderManager(nullptr)
{
}

FSkeletalRenderPass::~FSkeletalRenderPass()
{
    ReleaseShader();
}

void FSkeletalRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManager;
    
    CreateShader();
}

void FSkeletalRenderPass::InitializeShadowManager(class FShadowManager* InShadowManager)
{
    ShadowManager = InShadowManager;
}

void FSkeletalRenderPass::PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport)
{
    for (const auto iter : TObjectRange<USkeletalMeshComponent>())
    {
        if (iter->GetWorld() == Viewport->GetWorld())
            SkeletalMeshComponents.Add(iter);
    }
}

void FSkeletalRenderPass::RenderAllSkeletalMeshes(const std::shared_ptr<FViewportClient>& Viewport)
{
    for (USkeletalMeshComponent* Comp : SkeletalMeshComponents)
    {
        if (!Comp || Comp->GetSkeletalMesh() == nullptr)
            continue;

        FSkeletalMeshRenderData* RenderData = Comp->GetSkeletalMesh()->GetRenderData();
        
        if (RenderData == nullptr)
        {
            continue;
        }

        FMatrix WorldMatrix = Comp->GetWorldMatrix();
        FVector4 UUIDColor = Comp->EncodeUUID() / 255.0f;

        UpdateObjectConstant(WorldMatrix, UUIDColor, false);
        TArray<FStaticMaterial*> Materials = Comp->GetSkeletalMesh()->GetMaterials();
        for (int MaterialIndex = 0; MaterialIndex < Materials.Num(); MaterialIndex++)
        {
            MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->Material->GetMaterialInfo());
        }

        RenderPrimitive(RenderData->VertexBuffer, RenderData->Vertices.Num(), RenderData->IndexBuffer, RenderData->Indices.Num());

        //UWorld* ThisWorld = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetWorld();
        //if (ThisWorld->WorldType != EWorldType::EditorPreview) return;

        if (Viewport->GetShowFlag() & static_cast<uint64>(EEngineShowFlags::SF_Bone))
        {
            const float BoneSphereRadius = 10.0f;
            const FVector4 BoneDebugColor = FVector4(0.8f, 0.8f, 0.0f, 1.0f);

            const int32 BoneCount = RenderData->BoneNames.Num();

            for (int32 BoneIdx = 0; BoneIdx < BoneCount; ++BoneIdx)
            {
                const FMatrix& BoneMeshSpaceTransform = RenderData->ReferencePose[BoneIdx];
                FVector BoneJointPos_LocalSpace = BoneMeshSpaceTransform.GetTranslationVector(); 
                FVector BoneJointPos_WorldSpace = WorldMatrix.TransformPosition(BoneJointPos_LocalSpace);

                FEngineLoop::PrimitiveDrawBatch.AddJointSphereToBatch(
                    BoneJointPos_WorldSpace,
                    BoneSphereRadius,
                    BoneDebugColor,
                    WorldMatrix
                );
            }
        }
    }
}

void FSkeletalRenderPass::Render(const std::shared_ptr<FViewportClient>& Viewport)
{
    ShadowManager->BindResourcesForSampling();

    PrepareRenderState(Viewport);

    RenderAllSkeletalMeshes(Viewport);
    // RenderAllSkeletalMeshes(Viewport);
    // 렌더 타겟 해제
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    Graphics->DeviceContext->PSSetShaderResources(static_cast<int>(EShaderSRVSlot::SRV_PointLight), 1, &nullSRV); // t51 슬롯을 NULL로 설정
    Graphics->DeviceContext->PSSetShaderResources(static_cast<int>(EShaderSRVSlot::SRV_DirectionalLight), 1, &nullSRV); // t51 슬롯을 NULL로 설정
    Graphics->DeviceContext->PSSetShaderResources(static_cast<int>(EShaderSRVSlot::SRV_SpotLight), 1, &nullSRV); // t51 슬롯을 NULL로 설정

    
    // @todo 리소스 언바인딩 필요한가?
    // SRV 해제
    ID3D11ShaderResourceView* NullSRVs[14] = { nullptr };
    Graphics->DeviceContext->PSSetShaderResources(0, 14, NullSRVs);

    // 상수버퍼 해제
    ID3D11Buffer* NullPSBuffer[9] = { nullptr };
    Graphics->DeviceContext->PSSetConstantBuffers(0, 9, NullPSBuffer);
    ID3D11Buffer* NullVSBuffer[2] = { nullptr };
    Graphics->DeviceContext->VSSetConstantBuffers(0, 2, NullVSBuffer);
}

void FSkeletalRenderPass::ClearRenderArr()
{
    SkeletalMeshComponents.Empty();
}

void FSkeletalRenderPass::ChangeViewMode(EViewModeIndex view_mode)
{
    // int ViewModeIndex;
    // switch (ViewModeIndex)
    // {
    // case EViewModeIndex::VMI_Lit_Gouraud:
    //     VertexShader = ShaderManager->GetVertexShaderByKey(L"GOURAUD_StaticMeshVertexShader");
    //     InputLayout = ShaderManager->GetInputLayoutByKey(L"GOURAUD_StaticMeshVertexShader");
    //     PixelShader = ShaderManager->GetPixelShaderByKey(L"GOURAUD_StaticMeshPixelShader");
    //     UpdateLitUnlitConstant(1);
    //     break;
    // case EViewModeIndex::VMI_Lit_Lambert:
    //     VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    //     InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    //     PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
    //     UpdateLitUnlitConstant(1);
    //     break;
    // case EViewModeIndex::VMI_Lit_BlinnPhong:
    //     VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    //     InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    //     PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
    //     UpdateLitUnlitConstant(1);
    //     break;
    // case EViewModeIndex::VMI_Wireframe:
    // case EViewModeIndex::VMI_Unlit:
    //     VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    //     InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    //     PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
    //     UpdateLitUnlitConstant(0);
    //     break;
    //     // HeatMap ViewMode 등
    // default:
    //     VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    //     InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
    //     PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
    //     UpdateLitUnlitConstant(1);
    //     break;
    // }
}

void FSkeletalRenderPass::PrepareRenderState(const std::shared_ptr<FViewportClient>& Viewport)
{
    const EViewModeIndex ViewMode = Viewport->GetViewMode();

    ChangeViewMode(ViewMode);

    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    TArray<FString> PSBufferKeys = {
        TEXT("FLightInfoBuffer"),
        TEXT("FMaterialConstants"),
        TEXT("FLitUnlitConstants"),
        TEXT("FSubMeshConstants"),
        TEXT("FTextureConstants")
    };

    BufferManager->BindConstantBuffers(PSBufferKeys, 0, EShaderStage::Pixel);

    BufferManager->BindConstantBuffer(TEXT("FLightInfoBuffer"), 0, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer(TEXT("FMaterialConstants"), 1, EShaderStage::Vertex);
    BufferManager->BindConstantBuffer(TEXT("FObjectConstantBuffer"), 12, EShaderStage::Vertex);
    

    Graphics->DeviceContext->RSSetViewports(1, &Viewport->GetViewportResource()->GetD3DViewport());

    const EResourceType ResourceType = EResourceType::ERT_Scene;
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    FRenderTargetRHI* RenderTargetRHI = ViewportResource->GetRenderTarget(ResourceType);
    FDepthStencilRHI* DepthStencilRHI = ViewportResource->GetDepthStencil(ResourceType);

    Graphics->DeviceContext->OMSetRenderTargets(1, &RenderTargetRHI->RTV, DepthStencilRHI->DSV);


    // Rasterizer
    if (ViewMode == EViewModeIndex::VMI_Wireframe)
    {
        Graphics->DeviceContext->RSSetState(Graphics->RasterizerWireframeBack);
    }
    else
    {
        Graphics->DeviceContext->RSSetState(Graphics->RasterizerSolidBack);
    }

    //// Pixel Shader -> StaticMeshRender 패스의 쉐이더 그대로 받아서 사용한다
    //if (ViewMode == EViewModeIndex::VMI_SceneDepth)
    //{
    //    Graphics->DeviceContext->PSSetShader(DebugDepthShader, nullptr, 0);
    //}
    //else if (ViewMode == EViewModeIndex::VMI_WorldNormal)
    //{
    //    Graphics->DeviceContext->PSSetShader(DebugWorldNormalShader, nullptr, 0);
    //}
    //else
    //{
    //    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
    //}
}


void FSkeletalRenderPass::UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const
{
    FObjectConstantBuffer ObjectData = {};
    ObjectData.WorldMatrix = WorldMatrix;
    ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
    ObjectData.UUIDColor = UUIDColor;
    ObjectData.bIsSelected = bIsSelected;
    
    BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);
}
void FSkeletalRenderPass::UpdateLitUnlitConstant(int32 isLit) const
{
    FLitUnlitConstants Data;
    Data.bIsLit = isLit;
    BufferManager->UpdateConstantBuffer(TEXT("FLitUnlitConstants"), Data);
}

void FSkeletalRenderPass::RenderPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices) const
{
    UINT Stride = sizeof(FStaticMeshVertex);
    UINT Offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &Stride, &Offset);
    Graphics->DeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    Graphics->DeviceContext->DrawIndexed(numIndices, 0, 0);
}

void FSkeletalRenderPass::ReleaseShader()
{
}

void FSkeletalRenderPass::CreateShader()
{
    // !TODO : 나중에 SkeletalMesh 용 Shader 작성해서 사용
    //// Begin Debug Shaders
    //HRESULT hr = ShaderManager->AddPixelShader(L"StaticMeshPixelShaderDepth", L"Shaders/StaticMeshPixelShaderDepth.hlsl", "mainPS");
    //if (FAILED(hr))
    //{
    //    return;
    //}
    //hr = ShaderManager->AddPixelShader(L"StaticMeshPixelShaderWorldNormal", L"Shaders/StaticMeshPixelShaderWorldNormal.hlsl", "mainPS");
    //if (FAILED(hr))
    //{
    //    return;
    //}
    //// End Debug Shaders

    //#pragma region UberShader
    //D3D_SHADER_MACRO DefinesGouraud[] =
    //{
    //    { GOURAUD, "1" },
    //    { nullptr, nullptr }
    //};
    //hr = ShaderManager->AddPixelShader(L"GOURAUD_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesGouraud);
    //if (FAILED(hr))
    //{
    //    return;
    //}
    //    
    //D3D_SHADER_MACRO DefinesLambert[] =
    //{
    //    { LAMBERT, "1" },
    //    { nullptr, nullptr }
    //};
    //hr = ShaderManager->AddPixelShader(L"LAMBERT_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesLambert);
    //if (FAILED(hr))
    //{
    //    return;
    //}
    //    
    //D3D_SHADER_MACRO DefinesBlinnPhong[] =
    //{
    //    { PHONG, "1" },
    //    { nullptr, nullptr }
    //};
    //hr = ShaderManager->AddPixelShader(L"PHONG_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesBlinnPhong);
    //if (FAILED(hr))
    //{
    //    return;
    //}
        
    #pragma endregion UberShader
        
    VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        
    PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
    DebugDepthShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderDepth");
    DebugWorldNormalShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldNormal");
}
