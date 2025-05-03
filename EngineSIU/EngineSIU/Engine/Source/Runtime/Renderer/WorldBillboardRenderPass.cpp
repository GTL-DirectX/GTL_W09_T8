
#include "WorldBillboardRenderPass.h"

#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "Components/BillboardComponent.h"
#include "Engine/ViewportClient.h"

FWorldBillboardRenderPass::FWorldBillboardRenderPass()
{
    ResourceType = EResourceType::ERT_Scene;
}

void FWorldBillboardRenderPass::PrepareRenderArr(const std::shared_ptr<FViewportClient>& Viewport)
{
    if (Viewport == nullptr || Viewport->GetWorld() == nullptr)
        return;

    BillboardComps.Empty();
    for (const auto Component : TObjectRange<UBillboardComponent>())
    {
        if (Component->GetWorld() == Viewport->GetWorld() && !Component->bIsEditorBillboard)
        {
            BillboardComps.Add(Component);
        }
    }
}
