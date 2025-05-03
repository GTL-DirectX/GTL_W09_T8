#include "Wall.h"

#include "Bullet.h"
#include "PlayerCharacter.h"
#include "Audio/AudioManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ObjLoader.h"
#include "Components/LuaScriptComponent.h"
#include "Components/TextComponent.h"
#include "Components/Shapes/BoxComponent.h"
#include "Engine/Lua/LuaUtils/LuaTypeMacros.h"

AWall::AWall()
{
    StaticMeshComponent->SetStaticMesh(FObjManager::GetStaticMesh(L"Contents/SupplyBox/SupplyBox.obj"));
    StaticMeshComponent->SetRelativeScale3D(FVector(2, 2, 2));
    TextComponent = AddComponent<UTextComponent>("TEXTCOMPONENT_0");
    TextComponent->SetupAttachment(RootComponent);
    TextComponent->SetTexture(L"Assets/Texture/font.png");
    TextComponent->SetRowColumnCount(106, 106);
    TextComponent->SetText(L"");
    TextComponent->SetRelativeLocation(FVector(-2, 0, 2));

    BoxComponent = AddComponent<UBoxComponent>("BOXCOMP_0");
    BoxComponent->SetupAttachment(StaticMeshComponent);
    BoxComponent->SetRelativeLocation(FVector(0, 0.3, 1.2));
    BoxComponent->SetBoxExtent(FVector(0.8, 0.7, 0.6));

    // Initialize properties or components here
    SetActorLocation(FVector(20.0f, 0.0f, 0.0f));
}

UObject* AWall::Duplicate(UObject* InOuter)
{
    UObject* NewActor = Super::Duplicate(InOuter);
    AWall* PlayerCharacter = Cast<AWall>(NewActor);
    PlayerCharacter->TextComponent = GetComponentByFName<UTextComponent>("TEXTCOMPONENT_0");
    PlayerCharacter->BoxComponent = GetComponentByFName<UBoxComponent>("BOXCOMP_0");

    return NewActor;
}

void AWall::BeginPlay()
{
    Super::BeginPlay();
    OnActorBeginOverlapHandle = OnActorBeginOverlap.AddDynamic(this, &AWall::HandleOverlap);
}

void AWall::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AWall::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("Speed"), std::to_string(Speed));
}

void AWall::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("Speed"));
    if (TempStr)
    {
        Speed = std::stof(GetData(*TempStr));
    }
}

void AWall::HandleOverlap(AActor* OtherActor)
{
    if (IsActorBeingDestroyed())  
    {  
        return;  
    }

    UE_LOG(LogLevel::Display, "Handle Overlap %s,  %s", GetData(OtherActor->GetName()), GetData(GetName()));

    if (ABullet* Bullet = Cast<ABullet>(OtherActor))
    {
        if (LuaScriptComponent)
        {
            LuaScriptComponent->ActivateFunction("OnOverlapBullet", OtherActor);
        }

        AudioManager::Get().PlayOneShot(EAudioType::ShotBox);
    }
}

void AWall::RegisterLuaType(sol::state& Lua)
{
    DEFINE_LUA_TYPE_WITH_PARENT(AWall, sol::bases<AStaticMeshActor, AActor>(),
        "ActorLocation", sol::property(&ThisClass::GetActorLocation, &ThisClass::SetActorLocation),
        "Speed", sol::property(&ThisClass::GetSpeed, &ThisClass::SetSpeed),
        "VarientValue", sol::property(&ThisClass::GetVarientValue, &ThisClass::SetVarientValue)
    )
}

bool AWall::BindSelfLuaProperties()
{
    Super::BindSelfLuaProperties();
    sol::table& LuaTable = LuaScriptComponent->GetLuaSelfTable();
    if (!LuaTable.valid())
    {
        return false;
    }
    LuaTable["this"] = this;

    return true;
}

