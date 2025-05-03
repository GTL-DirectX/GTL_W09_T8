#pragma once

#include "Classes/Engine/StaticMeshActor.h"

class USkeletalMeshCompnent;

class ACube : public AStaticMeshActor
{
    DECLARE_CLASS(ACube, AStaticMeshActor)

public:
    ACube();

    USkeletalMeshCompnent* test;
};

