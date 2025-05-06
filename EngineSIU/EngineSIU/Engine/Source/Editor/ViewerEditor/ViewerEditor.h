#pragma once

#include "Define.h"

class UWorld;
class FEditorViewportClient;
class FString;
class AActor;
class USkeletalMesh;
struct FSkeletalMeshRenderData;

class ViewerEditor
{
public:
    ViewerEditor() = delete;

    static void DrawBoneHierarchyRecursive(int BoneIndex, const TArray<FString>& BoneNames, const TArray<TArray<int>>& Children, const FSkeletalMeshRenderData* RenderData);
    static void RenderViewerWindow(bool& bShowWindow);

    static AActor* SelectedActor;
    static USkeletalMesh* SelectedSkeletalMesh;
private:
    static void InitializeViewerResources();
    static void DestroyViewerResources();

    static UWorld* ViewerWorld;
    static FEditorViewportClient* ViewerViewportClient;
    static bool bIsInitialized;
    static FString ViewportIdentifier;

    static bool bShowBones;
    static int SelectedBone;
};
