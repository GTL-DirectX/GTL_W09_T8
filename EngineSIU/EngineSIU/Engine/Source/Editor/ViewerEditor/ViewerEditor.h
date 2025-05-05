#pragma once

// 이거는 Slate를 쓰기는 좀 그렇고, 그런데 ImGui를 쓸건데 굳이 이걸 따로 만드는게 좋을까 고민중
// 안만들면 class explosion

class UWorld;
class FEditorViewportClient;
class FString;

class ViewerEditor
{
public:
    ViewerEditor() = delete;
    static void RenderViewerWindow(bool& bShowWindow);

private:
    static void InitializeViewerResources();
    static void DestroyViewerResources();

    static UWorld* ViewerWorld;
    static FEditorViewportClient* ViewerViewportClient;
    static bool bIsInitialized;
    static FString ViewportIdentifier; 
};
