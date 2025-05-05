#pragma once

#include "Define.h"

class UWorld;
class FEditorViewportClient;
class FString;
class AActor;
class USkeletalMesh;

class ViewerEditor
{
public:
    ViewerEditor() = delete;
    static void RenderViewerWindow(bool& bShowWindow);

    static AActor* SelectedActor;
    static USkeletalMesh* SelectedSkeletalMesh;
private:
    static void InitializeViewerResources();
    static void DestroyViewerResources();

    static void DrawBoneHierarchyRecursive(int BoneIndex, const TArray<FString>& BoneNames, const TArray<TArray<int>>& Children);

    static UWorld* ViewerWorld;
    static FEditorViewportClient* ViewerViewportClient;
    static bool bIsInitialized;
    static FString ViewportIdentifier;

};
