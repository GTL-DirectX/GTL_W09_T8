#include "TextureManager.h"

#include "UObject/ObjectFactory.h"
#include "Engine/AssetManager.h"

FTextureManager::FTextureManager()
    : TextureMap()
{
}

UTexture* FTextureManager::LoadTexture(const FString& InFileName)
{
    if (InFileName.IsEmpty())
    {
        return nullptr;
    }
    if (TextureMap.Contains(InFileName))
    {
        return *TextureMap.Find(InFileName);
    }

    UTexture* NewTexture = FObjectFactory::ConstructObject<UTexture>(&UAssetManager::Get());
    NewTexture->SetTextureName(InFileName);
    TextureMap.Add(InFileName, NewTexture);

    return *TextureMap.Find(InFileName);
}
