#include "Cube.h"

#include "Components/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Shapes/BoxComponent.h"
#include "Components/Shapes/CapsuleComponent.h"
#include "Components/Shapes/SphereComponent.h"

#include "Engine/ObjLoader.h"

ACube::ACube()
{
    StaticMeshComponent->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/Reference/Reference.obj"));
    
    // TODO 임시 코드임. (Class Table에 생성되지 않으면 안넣어짐.)
    auto BoxComponent = AddComponent<UBoxComponent>();
    BoxComponent->DestroyComponent();

    auto SphereComponent = AddComponent<USphereComponent>();
    SphereComponent->DestroyComponent();

    auto CapsuleComponent = AddComponent<UCapsuleComponent>();
    CapsuleComponent->DestroyComponent();

    auto SpringArmComponent = AddComponent<USpringArmComponent>();
    SpringArmComponent->DestroyComponent();
    
    // End Test
}
