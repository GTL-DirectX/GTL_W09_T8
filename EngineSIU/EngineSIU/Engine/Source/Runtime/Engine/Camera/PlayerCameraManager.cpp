#include "PlayerCameraManager.h"

#include "World/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Engine/Lua/LuaUtils/LuaTypeMacros.h"
#include "Components/LuaScriptComponent.h"
#include "CameraModifier.h"

#include "CameraModifier.h"
#include "Actors/CameraActor.h"
#include "Camera/CameraComponent.h"

void FViewTarget::SetNewTarget(AActor* NewTarget)
{
    Target = NewTarget;
}

APawn* FViewTarget::GetTargetPawn() const
{
    if (APawn* Pawn = Cast<APawn>(Target))
    {
        return Pawn;
    }
    else if (AController* PC = Cast<AController>(Target))
    {
        return PC->GetPawn();
    }
    else
    {
        return nullptr;
    }
}

bool FViewTarget::Equals(const FViewTarget& Other) const
{
    return Target == Other.Target && POV.Equals(Other.POV);
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{
    if (!OwningController)
    {
        assert(false && TEXT("OwningController is null"));
    }

    if (Target == nullptr)
    {
        Target = OwningController;
    }

    // TODO: PlayerState와 관련된 부분은 생략. 추후 PlayerState가 생기면 추가로 구현 필수.
    //// Update ViewTarget PlayerState (used to follow same player through pawn transitions, etc., when spectating)
    //if (Target == OwningController)
    //{
    //    PlayerState = NULL;
    //}
    //else if (AController* TargetAsController = Cast<AController>(Target))
    //{
    //    PlayerState = TargetAsController->PlayerState;
    //}
    //else if (APawn* TargetAsPawn = Cast<APawn>(Target))
    //{
    //    PlayerState = TargetAsPawn->GetPlayerState();
    //}
    //else if (APlayerState* TargetAsPlayerState = Cast<APlayerState>(Target))
    //{
    //    PlayerState = TargetAsPlayerState;
    //}
    //else
    //{
    //    PlayerState = NULL;
    //}

    if (Target)
    {
        // 원래는 PlayerState가 되어야 함.
        if (OwningController->GetPawn())
        {
            OwningController->PlayerCameraManager->AssignViewTarget(OwningController->GetPawn(), *this);
        }
        else
        {
            Target = OwningController;
        }
    }
}

float FViewTargetTransitionParams::GetBlendAlpha(const float& TimePct) const
{
    switch (BlendFunction)
    {
    case EViewTargetBlendFunction::VTBlend_Linear: return FMath::Lerp(0.f, 1.f, TimePct);
    case EViewTargetBlendFunction::VTBlend_Cubic:	return FMath::CubicInterp(0.f, 0.f, 1.f, 0.f, TimePct);
    case EViewTargetBlendFunction::VTBlend_EaseInOut: return FMath::InterpEaseInOut(0.f, 1.f, TimePct, BlendExp);
    case EViewTargetBlendFunction::VTBlend_EaseIn: return FMath::Lerp(0.f, 1.f, FMath::Pow(TimePct, BlendExp));
    case EViewTargetBlendFunction::VTBlend_EaseOut: return FMath::Lerp(0.f, 1.f, FMath::Pow(TimePct, (FMath::IsNearlyZero(BlendExp) ? 1.f : (1.f / BlendExp))));
    default:
        break;
    }
    return 1.f;
}

void APlayerCameraManager::PostSpawnInitialize()
{
    Super::PostSpawnInitialize();
    ViewTarget.POV.Location = FVector(-3.0f, 0.0f, 3.0f);
}

UObject* APlayerCameraManager::Duplicate(UObject* InOuter)
{
    APlayerCameraManager* NewCameraManager = Cast<APlayerCameraManager>(Super::Duplicate(InOuter));


    return NewCameraManager;
}

void APlayerCameraManager::AddModifier(UCameraModifier* Modifier)
{
    if (Modifier)
    {
        Modifier->CameraOwner = this;
        Modifier->bDisabled = false;
        ModifierList.Add(Modifier);
    }
}

void APlayerCameraManager::RegisterLuaType(sol::state& Lua)
{
    DEFINE_LUA_TYPE_WITH_PARENT(APlayerCameraManager, sol::bases<AActor>());

}

bool APlayerCameraManager::BindSelfLuaProperties()
{
    if (!Super::BindSelfLuaProperties())
        return false;

    LuaScriptComponent->GetLuaSelfTable()["this"] = this;

    return true;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    FMinimalViewInfo NewPOV = ViewTarget.POV;

    if (PendingViewTarget.Target == NULL)
    {
        ViewTarget.CheckViewTarget(PCOwner);
        UpdateViewTarget(ViewTarget, DeltaTime);
    }

    if (PendingViewTarget.Target != NULL)
    {
        BlendTimeToGo -= DeltaTime;

        ViewTarget.CheckViewTarget(PCOwner);
        UpdateViewTarget(PendingViewTarget, DeltaTime);

        if (BlendTimeToGo > 0)
        {
            // Blend 로직 추가.

            float DurationPct = (BlendParams.BlendTime - BlendTimeToGo) / BlendParams.BlendTime;
            float BlendPct = 0.f;
            switch (BlendParams.BlendFunction)
            {
            case EViewTargetBlendFunction::VTBlend_Linear:
                BlendPct = FMath::Lerp(0.f, 1.f, DurationPct);
                break;
            case EViewTargetBlendFunction::VTBlend_Cubic:
                BlendPct = FMath::CubicInterp(0.f, 0.f, 1.f, 0.f, DurationPct);
                break;
            case EViewTargetBlendFunction::VTBlend_EaseIn:
                BlendPct = FMath::Lerp(0.f, 1.f, FMath::Pow(DurationPct, BlendParams.BlendExp));
                break;
            case EViewTargetBlendFunction::VTBlend_EaseOut:
                BlendPct = FMath::Lerp(0.f, 1.f, FMath::Pow(DurationPct, 1.f / BlendParams.BlendExp));
                break;
            case EViewTargetBlendFunction::VTBlend_EaseInOut:
                BlendPct = FMath::InterpEaseInOut(0.f, 1.f, DurationPct, BlendParams.BlendExp);
                break;
            case EViewTargetBlendFunction::VTBlend_PreBlended:
                BlendPct = 1.0f;
                break;
            default:
                break;
            }

            // Update pending view target blend
            NewPOV = ViewTarget.POV;
            NewPOV.BlendViewInfo(PendingViewTarget.POV, BlendPct);
        }
        else
        {
            ViewTarget = PendingViewTarget;
            PendingViewTarget.Target = NULL;
            BlendTimeToGo = 0;
            NewPOV = PendingViewTarget.POV;
        }
    }

    if (bEnableFading)
    {
        FadeTimeRemaining = FMath::Max(FadeTimeRemaining - DeltaTime, 0.0f);
        if (FadeTime > 0.0f)
        {
            FadeAmount = FadeAlpha.X + ((1.f - FadeTimeRemaining / FadeTime) * (FadeAlpha.Y - FadeAlpha.X));
        }

        if ((bHoldFadeWhenFinished == false) && (FadeTimeRemaining <= 0.f))
        {
            // done
            StopCameraFade();
        }
    }

    FillCameraCache(NewPOV);
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& OutVT, float DeltaTime)
{
    if ((PendingViewTarget.Target == nullptr) && BlendParams.bLockOutgoing && OutVT.Equals(ViewTarget))
    {
        return;
    }

    // Store previous POV, in case we need it later
    FMinimalViewInfo OrigPOV = OutVT.POV;

    // Reset the view target POV fully
    static const FMinimalViewInfo DefaultViewInfo;
    OutVT.POV = DefaultViewInfo;
    OutVT.POV.FOV = DefaultFOV;
    OutVT.POV.OrthoWidth = DefaultOrthoWidth;
    OutVT.POV.AspectRatio = DefaultAspectRatio;
    //OutVT.POV.bConstrainAspectRatio = bDefaultConstrainAspectRatio;
    OutVT.POV.ProjectionMode = bIsOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
    //OutVT.POV.PostProcessBlendWeight = 1.0f;
    //OutVT.POV.bAutoCalculateOrthoPlanes = bAutoCalculateOrthoPlanes;
    //OutVT.POV.AutoPlaneShift = AutoPlaneShift;
    //OutVT.POV.bUpdateOrthoPlanes = bUpdateOrthoPlanes;
    //OutVT.POV.bUseCameraHeightAsViewTarget = bUseCameraHeightAsViewTarget;

    if (ACameraActor* CamActor = Cast<ACameraActor>(OutVT.Target))
    {
        // Viewing through a camera actor.
        // Target 액터가 카메라 액터이고.
        // 액터가 카메라 컴포넌트를 가지고 있을 때.
        CamActor->GetCameraComponent()->GetCameraView(DeltaTime, OutVT.POV);
    }
    else if (AController* PC = Cast<AController>(OutVT.Target))
    {
        // Viewing through a controller.
        // Target 액터가 컨트롤러일 때.
        if (APawn* Pawn = PC->GetPawn())
        {
            OutVT.POV.Location = Pawn->GetActorLocation();
            OutVT.POV.Rotation = Pawn->GetActorRotation();
            OutVT.POV.FOV = 90.0f;
        }
        else
        {
            OutVT.POV.Location = PC->GetActorLocation();
            OutVT.POV.Rotation = PC->GetActorRotation();
            OutVT.POV.FOV = 90.0f;
        }
    }
    else
    {
        if (UCameraComponent* CamComp = OutVT.Target->GetComponentByClass<UCameraComponent>())
        {
            OutVT.POV.Location = CamComp->GetWorldLocation();
            OutVT.POV.Rotation = CamComp->GetWorldRotation();
            OutVT.POV.FOV = CamComp->GetFieldOfView();
        }
    }

    ApplyCameraModifiers(DeltaTime, OutVT.POV);

    TArray<UCameraModifier*> RemoveModifierList;
    for (UCameraModifier* Modifier : ModifierList)
    {
        if (Modifier && Modifier->IsDisabled())
        {
            RemoveModifierList.Add(Modifier);
        }
    }
    for (UCameraModifier* Modifier : RemoveModifierList)
    {
        RemoveCameraModifier(Modifier);
    }
}

bool APlayerCameraManager::RemoveCameraModifier(UCameraModifier* ModifierToRemove)
{
    if (ModifierToRemove)
    {
        ModifierList.Remove(ModifierToRemove);
        return true;
    }
    return false;
}

void APlayerCameraManager::SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
    if (NewViewTarget == nullptr)
    {
        return;
    }

    ViewTarget.CheckViewTarget(PCOwner);
    if (PendingViewTarget.Target)
    {
        PendingViewTarget.CheckViewTarget(PCOwner);
    }

    // If we're already transitioning to this new target, don't interrupt.
    if (PendingViewTarget.Target != NULL && NewViewTarget == PendingViewTarget.Target)
    {
        return;
    }

    if ((NewViewTarget != ViewTarget.Target) /*|| (PendingViewTarget.Target && BlendParams.bLockOutgoing)*/)
    {
        // if a transition time is specified, then set pending view target accordingly
        if (TransitionParams.BlendTime > 0)
        {
            // band-aid fix so that EndViewTarget() gets called properly in this case
            if (PendingViewTarget.Target == NULL)
            {
                PendingViewTarget.Target = ViewTarget.Target;
            }

            // use last frame's POV
            ViewTarget.POV = GetLastFrameCameraCacheView();
            BlendTimeToGo = TransitionParams.BlendTime;

            AssignViewTarget(NewViewTarget, PendingViewTarget, TransitionParams);
            PendingViewTarget.CheckViewTarget(PCOwner);

        }
        else
        {
            // otherwise, assign new viewtarget instantly
            AssignViewTarget(NewViewTarget, ViewTarget);
            ViewTarget.CheckViewTarget(PCOwner);
            // remove old pending ViewTarget so we don't still try to switch to it
            PendingViewTarget.Target = NULL;
        }
    }

    BlendParams = TransitionParams;
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    for (UCameraModifier* Modifier : ModifierList)
    {
        if (Modifier)
        {
            Modifier->ModifyCamera(DeltaTime, InOutPOV);
        }
    }
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
    FMinimalViewInfo DefaultFOVCache = GetCameraCacheView();
    DefaultFOVCache.FOV = DefaultFOV;
    SetCameraCachePOV(DefaultFOVCache);

    PCOwner = PC;

    SetViewTarget(PCOwner);

    // set the level default scale
    //SetDesiredColorScale(GetWorldSettings()->DefaultColorScale, 5.f);

    // Force camera update so it doesn't sit at (0,0,0) for a full tick.
    // This can have side effects with streaming.
    UpdateCamera(0.f);
}

