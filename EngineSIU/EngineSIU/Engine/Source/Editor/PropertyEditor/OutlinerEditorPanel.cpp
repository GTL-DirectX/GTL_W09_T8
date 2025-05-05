#include "OutlinerEditorPanel.h"
#include "World/World.h"
#include "GameFramework/Actor.h"
#include "Engine/EditorEngine.h"
#include <functional>

#include "ImGUI/imgui.h"

void OutlinerEditorPanel::Render()
{
    ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_None;

    if (!ImGui::Begin("Outliner"))
    {
        ImGui::End();
        return;
    }
    
    ImGui::BeginChild("Objects");
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        ImGui::EndChild();
        ImGui::End();

        return;
    }

    std::function<void(USceneComponent*)> CreateNode = [&CreateNode, &Engine](USceneComponent* InComp)->void
        {
            FString Name = InComp->GetName();

            ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;
            if (InComp->GetAttachChildren().Num() == 0)
                Flags |= ImGuiTreeNodeFlags_Leaf;
        
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            bool NodeOpen = ImGui::TreeNodeEx(*Name, Flags);

            if (ImGui::IsItemClicked())
            {
                Engine->SelectActor(InComp->GetOwner());
                Engine->SelectComponent(InComp);
            }

            if (NodeOpen)
            {
                for (USceneComponent* Child : InComp->GetAttachChildren())
                {
                    CreateNode(Child);
                }
                ImGui::TreePop(); // 트리 닫기
            }
        };

    for (AActor* Actor : Engine->ActiveWorld->GetActiveLevel()->Actors)
    {
        // 현재 RootComponent 없으면 에러 생김
        if (Actor->GetRootComponent() == nullptr)
            continue;

        ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;

        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        bool NodeOpen = ImGui::TreeNodeEx(*Actor->GetName(), Flags);

        if (ImGui::IsItemClicked())
        {
            Engine->SelectActor(Actor);
            Engine->DeselectComponent(Engine->GetSelectedComponent());
        }

        if (NodeOpen)
        {
            CreateNode(Actor->GetRootComponent());
            ImGui::TreePop();
        }
    }

    ImGui::EndChild();

    ImGui::End();
}
    
void OutlinerEditorPanel::OnResize(HWND hWnd)
{
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    Width = clientRect.right - clientRect.left;
    Height = clientRect.bottom - clientRect.top;
}
