#include "ViewerEditor.h"
#include "ImGUI/imgui.h"

#include "Rendering/Mesh/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Classes/Engine/AssetManager.h"
#include "Engine/ObjLoader.h"

// Begin Test
#include "Runtime/CoreUObject/UObject/ObjectFactory.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Engine/Classes/Actors/Cube.h"
#include "Editor/LevelEditor/SLevelEditor.h"
#include "Editor/UnrealEd/EditorViewportClient.h"
#include "Runtime/Engine/UnrealClient.h"
#include "Runtime/Core/Container/String.h"
#include "Runtime/Engine/Classes/Engine/Resource/FBXManager.h"

#include "Runtime/Engine/Classes/Actors/DirectionalLightActor.h"
#include "Components/Light/DirectionalLightComponent.h"

#include "ShowFlag.h"
#include "Math/Quat.h"
// End Test

UWorld* ViewerEditor::ViewerWorld = nullptr;
FEditorViewportClient* ViewerEditor::ViewerViewportClient = nullptr;
bool ViewerEditor::bIsInitialized = false;
FString ViewerEditor::ViewportIdentifier = TEXT("SkeletalMeshViewerViewport");
AActor* ViewerEditor::SelectedActor = nullptr;
USkeletalMesh* ViewerEditor::SelectedSkeletalMesh = nullptr;
bool ViewerEditor::bShowBones = false;
int ViewerEditor::SelectedBone = -1;
USkeletalMeshComponent* ViewerEditor::SkeletalMeshComponent = nullptr;

void ViewerEditor::InitializeViewerResources()
{
    if (bIsInitialized || !GEngineLoop.GetLevelEditor())
    {
        return;
    }

    ViewerWorld = FObjectFactory::ConstructObject<UWorld>(nullptr);
    if (!ViewerWorld)
    {
        return;
    }

    ViewerWorld->InitializeNewWorld();
    ViewerWorld->WorldType = EWorldType::EditorPreview;

    SelectedActor = ViewerWorld->SpawnActor<AActor>();
    SelectedActor->AddComponent<USkeletalMeshComponent>();
    SkeletalMeshComponent = SelectedActor->GetComponentByClass<USkeletalMeshComponent>();
    ADirectionalLight* LightActor = ViewerWorld->SpawnActor<ADirectionalLight>();
    UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
    LightComp->SetRelativeRotation(FRotator(0.f, 180.f,0.f));

    if (SelectedActor)
    {
        SelectedActor->SetActorLabel(TEXT("Viewer_SkeletalMesh"));
    }

    ViewerViewportClient = dynamic_cast<FEditorViewportClient*>(GEngineLoop.GetLevelEditor()->AddWindowViewportClient(
        ViewportIdentifier,
        ViewerWorld,
        FRect(100, 100, 800, 800),
        EEditorViewportType::SkeletalMeshEditor
    ));
    // End Test

    if (!ViewerViewportClient)
    {
        if (ViewerWorld)
        {
            ViewerWorld = nullptr;
        }
        return;
    }

    ViewerViewportClient->SetShouldDraw(false);
    FViewportCamera& vpCam = ViewerViewportClient->GetPerspectiveCamera();
    vpCam.SetLocation(FVector(-100, 0, 5));
    FVector TargetLocation = FVector(0, 0, 0);
    FVector CamLocation = vpCam.GetLocation();
    FVector Dir = (TargetLocation - CamLocation).GetSafeNormal();

    FVector forward;

    float pitch = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.Z));
    float yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.X, Dir.Z));

    forward.X = FMath::Cos(FMath::DegreesToRadians(pitch)) * FMath::Cos(FMath::DegreesToRadians(yaw));
    forward.Y = FMath::Cos(FMath::DegreesToRadians(pitch)) * FMath::Sin(FMath::DegreesToRadians(yaw));
    forward.Z = FMath::Sin(FMath::DegreesToRadians(pitch));

    vpCam.SetRotation(forward);

    bIsInitialized = true;
}

