#pragma once

#include "EditorViewportClient.h"

class FSkeletalMeshViewportClient : public FEditorViewportClient
{
public:
    FSkeletalMeshViewportClient();

    virtual void Initialize(EViewScreenLocation InViewportIndex, const FRect& InRect, UWorld* InWorld) override;

    virtual void Tick(float DeltaTime);


};

