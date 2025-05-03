#pragma once
#include "Define.h"
#include "World/World.h"

class SSplitterH;
class SSplitterV;
class UEngine;
class FEditorViewportClient;


class SLevelEditor
{
public:
    SLevelEditor();

    void Initialize(uint32 InEditorWidth, uint32 InEditorHeight, UEngine* EditorEngine);
    void Tick(float DeltaTime);
    void Release();

    void ResizeEditor(uint32 InEditorWidth, uint32 InEditorHeight);
    void SelectViewport(const FVector2D& Point);

    void ResizeViewports();
    void SetEnableMultiViewport(bool bIsEnable);
    bool IsMultiViewport() const;

    // AdditionalViewports에 추가적인 뷰포트를 추가
    FEditorViewportClient* AddWindowViewportClient(FName ViewportName, UWorld* PreviewWorld, const FRect& InRect);
    void RemoveWindowViewportClient(FName ViewportName);

private:
    void OnWorldChanged(UWorld* PIEWorld);

private:
    SSplitterH* HSplitter;
    SSplitterV* VSplitter;
    
    std::shared_ptr<FEditorViewportClient> ViewportClients[4];
    std::shared_ptr<FEditorViewportClient> ActiveViewportClient;
    TMap<FName, std::shared_ptr<FEditorViewportClient>> WindowViewportClients;

    /** 우클릭 시 캡처된 마우스 커서의 초기 위치 (스크린 좌표계) */
    FVector2D MousePinPosition;

    /** 좌클릭시 커서와 선택된 Actor와의 거리 차 */
    FVector TargetDiff;

    bool bMultiViewportMode;
    
    uint32 EditorWidth;
    uint32 EditorHeight;

public:
    std::shared_ptr<FEditorViewportClient>* GetViewports() { return ViewportClients; }
    std::shared_ptr<FEditorViewportClient> GetActiveViewportClient() const
    {
        return ActiveViewportClient;
    }
    void SetActiveViewportClient(const std::shared_ptr<FEditorViewportClient>& InViewportClient)
    {
        ActiveViewportClient = InViewportClient;
    }
    void SetActiveViewportClient(int Index)
    {
        ActiveViewportClient = ViewportClients[Index];
    }
    const TMap<FName, std::shared_ptr<FEditorViewportClient>>& GetWindowViewportClients() const
    {
        return WindowViewportClients;
    }

public:
    void LoadConfig();
    void SaveConfig();
};
