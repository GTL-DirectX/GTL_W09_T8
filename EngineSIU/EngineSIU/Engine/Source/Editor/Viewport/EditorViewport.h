#pragma once

#include "Viewport.h"

class FEditorViewport : public FViewport
{
public:
    FEditorViewport() = default;
    FEditorViewport(EViewScreenLocation InViewLocation);

    void SetViewportName(const FString& InViewportName) { ViewportName = InViewportName; }

private:
    FString ViewportName;

};

