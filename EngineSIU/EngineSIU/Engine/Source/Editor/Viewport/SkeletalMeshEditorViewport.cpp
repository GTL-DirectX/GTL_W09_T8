#include "SkeletalMeshEditorViewport.h"

#include "ImGUI/imgui.h"
#include "ImGUI/imgui_internal.h"   

#include "Classes/Engine/AssetManager.h"
#include "Engine/Resource/FBXManager.h"

#include "World/World.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/Mesh/SkeletalMesh.h"

FSkeletalMeshEditorViewport::FSkeletalMeshEditorViewport(EViewScreenLocation InViewLocation)
    : FEditorViewport(InViewLocation)
{
}

void FSkeletalMeshEditorViewport::Initialize(const FRect& InRect)
{
    FEditorViewport::Initialize(InRect);

    if (World)
    {
        ViewerActor = World->SpawnActor<AActor>();
        if (ViewerActor)
        {
            SkeletalMeshComponent = ViewerActor->AddComponent<USkeletalMeshComponent>("ViwerSKMeshComp");
        }
    }

}

void FSkeletalMeshEditorViewport::Draw(float DeltaTime)
{
    
    if (ImGui::Begin("SkeletalMesh Viewer", &bShowViewport))
    {
        ImVec2 ContentRegion = ImGui::GetContentRegionAvail();

        ImGui::Columns(2, "ViewerColumns", true);

        ID3D11ShaderResourceView* SRV = GetViewportResource()->GetRenderTarget(EResourceType::ERT_Compositing)->SRV;
        if (SRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc;
            SRV->GetDesc(&SrvDesc);
            D3D11_TEXTURE2D_DESC TexDesc;

            ID3D11Resource* Resource;
            SRV->GetResource(&Resource);
            ID3D11Texture2D* Texture = static_cast<ID3D11Texture2D*>(Resource);
            Texture->GetDesc(&TexDesc);
            Resource->Release();

            float originalWidth = static_cast<float>(TexDesc.Width);
            float originalHeight = static_cast<float>(TexDesc.Height);
            float OriginalAspect = originalWidth / originalHeight;

            float AvailableWidth = ContentRegion.x;
            float AvailableHeight = ContentRegion.y;

            float DisplayWidth, DisplayHeight;

            if (AvailableWidth / AvailableHeight > OriginalAspect)
            {
                DisplayHeight = AvailableHeight;
                DisplayWidth = DisplayHeight * OriginalAspect;
            }
            else
            {
                DisplayWidth = AvailableWidth;
                DisplayHeight = DisplayWidth / OriginalAspect;
            }

            float OffsetX = (AvailableWidth - DisplayWidth) * 0.5f;
            float OffsetY = (AvailableHeight - DisplayHeight) * 0.5f;

            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + OffsetX, ImGui::GetCursorPosY() + OffsetY));
            ImGui::Image(reinterpret_cast<ImTextureID>(SRV), ImVec2(DisplayWidth, DisplayHeight));
        }

        ImGui::NextColumn();

        const TMap<FName, FAssetInfo> Assets = UAssetManager::Get().GetAssetRegistry();

        FString PreviewName;
        if (SkeletalMesh)
        {
            PreviewName = SkeletalMesh->GetObjectName();
        }
        else
        {
            PreviewName = TEXT("None");
        }

        if (ImGui::BeginCombo("##SkeletalMesh", GetData(PreviewName), ImGuiComboFlags_None))
        {
            if (ImGui::Selectable(TEXT("None"), false))
            {
                SetSkeletalMesh(nullptr);
            }

            for (const auto& Asset : Assets)
            {
                if (Asset.Value.AssetType == EAssetType::SkeletalMesh)
                {
                    if (ImGui::Selectable(GetData(Asset.Value.AssetName.ToString()), false))
                    {
                        FString MeshName = Asset.Value.PackagePath.ToString() + "/" + Asset.Value.AssetName.ToString();
                        USkeletalMesh* SkeletalMesh = FFBXManager::Get().LoadFbx(MeshName);
                        if (SkeletalMesh && SkeletalMeshComponent)
                        {
                            SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Columns(1);

    }
    ImGui::End(); 
}

void FSkeletalMeshEditorViewport::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    SkeletalMesh = InSkeletalMesh;
    if (SkeletalMesh && SkeletalMeshComponent)
    {
        SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
    }
}
 
