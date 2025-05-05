#pragma once

#include "Rendering/Texture/Texture.h"

class FTextureManager
{
    TMap<FString, UTexture*> TextureMap;

public:
    FTextureManager();

    UTexture* LoadTexture(const FString& InFileName);

};

