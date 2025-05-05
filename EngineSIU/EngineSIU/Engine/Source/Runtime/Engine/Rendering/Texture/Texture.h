#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UTexture : public UObject
{
    DECLARE_CLASS(UTexture, UObject)

public:
    UTexture() = default;

    void SetTextureName(const FString& InTextureName) { TextureName = InTextureName; }
    FString GetTextureName() const { return TextureName; }
private:
    FString TextureName;

};

