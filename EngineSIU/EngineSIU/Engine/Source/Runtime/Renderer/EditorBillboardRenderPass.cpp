
#include "EditorBillboardRenderPass.h"

#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "Components/BillboardComponent.h"
#include "Engine/ViewportClient.h"

FEditorBillboardRenderPass::FEditorBillboardRenderPass()
{
    ResourceType = EResourceType::ERT_Editor;
}

void FEditorBillboardRenderPass::PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport)
{
    if (Viewport == nullptr || Viewport->GetWorld() == nullptr)
        return;

    BillboardComps.Empty();
    for (const auto Component : TObjectRange<UBillboardComponent>())
    {
        if (Component->GetWorld() == Viewport->GetWorld() && Component->bIsEditorBillboard)
        {
            BillboardComps.Add(Component);
        }
    }
}
