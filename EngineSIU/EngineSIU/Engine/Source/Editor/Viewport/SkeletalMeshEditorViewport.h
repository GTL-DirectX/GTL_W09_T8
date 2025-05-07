#pragma once

#include "EditorViewport.h"

class UWorld;
class AActor;
class USkeletalMesh;
class USkeletalMeshComponent;

class FSkeletalMeshEditorViewport : public FEditorViewport
{
public:
    FSkeletalMeshEditorViewport() = default;
    FSkeletalMeshEditorViewport(EViewScreenLocation InViewLocation);

    virtual void Initialize(const FRect& InRect) override;
    void SetWorld(UWorld* InWorld) { World = InWorld; }
    virtual void Draw(float DeltaTime) override;

    void SetShowViewport(bool bShow) { bShowViewport = bShow; }

    void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);

private:
    UWorld* World = nullptr;
    AActor* ViewerActor = nullptr;
    USkeletalMesh* SkeletalMesh = nullptr;
    USkeletalMeshComponent* SkeletalMeshComponent = nullptr;

    bool bShowViewport = true;
    
};

