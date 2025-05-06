#include "Material.h"
#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"


UObject* UMaterial::Duplicate(UObject* InOuter)
{
    ThisClass* NewMaterial = Cast<ThisClass>(Super::Duplicate(InOuter));

    NewMaterial->materialInfo = materialInfo;

    return NewMaterial;
}


UMaterial* UMaterial::CreateMaterial(const FObjMaterialInfo& materialInfo)
{
    if (materialMap[materialInfo.MaterialName] != nullptr)
        return materialMap[materialInfo.MaterialName];

    UMaterial* newMaterial = FObjectFactory::ConstructObject<UMaterial>(nullptr); // Material은 Outer가 없이 따로 관리되는 객체이므로 Outer가 없음으로 설정. 추후 Garbage Collection이 추가되면 AssetManager를 생성해서 관리.
    newMaterial->SetMaterialInfo(materialInfo);
    materialMap.Add(materialInfo.MaterialName, newMaterial);

    // !TODO : 텍스쳐 로드 로직 나중에 변경
    GEngineLoop.ResourceManager.LoadTextureFromFile(GEngineLoop.GraphicDevice.Device, GEngineLoop.GraphicDevice.DeviceContext, materialInfo.DiffuseTexturePath.c_str());
    GEngineLoop.ResourceManager.LoadTextureFromFile(GEngineLoop.GraphicDevice.Device, GEngineLoop.GraphicDevice.DeviceContext, materialInfo.BumpTexturePath.c_str());
    GEngineLoop.ResourceManager.LoadTextureFromFile(GEngineLoop.GraphicDevice.Device, GEngineLoop.GraphicDevice.DeviceContext, materialInfo.SpecularTexturePath.c_str());
    GEngineLoop.ResourceManager.LoadTextureFromFile(GEngineLoop.GraphicDevice.Device, GEngineLoop.GraphicDevice.DeviceContext, materialInfo.AmbientTexturePath.c_str());
    GEngineLoop.ResourceManager.LoadTextureFromFile(GEngineLoop.GraphicDevice.Device, GEngineLoop.GraphicDevice.DeviceContext, materialInfo.AlphaTexturePath.c_str());


    return newMaterial;
}

UMaterial* UMaterial::GetMaterial(FString name)
{
    return materialMap[name];
}
