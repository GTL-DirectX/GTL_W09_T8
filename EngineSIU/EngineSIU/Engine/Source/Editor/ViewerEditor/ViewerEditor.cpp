#include "ViewerEditor.h"
#include "ImGUI/imgui.h"

#include "Rendering/Mesh/SkeletalMesh.h"
#include "Classes/Engine/AssetManager.h"
#include "Engine/ObjLoader.h"

void ViewerEditor::CreateViewerWindow()
{
    static bool show_new_window = false;

    if (ImGui::Button("SkeletalMesh Viewer"))
    {
        show_new_window = true;
    }

    if (show_new_window)
    {
        ImGui::Begin("SkeletalMesh Viewer", &show_new_window);

        ImGui::Text("SkeletalMesh Viewer");
        if (ImGui::Button("Close"))
        {
            show_new_window = false;
        }

        ImGui::End();
    }

}

void ViewerEditor::RenderSkeletalMesh(USkeletalMesh* SkeletalMeshComp) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Static Mesh", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        ImGui::Text("StaticMesh");
        ImGui::SameLine();

        FString PreviewName;
        // TO-DO: USkeletalMeshComponent::GetSkeletalMesh()를 구현할 예정
        // 다른 함수들도 많이 남았음.
        if (SkeletalMeshComp->GetSkeletalMesh())
        {
            PreviewName = SkeletalMeshComp->GetSkeletalMesh()->GetRenderData()->DisplayName;
        }
        else
        {
            PreviewName = TEXT("None");
        }

        const TMap<FName, FAssetInfo> Assets = UAssetManager::Get().GetAssetRegistry();

        if (ImGui::BeginCombo("##StaticMesh", GetData(PreviewName), ImGuiComboFlags_None))
        {
            if (ImGui::Selectable(TEXT("None"), false))
            {
                SkeletalMeshComp->SetSkeletalMesh(nullptr);
            }

            for (const auto& Asset : Assets)
            {
                if (Asset.Value.AssetType == EAssetType::SkeletalMesh)
                {
                    if (ImGui::Selectable(GetData(Asset.Value.AssetName.ToString()), false))
                    {
                        FString MeshName = Asset.Value.PackagePath.ToString() + "/" + Asset.Value.AssetName.ToString();
                        USkeletalMesh* SkeletalMesh = FObjManager::GetSkeletalMesh(MeshName.ToWideString());
                        if (SkeletalMesh)
                        {
                            SkeletalMeshComp->SetSkeletalMesh(SkeletalMesh);
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}
