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

// End Test

// static 멤버 변수 정의 및 초기화
AActor* ViewerEditor::SelectedActor = nullptr;
USkeletalMesh* ViewerEditor::SelectedSkeletalMesh = nullptr;

UWorld* ViewerEditor::ViewerWorld = nullptr;
FEditorViewportClient* ViewerEditor::ViewerViewportClient = nullptr;
bool ViewerEditor::bIsInitialized = false;
FString ViewerEditor::ViewportIdentifier = TEXT("SkeletalMeshViewerViewport");

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

    // Begin Test
    SelectedActor = ViewerWorld->SpawnActor<AActor>();
    USkeletalMeshComponent* SkeletalMeshComp = SelectedActor->AddComponent<USkeletalMeshComponent>();

    ADirectionalLight* LightActor = ViewerWorld->SpawnActor<ADirectionalLight>();
    UDirectionalLightComponent* LightComp=LightActor->GetComponentByClass<UDirectionalLightComponent>();
    LightComp->SetRelativeRotation(FRotator(0.f, 180.f,0.f));

    SelectedSkeletalMesh = FFBXManager::Get().LoadFbx("Contents\\fbx\\character.fbx");
    SkeletalMeshComp->SetSkeletalMesh(SelectedSkeletalMesh);
    if (SelectedActor)
    {
        SelectedActor->SetActorLabel(TEXT("Viewer_SkeletalMesh"));
    }

    ViewerViewportClient = GEngineLoop.GetLevelEditor()->AddWindowViewportClient(
        ViewportIdentifier,
        ViewerWorld,
        FRect(100, 100, 800, 800)
    );
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
    vpCam.SetLocation(FVector(-10, 0, 5));
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

void ViewerEditor::DrawBoneHierarchyRecursive(int BoneIndex, const TArray<FString>& BoneNames, const TArray<TArray<int>>& Children)
{
    if (BoneIndex < 0 || BoneIndex >= BoneNames.Num())
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

    bool bNodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>((intptr_t)BoneIndex), NodeFlags, "%s", GetData(BoneName));

    if (bNodeOpen && !(NodeFlags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for (int ChildIndex : CurrentChildren)
        {
            DrawBoneHierarchyRecursive(ChildIndex, BoneNames, Children);
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
        if (ViewerViewportClient && ViewerViewportClient->GetViewportResource())
        {
            ImVec2 contentRegion = ImGui::GetContentRegionAvail();
            float width = FMath::Max(1.0f, contentRegion.x);
            float height = FMath::Max(1.0f, contentRegion.y);

            ID3D11ShaderResourceView* SRV = ViewerViewportClient->GetViewportResource()->GetRenderTarget(EResourceType::ERT_Compositing)->SRV;
            if (SRV)
            {
                ImGui::Image(reinterpret_cast<ImTextureID>(SRV), ImVec2(width, height));
            }
        }

        //여기에 skeletal mesh property가 들어갈 예정
        // 1. 그 SkeletalMesh 고르는 것

        // 임시로 파일 경로로 해놨음.
        // 원래 DisplayName이 있었던 것 같은데
        FString PreviewName = SelectedSkeletalMesh ? SelectedSkeletalMesh->GetRenderData()->FilePath : TEXT("None");

        if (ImGui::BeginCombo("Skeletal Mesh", GetData(PreviewName)))
        {
            if (ImGui::Selectable(TEXT("None"), SelectedSkeletalMesh == nullptr))
            {
                SelectedSkeletalMesh = nullptr;
            }

            // 지금 registry해놓은게 없어서 일단 테스트 코드로 수행.

            ImGui::EndCombo();
        }

        if (SelectedSkeletalMesh)
        {
            FSkeletalMeshRenderData* RenderData = SelectedSkeletalMesh->GetRenderData();

            if (ImGui::CollapsingHeader("Bone Hierarchy", ImGuiTreeNodeFlags_DefaultOpen))
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
                        DrawBoneHierarchyRecursive(i, RenderData->BoneNames, Children);
                    }
                }
            }
        }

    }
    ImGui::End();
}
