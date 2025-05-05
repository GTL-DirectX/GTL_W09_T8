#pragma once

#include "Classes/Engine/StaticMeshActor.h"

class USkeletalMeshComponent;

class ACube : public AStaticMeshActor
{
    DECLARE_CLASS(ACube, AStaticMeshActor)

public:
    ACube();

    USkeletalMeshComponent* test;
};

