#include "MeshRenderPass.h"
#include "ShadowManager.h"
#include "Viewport.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Casts.h"

#include "Components/SkeletalMeshComponent.h"
#include "ViewportClient.h"
#include "Engine/EditorEngine.h"
#include "ShowFlag.h"
#include "Rendering/Mesh/SkeletalMeshRenderData.h"

#include "D3D11RHI/DXDBufferManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "D3D11RHI/DXDShaderManager.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Rendering/Mesh/SkeletalMesh.h"
#include "Rendering/Mesh/StaticMesh.h"
#include "BaseGizmos/GizmoBaseComponent.h"


void FMeshRenderPass::Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager)
{
    BufferManager = InBufferManager;
    Graphics = InGraphics;
    ShaderManager = InShaderManager;

    CreateShader();
}

void FMeshRenderPass::InitializeShadowManager(FShadowManager* InShadowManager)
{
    ShadowManager = InShadowManager;
}

void FMeshRenderPass::PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport)
{
    if (Viewport == nullptr || Viewport->GetWorld() == nullptr)
        return;

    for (const auto iter : TObjectRange<UStaticMeshComponent>())
    {
        if (!Cast<UGizmoBaseComponent>(iter) && iter->GetWorld() == Viewport->GetWorld())
        {
            StaticMeshComponents.Add(iter);
        }
    }

    for (const auto iter : TObjectRange<USkeletalMeshComponent>())
    {
        if (iter->GetWorld() == Viewport->GetWorld())
            SkeletalMeshComponents.Add(iter);
    }
}