void ViewerEditor::DestroyViewerResources()
{
    if (!bIsInitialized || !GEngineLoop.GetLevelEditor())
    {
        return;
    }

    if (ViewerViewportClient)
    {
        GEngineLoop.GetLevelEditor()->RemoveWindowViewportClient(ViewportIdentifier);
        ViewerViewportClient = nullptr;
    }

    if (ViewerWorld)
    {
        ViewerWorld->Release();
        ViewerWorld = nullptr;
    }

    bIsInitialized = false;
}


void ViewerEditor::DrawBoneHierarchyRecursive(int BoneIndex, const TArray<FString>& BoneNames, const TArray<TArray<int>>& Children, const FSkeletalMeshRenderData* RenderData)
{
    if (BoneIndex < 0 || BoneIndex >= BoneNames.Num() || !RenderData)
    {
        return;
    }

    const FString& BoneName = BoneNames[BoneIndex];
    const TArray<int>& CurrentChildren = Children[BoneIndex];

    ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (CurrentChildren.IsEmpty())
    {
        NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // 선택된 본인지 확인
    bool bIsSelected = (SelectedBone == BoneIndex);
    if (bIsSelected)
    {
        NodeFlags |= ImGuiTreeNodeFlags_Selected;
    }

    bool bNodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>((intptr_t)BoneIndex), NodeFlags, "%s", GetData(BoneName));

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        SelectedBone = BoneIndex;
    }


    if (bNodeOpen && !(NodeFlags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for (int ChildIndex : CurrentChildren)
        {
            DrawBoneHierarchyRecursive(ChildIndex, BoneNames, Children, RenderData);
        }
        ImGui::TreePop();
    }
}

