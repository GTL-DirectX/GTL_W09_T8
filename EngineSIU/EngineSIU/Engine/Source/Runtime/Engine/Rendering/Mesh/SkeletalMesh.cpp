#include "SkeletalMesh.h"
#include "SkeletalMeshRenderData.h"
#include "Rendering/Material/Material.h"

USkeletalMesh::~USkeletalMesh()
{
    if (RenderData == nullptr) return;

    if(RenderData->VertexBuffer)
    {
        RenderData->VertexBuffer->Release();
        RenderData->VertexBuffer = nullptr;
    }
    if (RenderData->IndexBuffer)
    {
        RenderData->IndexBuffer->Release();
        RenderData->IndexBuffer = nullptr;
    }

    delete RenderData;
    RenderData = nullptr;
}

uint32 USkeletalMesh::GetMaterialIndex(FName MaterialSlotName) const
{
    for (uint32 materialIndex = 0; materialIndex < Materials.Num(); materialIndex++) {
        if (Materials[materialIndex]->MaterialSlotName == MaterialSlotName)
            return materialIndex;
    }

    return -1;
}

void USkeletalMesh::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    for (const FStaticMaterial* Material : Materials)
    {
        Out.Emplace(Material->Material);
    }
}

void USkeletalMesh::AddMaterial(UMaterial* InMaterial)
{
    FStaticMaterial* newMaterialSlot = new FStaticMaterial();
    newMaterialSlot->Material = InMaterial;
    newMaterialSlot->MaterialSlotName = InMaterial->GetName();
    Materials.Add(newMaterialSlot);
}

FString USkeletalMesh::GetObjectName() const
{
    if(!RenderData)
        return FString();
    return RenderData->FilePath;
}

void USkeletalMesh::SetData(FSkeletalMeshRenderData* renderData)
{
    RenderData = renderData;

    for (int materialIndex = 0; materialIndex < RenderData->Materials.Num(); materialIndex++) 
    {
        FStaticMaterial* newMaterialSlot = new FStaticMaterial();
        UMaterial* newMaterial = UMaterial::CreateMaterial(RenderData->Materials[materialIndex]);
        newMaterialSlot->Material = newMaterial;
        newMaterialSlot->MaterialSlotName = RenderData->Materials[materialIndex].MaterialName;
        Materials.Add(newMaterialSlot);
    }
}
