#include "UnrealEd.h"
#include "EditorPanel.h"

#include "PropertyEditor/ControlEditorPanel.h"
#include "PropertyEditor/OutlinerEditorPanel.h"
#include "PropertyEditor/PropertyEditorPanel.h"

void UnrealEd::Initialize()
{
    auto ControlPanel = std::make_shared<ControlEditorPanel>();
    Panels["ControlPanel"] = ControlPanel;
    
    auto OutlinerPanel = std::make_shared<OutlinerEditorPanel>();
    Panels["OutlinerPanel"] = OutlinerPanel;
    
    auto PropertyPanel = std::make_shared<PropertyEditorPanel>();
    Panels["PropertyPanel"] = PropertyPanel;

    // 1. Viewer button을 만든다고 했으니까 그러면 ViewerPanel로 만드는게 아니라, ViewerWindow를 만들어야 하는건가?
    // 2. 월드 단위로 한다고 했으면 언리얼처럼 따로 월드(아니면 레벨 단위)로 패널을 따로 두는게 맞을라나
    //auto ViewerPanel = std::make_shared<ViewerEditorPanel>();
    //Panels["ViewerPanel"] = ViewerPanel;
}

void UnrealEd::Render() const
{
    for (const auto& Panel : Panels)
    {
        Panel.Value->Render();
    }
}

void UnrealEd::AddEditorPanel(const FString& PanelId, const std::shared_ptr<UEditorPanel>& EditorPanel)
{
    Panels[PanelId] = EditorPanel;
}

void UnrealEd::OnResize(HWND hWnd) const
{
    for (auto& Panel : Panels)
    {
        Panel.Value->OnResize(hWnd);
    }
}

std::shared_ptr<UEditorPanel> UnrealEd::GetEditorPanel(const FString& PanelId)
{
    return Panels[PanelId];
}