void APlayerCameraManager::SetCameraCachePOV(const FMinimalViewInfo& InPOV)
{
    CameraCachePrivate.POV = InPOV;
}

void APlayerCameraManager::SetLastFrameCameraCachePOV(const FMinimalViewInfo& InPOV)
{
    LastFrameCameraCachePrivate.POV = InPOV;
}

const FMinimalViewInfo& APlayerCameraManager::GetCameraCacheView() const
{
    return CameraCachePrivate.POV;
}

const FMinimalViewInfo& APlayerCameraManager::GetLastFrameCameraCacheView() const
{
    return LastFrameCameraCachePrivate.POV;
}

FMinimalViewInfo APlayerCameraManager::GetCameraCachePOV() const
{
    return GetCameraCacheView();
}

FMinimalViewInfo APlayerCameraManager::GetLastFrameCameraCachePOV() const
{
    return GetLastFrameCameraCacheView();
}

void APlayerCameraManager::FillCameraCache(const FMinimalViewInfo& NewInfo)
{
    // Backup last frame results.
    const float CurrentCacheTime = GetCameraCacheTime();
    const float CurrentGameTime = GetWorld()->TimeSeconds;
    if (CurrentCacheTime != CurrentGameTime)
    {
        SetLastFrameCameraCachePOV(GetCameraCacheView());
        SetLastFrameCameraCacheTime(CurrentCacheTime);
    }

    SetCameraCachePOV(NewInfo);
    SetCameraCacheTime(CurrentGameTime);
}