void ViewerEditor::RenderViewerWindow(bool& bShowWindow)
{
    if (!bShowWindow)
    {
        if (bIsInitialized)
        {
            DestroyViewerResources();
        }
        return;
    }
    if (!bIsInitialized)
    {
        InitializeViewerResources();
        if (!bIsInitialized) {
            bShowWindow = false;
            return;
        }
    }
    if (ImGui::Begin("SkeletalMesh Viewer", &bShowWindow))
    {
        if (SkeletalMeshComponent)
        {
            SelectedSkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
        }
        else
        {
            SelectedSkeletalMesh = nullptr;
        }
        ImGui::Columns(2, "ViewerColumns", true);

        if (ViewerViewportClient && ViewerViewportClient->GetViewportResource())
        {
            ImVec2 ContentRegion = ImGui::GetContentRegionAvail();

            ID3D11ShaderResourceView* SRV = ViewerViewportClient->GetViewportResource()->GetRenderTarget(EResourceType::ERT_Compositing)->SRV;
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
        }

        ImGui::NextColumn();

        FString PreviewName;

        if (SelectedSkeletalMesh)
        {
            PreviewName = SelectedSkeletalMesh->GetRenderData()->ObjectName;
        }
        else
        {
            PreviewName = TEXT("None");
        }

        const TMap<FName, FAssetInfo> Assets = UAssetManager::Get().GetAssetRegistry();

        if (ImGui::BeginCombo("##SkeletalMesh", GetData(PreviewName), ImGuiComboFlags_None))
        {
            if (ImGui::Selectable(TEXT("None"), false))
            {
                SkeletalMeshComponent->SetSkeletalMesh(nullptr);
            }

            for (const auto& Asset : Assets)
            {
                if (Asset.Value.AssetType == EAssetType::SkeletalMesh)
                {
                    if (ImGui::Selectable(GetData(Asset.Value.AssetName.ToString()), false))
                    {
                        FString MeshName = Asset.Value.PackagePath.ToString() + "/" + Asset.Value.AssetName.ToString();
                        USkeletalMesh* SkeletalMesh = FFBXManager::Get().LoadFbx(MeshName);
                        if (SkeletalMesh)
                        {
                            SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }

        if (SelectedSkeletalMesh)
        {
            FSkeletalMeshRenderData* RenderData = SelectedSkeletalMesh->GetRenderData();
            if (ImGui::CollapsingHeader("Bone Hierarchy", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Show Bones", &bShowBones);
                if (ViewerViewportClient)
                {
                    ViewerViewportClient->SetShowFlagState(EEngineShowFlags::SF_Bone, bShowBones);
                }

                // 본 계층 트리 표시를 위한 Child Window (스크롤 가능하게)
                ImGui::BeginChild("BoneTreeChildWindow", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.5f), true, ImGuiWindowFlags_HorizontalScrollbar);
                {
                    int BoneCount = RenderData->BoneNames.Num();
                    TArray<TArray<int>> Children;
                    Children.SetNum(BoneCount);
                    for (int i = 0; i < BoneCount; ++i)
                    {
                        int ParentIndex = RenderData->ParentBoneIndices[i];
                        if (ParentIndex >= 0 && ParentIndex < BoneCount)
                        {
                            Children[ParentIndex].Add(i);
                        }
                    }
                    for (int i = 0; i < BoneCount; ++i)
                    {
                        if (RenderData->ParentBoneIndices[i] < 0)
                        {
                            DrawBoneHierarchyRecursive(i, RenderData->BoneNames, Children, RenderData);
                        }
                    }
                }
                ImGui::EndChild();

                // FIX-ME
                // FTransform 반드시 만들것..
                if (SelectedBone != -1 && SelectedBone < RenderData->BoneNames.Num())
                {
                    ImGui::Separator();
                    ImGui::Text("Selected Bone");
                    ImGui::Indent();
                    ImGui::Text("Index: %d", SelectedBone);

                    int ParentIdx = RenderData->ParentBoneIndices[SelectedBone];
                    if (ParentIdx != -1)
                    {
                        ImGui::Text("Parent: %s (Index: %d)", GetData(RenderData->BoneNames[ParentIdx]), ParentIdx);
                    }
                    else
                    {
                        ImGui::Text("Parent: None (Root Bone)");
                    }

                    static int   LastSelectedBoneForLocalEdit = -1;
                    static float EditableLocalLoc[3] = { 0.0f, 0.0f, 0.0f };
                    static float EditableLocalRot[3] = { 0.0f, 0.0f, 0.0f }; // Pitch, Yaw, Roll
                    static float EditableLocalScale[3] = { 1.0f, 1.0f, 1.0f };

                    bool bValueChanged = false;

                    const FMatrix& CurrentLocalBindMatrix = RenderData->LocalBindPose[SelectedBone];
                    FVector  CurrentOriginLoc = CurrentLocalBindMatrix.GetTranslationVector();
                    FRotator CurrentOriginRot = CurrentLocalBindMatrix.GetMatrixWithoutScale().ToQuat().Rotator();
                    FVector  CurrentOriginScale = CurrentLocalBindMatrix.GetScaleVector();
                    if (SelectedBone != LastSelectedBoneForLocalEdit)
                    {
                        EditableLocalLoc[0] = CurrentOriginLoc.X;
                        EditableLocalLoc[1] = CurrentOriginLoc.Y;
                        EditableLocalLoc[2] = CurrentOriginLoc.Z;

                        EditableLocalRot[0] = CurrentOriginRot.Pitch;
                        EditableLocalRot[1] = CurrentOriginRot.Yaw;
                        EditableLocalRot[2] = CurrentOriginRot.Roll;

                        EditableLocalScale[0] = CurrentOriginScale.X;
                        EditableLocalScale[1] = CurrentOriginScale.Y;
                        EditableLocalScale[2] = CurrentOriginScale.Z;

                        LastSelectedBoneForLocalEdit = SelectedBone;
                    }

                    if (ImGui::InputFloat3("Local Location##LocalEdit", EditableLocalLoc)) bValueChanged = true;
                    if (ImGui::InputFloat3("Local Rotation (P,Y,R)##LocalEdit", EditableLocalRot)) bValueChanged = true;
                    if (ImGui::InputFloat3("Local Scale##LocalEdit", EditableLocalScale)) bValueChanged = true;

                    if (bValueChanged)
                    {
                        if (SkeletalMeshComponent && SelectedSkeletalMesh && SelectedSkeletalMesh->GetRenderData())
                        {
                            FSkeletalMeshRenderData* ModifiableRenderData = SelectedSkeletalMesh->GetRenderData();

                            FVector  TargetLocalLocation = FVector(EditableLocalLoc[0], EditableLocalLoc[1], EditableLocalLoc[2]);
                            FRotator TargetLocalRotation = FRotator(EditableLocalRot[0], EditableLocalRot[1], EditableLocalRot[2]);
                            FVector  TargetLocalScale = FVector(EditableLocalScale[0], EditableLocalScale[1], EditableLocalScale[2]);

                            FMatrix OldScale = FMatrix::CreateScaleMatrix(CurrentOriginScale.X, CurrentOriginScale.Y, CurrentOriginScale.Z);
                            FMatrix OldRotation = FMatrix::CreateRotationMatrix(CurrentOriginRot.Roll, CurrentOriginRot.Pitch, CurrentOriginRot.Yaw); 
                            FMatrix OldTranslation = FMatrix::CreateTranslationMatrix(CurrentOriginLoc);
                            FMatrix M_old = OldTranslation * OldRotation * OldScale;

                            FMatrix NewScale = FMatrix::CreateScaleMatrix(TargetLocalScale.X, TargetLocalScale.Y, TargetLocalScale.Z);
                            FMatrix NewRotation = FMatrix::CreateRotationMatrix(TargetLocalRotation.Roll, TargetLocalRotation.Pitch, TargetLocalRotation.Yaw);
                            FMatrix NewTranslation = FMatrix::CreateTranslationMatrix(TargetLocalLocation);
                            FMatrix M_new = NewTranslation * NewRotation * NewScale;

                            FMatrix OldInverse= FMatrix::Inverse(M_old);
                            FMatrix DeltaTransform = FMatrix::Identity;

                            DeltaTransform = (OldInverse.Determinant3x3() == 0) ? FMatrix::Identity : M_new * OldInverse;

                            FVector  DeltaLocation = DeltaTransform.GetTranslationVector();
                            FRotator DeltaRotation = DeltaTransform.GetMatrixWithoutScale().ToQuat().Rotator();
                            FVector  DeltaScale = DeltaTransform.GetScaleVector();

                            ModifiableRenderData->ApplyBoneOffsetAndRebuild(
                                SelectedBone,
                                DeltaLocation,
                                DeltaRotation,
                                DeltaScale
                            );
                        }
                    }
                    ImGui::Spacing();

                    const FMatrix& WorldReferencePose = RenderData->ReferencePose[SelectedBone];
                    FVector WorldPosition = WorldReferencePose.GetTranslationVector();
                    FRotator WorldRotation = WorldReferencePose.GetMatrixWithoutScale().ToQuat().Rotator();
                    FVector WorldScale = WorldReferencePose.GetScaleVector();

                    ImGui::Text("World Position: (X: %.3f, Y: %.3f, Z: %.3f)", WorldPosition.X, WorldPosition.Y, WorldPosition.Z);
                    ImGui::Text("World Rotation: (P: %.3f, Y: %.3f, R: %.3f)", WorldRotation.Pitch, WorldRotation.Yaw, WorldRotation.Roll);
                    ImGui::Text("World Scale: (X: %.3f, Y: %.3f, Z: %.3f)", WorldScale.X, WorldScale.Y, WorldScale.Z);

                    ImGui::Unindent();
                }
            }
        }
        ImGui::Columns(1);
    }
    ImGui::End();
}
