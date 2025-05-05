#include "ViewerEditor.h"
#include "ImGUI/imgui.h"

#include "Rendering/Mesh/SkeletalMesh.h"
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
// End Test

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
    ACube* CubeActor = ViewerWorld->SpawnActor<ACube>();
    if (CubeActor)
    {
        CubeActor->SetActorLabel(TEXT("Viewer_Cube"));
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
    }
    ImGui::End(); 
}