void APlayerCameraManager::AssignViewTarget(AActor* NewTarget, FViewTarget& VT, struct FViewTargetTransitionParams InTransitionParams)
{
    if (!NewTarget)
    {
        return;
    }

    if (NewTarget == VT.Target)
    {
        return;
    }

    AActor* OldViewTarget = VT.Target;
    VT.Target = NewTarget; // 핵심은 이 줄. 현재 타겟을 뉴 타겟으로 바꿔줌.

    VT.POV.AspectRatio = DefaultAspectRatio;
    VT.POV.FOV = DefaultFOV;
    BlendParams = InTransitionParams;

    if (OldViewTarget)
    {
        // ViewTarget 없어졌다는 이벤트 호출.
        // OldViewTarget->OnEnd(PCOwner);
    }

    // 새 ViewTarget에 대한 이벤트 호출.
    //VT.Target->BecomeViewTarget(PCOwner);
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float InFadeTime, FLinearColor InFadeColor, bool bInFadeAudio, bool bInHoldWhenFinished)
{
    bEnableFading = true;

    FadeColor = InFadeColor;
    FadeAlpha = FVector2D(FromAlpha, ToAlpha);
    FadeTime = InFadeTime;
    FadeTimeRemaining = InFadeTime;

    //bAutoAnimateFade = true;
    bHoldFadeWhenFinished = bInHoldWhenFinished;
}

void APlayerCameraManager::StopCameraFade()
{
    if (bEnableFading == true)
    {
        // Make sure FadeAmount finishes at the desired value
        FadeAmount = FadeAlpha.Y;
        bEnableFading = false;
    }
}