void FMeshRenderPass::Render(const std::shared_ptr<FViewportClient>& Viewport)
{
    ShadowManager->BindResourcesForSampling();

    PrepareRenderState(Viewport);

    RenderAllStaticMeshes(Viewport);
    RenderAllSkeletalMeshes(Viewport);

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

void FMeshRenderPass::ClearRenderArr()
{
    StaticMeshComponents.Empty();
    SkeletalMeshComponents.Empty();
}

void FMeshRenderPass::CreateShader()
{
    // Begin Debug Shaders
    HRESULT hr = ShaderManager->AddPixelShader(L"StaticMeshPixelShaderDepth", L"Shaders/StaticMeshPixelShaderDepth.hlsl", "mainPS");
    if (FAILED(hr))
    {
        return;
    }
    hr = ShaderManager->AddPixelShader(L"StaticMeshPixelShaderWorldNormal", L"Shaders/StaticMeshPixelShaderWorldNormal.hlsl", "mainPS");
    if (FAILED(hr))
    {
        return;
    }
    // End Debug Shaders

#pragma region UberShader
    D3D_SHADER_MACRO DefinesGouraud[] =
    {
        { GOURAUD, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddPixelShader(L"GOURAUD_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesGouraud);
    if (FAILED(hr))
    {
        return;
    }

    D3D_SHADER_MACRO DefinesLambert[] =
    {
        { LAMBERT, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddPixelShader(L"LAMBERT_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesLambert);
    if (FAILED(hr))
    {
        return;
    }

    D3D_SHADER_MACRO DefinesBlinnPhong[] =
    {
        { PHONG, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddPixelShader(L"PHONG_StaticMeshPixelShader", L"Shaders/StaticMeshPixelShader.hlsl", "mainPS", DefinesBlinnPhong);
    if (FAILED(hr))
    {
        return;
    }

#pragma endregion UberShader

    StaticMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    StaticMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");

    StaticMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
    StaticMesh_DebugDepthShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderDepth");
    StaticMesh_DebugWorldNormalShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldNormal");

    // !TODO : 나중에 SkeletalMesh 용 Shader 작성해서 사용
    SkeletalMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
    SkeletalMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");

    SkeletalMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
    SkeletalMesh_DebugDepthShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderDepth");
    SkeletalMesh_DebugWorldNormalShader = ShaderManager->GetPixelShaderByKey(L"StaticMeshPixelShaderWorldNormal");
}

void FMeshRenderPass::ReleaseShader()
{
}

void FMeshRenderPass::ChangeViewMode(EViewModeIndex ViewModeIndex)
{
    switch (ViewModeIndex)
    {
    case EViewModeIndex::VMI_Lit_Gouraud:
        StaticMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"GOURAUD_StaticMeshVertexShader");
        StaticMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"GOURAUD_StaticMeshVertexShader");
        StaticMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"GOURAUD_StaticMeshPixelShader");
        SkeletalMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"GOURAUD_StaticMeshVertexShader");
        SkeletalMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"GOURAUD_StaticMeshVertexShader");
        SkeletalMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"GOURAUD_StaticMeshPixelShader");

        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_Lit_Lambert:
        StaticMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        StaticMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        StaticMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");
        SkeletalMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        SkeletalMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        SkeletalMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"LAMBERT_StaticMeshPixelShader");

        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_Lit_BlinnPhong:
        StaticMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        StaticMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        StaticMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
        SkeletalMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        SkeletalMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        SkeletalMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");

        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_Wireframe:
    case EViewModeIndex::VMI_Unlit:
        StaticMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        StaticMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        StaticMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
        SkeletalMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        SkeletalMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        SkeletalMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");

        UpdateLitUnlitConstant(0);
        break;
        // HeatMap ViewMode 등
    default:
        StaticMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        StaticMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        StaticMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");
        SkeletalMesh_VertexShader = ShaderManager->GetVertexShaderByKey(L"StaticMeshVertexShader");
        SkeletalMesh_InputLayout = ShaderManager->GetInputLayoutByKey(L"StaticMeshVertexShader");
        SkeletalMesh_PixelShader = ShaderManager->GetPixelShaderByKey(L"PHONG_StaticMeshPixelShader");

        UpdateLitUnlitConstant(1);
        break;
    }
}

void FMeshRenderPass::UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const
{
    FObjectConstantBuffer ObjectData = {};
    ObjectData.WorldMatrix = WorldMatrix;
    ObjectData.InverseTransposedWorld = FMatrix::Transpose(FMatrix::Inverse(WorldMatrix));
    ObjectData.UUIDColor = UUIDColor;
    ObjectData.bIsSelected = bIsSelected;

    BufferManager->UpdateConstantBuffer(TEXT("FObjectConstantBuffer"), ObjectData);
}

void FMeshRenderPass::UpdateLitUnlitConstant(int32 isLit) const
{
    FLitUnlitConstants Data;
    Data.bIsLit = isLit;
    BufferManager->UpdateConstantBuffer(TEXT("FLitUnlitConstants"), Data);
}

void FMeshRenderPass::RenderAllStaticMeshes(const std::shared_ptr<FViewportClient>& Viewport)
{
    for (UStaticMeshComponent* Comp : StaticMeshComponents)
    {
        if (!Comp || !Comp->GetStaticMesh())
        {
            continue;
        }

        FStaticMeshRenderData* RenderData = Comp->GetStaticMesh()->GetRenderData();
        if (RenderData == nullptr)
        {
            continue;
        }

        USceneComponent* TargetComponent = nullptr;
        UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
        if (Engine)
        {
            USceneComponent* SelectedComponent = Engine->GetSelectedComponent();
            AActor* SelectedActor = Engine->GetSelectedActor();

            if (SelectedComponent != nullptr)
            {
                TargetComponent = SelectedComponent;
            }
            else if (SelectedActor != nullptr)
            {
                TargetComponent = SelectedActor->GetRootComponent();
            }

        }
        const bool bIsSelected = (Engine && TargetComponent == Comp);


        FMatrix WorldMatrix = Comp->GetWorldMatrix();
        FVector4 UUIDColor = Comp->EncodeUUID() / 255.0f;

        UpdateObjectConstant(WorldMatrix, UUIDColor, bIsSelected);

        RenderStaticMesh(RenderData, Comp->GetStaticMesh()->GetMaterials(), Comp->GetOverrideMaterials(), Comp->GetselectedSubMeshIndex());

        if (Viewport->GetShowFlag() & static_cast<uint64>(EEngineShowFlags::SF_AABB))
        {
            FEngineLoop::PrimitiveDrawBatch.AddAABBToBatch(Comp->GetBoundingBox(), Comp->GetWorldLocation(), WorldMatrix);
        }
    }
}

void FMeshRenderPass::RenderAllSkeletalMeshes(const std::shared_ptr<FViewportClient>& Viewport)
{
    for (USkeletalMeshComponent* Comp : SkeletalMeshComponents)
    {
        if (!Comp || !Comp->GetSkeletalMesh())
        {
            continue;
        }

        FSkeletalMeshRenderData* RenderData = Comp->GetSkeletalMesh()->GetRenderData();
        if (RenderData == nullptr)
        {
            continue;
        }

        USceneComponent* TargetComponent = nullptr;
        UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
        if (Engine)
        {
            USceneComponent* SelectedComponent = Engine->GetSelectedComponent();
            AActor* SelectedActor = Engine->GetSelectedActor();

            if (SelectedComponent != nullptr)
            {
                TargetComponent = SelectedComponent;
            }
            else if (SelectedActor != nullptr)
            {
                TargetComponent = SelectedActor->GetRootComponent();
            }

        }
        const bool bIsSelected = (Engine && TargetComponent == Comp);


        FMatrix WorldMatrix = Comp->GetWorldMatrix();
        FVector4 UUIDColor = Comp->EncodeUUID() / 255.0f;

        UpdateObjectConstant(WorldMatrix, UUIDColor, bIsSelected);

        RenderSkeletalMesh(RenderData, Comp->GetSkeletalMesh()->GetMaterials(), Comp->GetOverrideMaterials(), Comp->GetselectedSubMeshIndex());

        if (Viewport->GetShowFlag() & static_cast<uint64>(EEngineShowFlags::SF_AABB))
        {
            FEngineLoop::PrimitiveDrawBatch.AddAABBToBatch(Comp->GetBoundingBox(), Comp->GetWorldLocation(), WorldMatrix);
        }
        // Begin Test
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
        // End Test
    }
}

void FMeshRenderPass::RenderStaticMesh(FStaticMeshRenderData* RenderData, TArray<FMaterialSlot*> Materials, TArray<UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const
{
    UINT Stride = sizeof(FStaticMeshVertex);
    UINT Offset = 0;

    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &RenderData->VertexBuffer, &Stride, &Offset);

    if (RenderData->IndexBuffer)
    {
        Graphics->DeviceContext->IASetIndexBuffer(RenderData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }

    if (RenderData->MaterialSubsets.Num() == 0)
    {
        Graphics->DeviceContext->DrawIndexed(RenderData->Indices.Num(), 0, 0);
        return;
    }

    for (int SubMeshIndex = 0; SubMeshIndex < RenderData->MaterialSubsets.Num(); SubMeshIndex++)
    {
        uint32 MaterialIndex = RenderData->MaterialSubsets[SubMeshIndex].MaterialIndex;

        FSubMeshConstants SubMeshData = (SubMeshIndex == SelectedSubMeshIndex) ? FSubMeshConstants(true) : FSubMeshConstants(false);

        BufferManager->UpdateConstantBuffer(TEXT("FSubMeshConstants"), SubMeshData);

        if (OverrideMaterials[MaterialIndex] != nullptr)
        {
            MaterialUtils::UpdateMaterial(BufferManager, Graphics, OverrideMaterials[MaterialIndex]->GetMaterialInfo());
        }
        else
        {
            MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->Material->GetMaterialInfo());
        }

        uint32 StartIndex = RenderData->MaterialSubsets[SubMeshIndex].IndexStart;
        uint32 IndexCount = RenderData->MaterialSubsets[SubMeshIndex].IndexCount;
        Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
    }
}

void FMeshRenderPass::RenderSkeletalMesh(FSkeletalMeshRenderData* RenderData, TArray<FMaterialSlot*> Materials, TArray<UMaterial*> OverrideMaterials, int SelectedSubMeshIndex) const
{
    // !TODO : SkeletalMesh 쉐이더 생기면 변경
    UINT Stride = sizeof(FStaticMeshVertex);
    UINT Offset = 0;

    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &RenderData->VertexBuffer, &Stride, &Offset);

    if (RenderData->IndexBuffer)
    {
        Graphics->DeviceContext->IASetIndexBuffer(RenderData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }

    if (RenderData->MaterialSubsets.Num() == 0)
    {
        Graphics->DeviceContext->DrawIndexed(RenderData->Indices.Num(), 0, 0);
        return;
    }

    for (int SubMeshIndex = 0; SubMeshIndex < RenderData->MaterialSubsets.Num(); SubMeshIndex++)
    {
        uint32 MaterialIndex = RenderData->MaterialSubsets[SubMeshIndex].MaterialIndex;

        FSubMeshConstants SubMeshData = (SubMeshIndex == SelectedSubMeshIndex) ? FSubMeshConstants(true) : FSubMeshConstants(false);

        BufferManager->UpdateConstantBuffer(TEXT("FSubMeshConstants"), SubMeshData);

        if (OverrideMaterials.IsValidIndex(MaterialIndex) && OverrideMaterials[MaterialIndex] != nullptr)
        {
            MaterialUtils::UpdateMaterial(BufferManager, Graphics, OverrideMaterials[MaterialIndex]->GetMaterialInfo());
        }
        else
        {
            MaterialUtils::UpdateMaterial(BufferManager, Graphics, Materials[MaterialIndex]->Material->GetMaterialInfo());
        }

        uint32 StartIndex = RenderData->MaterialSubsets[SubMeshIndex].IndexStart;
        uint32 IndexCount = RenderData->MaterialSubsets[SubMeshIndex].IndexCount;
        Graphics->DeviceContext->DrawIndexed(IndexCount, StartIndex, 0);
    }
}

void FMeshRenderPass::PrepareRenderState(const std::shared_ptr<FViewportClient>& Viewport)
{
    // !TODO : 이 패스에서 어떤 메시를 렌더링하는지에 따라 분기(Static / Skeletal)
    const EViewModeIndex ViewMode = Viewport->GetViewMode();

    ChangeViewMode(ViewMode);

    Graphics->DeviceContext->VSSetShader(StaticMesh_VertexShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(StaticMesh_InputLayout);

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

    // Pixel Shader
    if (ViewMode == EViewModeIndex::VMI_SceneDepth)
    {
        Graphics->DeviceContext->PSSetShader(StaticMesh_DebugDepthShader, nullptr, 0);
    }
    else if (ViewMode == EViewModeIndex::VMI_WorldNormal)
    {
        Graphics->DeviceContext->PSSetShader(StaticMesh_DebugWorldNormalShader, nullptr, 0);
    }
    else
    {
        Graphics->DeviceContext->PSSetShader(StaticMesh_PixelShader, nullptr, 0);
    }
}
