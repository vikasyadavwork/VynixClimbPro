#include "EndlessClimber.h"
#include "ClimbRunRules.h"
#include "ClimbSaveGame.h"
#include "EndlessClimbWorld.h"
#include "ClimbHandIKAnimInstance.h"
#include "GroomComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace { const TCHAR* SaveSlot = TEXT("VynixClimbBestRun"); }

AEndlessClimber::AEndlessClimber(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(36.f, 96.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    GetCharacterMovement()->GravityScale = 0.f;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    bUseControllerRotationYaw = false;
    GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -96), FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
    if (Quinn.Succeeded()) GetMesh()->SetSkeletalMesh(Quinn.Object);
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Hang(TEXT("/Game/new_animation/exported/Hang_Idle.Hang_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Left(TEXT("/Game/new_animation/exported/JumpLeft.JumpLeft"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Right(TEXT("/Game/new_animation/exported/JumpRight.JumpRight"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Up(TEXT("/Game/new_animation/exported/Jump_Up.Jump_Up"));
    HangAnimation = Hang.Object;
    LeftAnimation = Left.Object;
    RightAnimation = Right.Object;
    UpAnimation = Up.Object;

    ArcadeCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ClimbArcadeCameraBoom"));
    ArcadeCameraBoom->SetupAttachment(RootComponent);
    ArcadeCameraBoom->TargetArmLength = 1250.f;
    ArcadeCameraBoom->SetUsingAbsoluteRotation(true);
    ArcadeCameraBoom->SetRelativeRotation(FRotator(0, 0, 0));
    ArcadeCameraBoom->TargetOffset = FVector(0, 0, 190);
    ArcadeCameraBoom->bDoCollisionTest = false;
    ArcadeCameraBoom->bEnableCameraLag = true;
    ArcadeCameraBoom->CameraLagSpeed = 5.f;
    ArcadeCameraBoom->CameraLagMaxDistance = 220.f;
    ArcadeCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ClimbCamera"));
    ArcadeCamera->SetupAttachment(ArcadeCameraBoom, USpringArmComponent::SocketName);
    ArcadeCamera->FieldOfView = 62.f;
    ArcadeCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    ArcadeCamera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    ArcadeCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
    ArcadeCamera->PostProcessSettings.AutoExposureBias = 2.f;
    ArcadeCamera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    ArcadeCamera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    ArcadeCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    ArcadeCamera->PostProcessSettings.MotionBlurAmount = 0.f;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for (int32 Index = 0; Index < 10; ++Index)
    {
        UStaticMeshComponent* Chalk = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Chalk_%d"), Index));
        Chalk->SetupAttachment(RootComponent);
        Chalk->SetUsingAbsoluteLocation(true);
        Chalk->SetStaticMesh(Sphere.Object);
        Chalk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Chalk->SetCastShadow(false);
        Chalk->SetVisibility(false);
        ChalkTrail.Add(Chalk);
        ChalkLife.Add(0.f);
    }
}

void AEndlessClimber::BeginPlay()
{
    // Keep the original Blueprint's cosmetic construction, but use only arcade input/movement.
    ACharacter::BeginPlay();
    SetActorLocation(RunOrigin + FVector(0, ClimbRunRules::LaneY(CurrentLane), ClimbRunRules::StartingHeight));
    SetActorRotation(FRotator::ZeroRotator);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->SetComponentTickEnabled(false);
    TInlineComponentArray<UCameraComponent*> Cameras(this);
    for (UCameraComponent* Camera : Cameras) Camera->SetActive(Camera == ArcadeCamera);
    // Use Payton's supplied hair cards for the arcade camera and close views alike.
    // They avoid the legacy strand LOD disappearing on lower-end graphics hardware.
    TInlineComponentArray<UGroomComponent*> Grooms(this);
    for (UGroomComponent* Groom : Grooms)
        if (Groom->GetFName() == TEXT("Hair")) Groom->SetUseCards(true);
    RestMeshRotation = GetMesh()->GetRelativeRotation();
    IdleSeconds = 1.f;
    PlayClimbAnimation(HangAnimation, true);
    GetMesh()->TickAnimation(0.f, false);
    GetMesh()->RefreshBoneTransforms();
    if (GetMesh()->DoesSocketExist(TEXT("pelvis")))
        HangPelvisOffset = GetMesh()->GetSocketLocation(TEXT("pelvis")) - GetActorLocation();
    if (GetMesh()->DoesSocketExist(TEXT("hand_l")) && GetMesh()->DoesSocketExist(TEXT("hand_r")))
    {
        const FVector Hands = (GetMesh()->GetSocketLocation(TEXT("hand_l"))
            + GetMesh()->GetSocketLocation(TEXT("hand_r"))) * 0.5;
        HangHandHeight = Hands.Z - GetActorLocation().Z;
    }
    bGripReady = true;
    if (UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
    {
        if (UClimbSaveGame* Saved = Cast<UClimbSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
        {
            BestScore = FMath::Max(0, Saved->BestScore);
            BestHeight = FMath::Max(0.f, Saved->BestHeight);
        }
    }
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->SetViewTarget(this);
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
    }
}

void AEndlessClimber::SetupPlayerInputComponent(UInputComponent* Input)
{
    ACharacter::SetupPlayerInputComponent(Input);
    Input->BindKey(EKeys::A, IE_Pressed, this, &AEndlessClimber::LeapLeft);
    Input->BindKey(EKeys::Left, IE_Pressed, this, &AEndlessClimber::LeapLeft);
    Input->BindKey(EKeys::D, IE_Pressed, this, &AEndlessClimber::LeapRight);
    Input->BindKey(EKeys::Right, IE_Pressed, this, &AEndlessClimber::LeapRight);
    Input->BindKey(EKeys::W, IE_Pressed, this, &AEndlessClimber::LeapUp);
    Input->BindKey(EKeys::Up, IE_Pressed, this, &AEndlessClimber::LeapUp);
    Input->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AEndlessClimber::LeapUp);
    Input->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AEndlessClimber::LeapLeft);
    Input->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &AEndlessClimber::LeapRight);
    Input->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AEndlessClimber::GamepadConfirm).bExecuteWhenPaused = true;
    Input->BindKey(EKeys::P, IE_Pressed, this, &AEndlessClimber::ToggleRunPause).bExecuteWhenPaused = true;
    Input->BindKey(EKeys::Escape, IE_Pressed, this, &AEndlessClimber::ToggleRunPause).bExecuteWhenPaused = true;
    Input->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AEndlessClimber::ToggleRunPause).bExecuteWhenPaused = true;
    Input->BindKey(EKeys::R, IE_Pressed, this, &AEndlessClimber::RestartRun).bExecuteWhenPaused = true;
    Input->BindTouch(IE_Pressed, this, &AEndlessClimber::TouchPressed);
}

void AEndlessClimber::LeapLeft() { RequestDodge(0); }
void AEndlessClimber::LeapRight() { RequestDodge(1); }
void AEndlessClimber::LeapUp() { LeapToLane(CurrentLane); }

bool AEndlessClimber::CalculateGrip(const FBox& Bounds, float DesiredY, FVector& OutLocation) const
{
    return ClimbRunRules::TryHangLocation(Bounds, DesiredY,
        GetCapsuleComponent()->GetScaledCapsuleRadius(), GripClearance, HangHandHeight, OutLocation);
}

bool AEndlessClimber::GetHandLedgeBounds(FBox& OutBounds) const
{
    return !bLeaping && ClimbArena.IsValid()
        && ClimbArena->GetLedgeBounds(CompletedJumps, CurrentLane, OutBounds);
}

bool AEndlessClimber::GetJumpLedgeBounds(FBox& OutBounds) const
{
    FBox Departure;
    if (!bLeaping || !ClimbArena.IsValid()
        || !ClimbArena->GetLedgeBounds(CompletedJumps, CurrentLane, Departure)
        || !ClimbArena->GetLedgeBounds(CompletedJumps + 1, TargetLane, OutBounds)) return false;
    // Clear both grips if their depth differs.
    OutBounds.Min.X = FMath::Min(Departure.Min.X, OutBounds.Min.X);
    return true;
}

void AEndlessClimber::AttachToArena(AEndlessClimbWorld* Arena)
{
    ClimbArena = Arena;
    FVector Grip;
    if (Arena && Arena->GetGripLocation(CompletedJumps, CurrentLane, Grip)) SetActorLocation(Grip);
}

void AEndlessClimber::GamepadConfirm()
{
    if (bRunOver) RestartRun();
    else if (bRunPaused) ToggleRunPause();
    else LeapUp();
}

void AEndlessClimber::RequestDodge(int32 Lane)
{
    if (bRunOver || bRunPaused || Lane < 0 || Lane > 1) return;
    if (bLeaping || IdleSeconds < 0.1f)
    {
        BufferedLane = Lane;
        BufferedDodgeSeconds = 0.8f;
    }
    else
    {
        BufferedLane = -1;
        LeapToLane(Lane);
    }
}

void AEndlessClimber::TouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
    if (FingerIndex != ETouchIndex::Touch1 || bRunOver || bRunPaused) return;
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        int32 Width, Height;
        PC->GetViewportSize(Width, Height);
        if (Location.Y > Height * 0.28f) RequestDodge(Location.X < Width * 0.5f ? 0 : 1);
    }
}

void AEndlessClimber::LeapToLane(int32 Lane)
{
    if (bLeaping || bRunOver || bRunPaused || Lane < 0 || Lane > 1 || IdleSeconds < 0.10f) return;
    FVector Grip;
    if (!ClimbArena.IsValid() || !ClimbArena->GetGripLocation(CompletedJumps + 1, Lane, Grip)) return;
    if (IdleSeconds > 2.2f) Combo = 0;
    TargetLane = Lane;
    JumpStart = GetActorLocation();
    JumpTarget = Grip;
    JumpElapsed = 0.f;
    bLeaping = true;
    JumpFeedback = 1.f;
    UAnimSequence* Animation = Lane == CurrentLane ? UpAnimation : (Lane == 0 ? LeftAnimation : RightAnimation);
    PlayClimbAnimation(Animation, false);
}

void AEndlessClimber::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bRunPaused) return;
    DamageFeedback = FMath::Max(0.f, DamageFeedback - DeltaSeconds * 1.8f);
    JumpFeedback = FMath::Max(0.f, JumpFeedback - DeltaSeconds * 1.5f);
    InvulnerableSeconds = FMath::Max(0.f, InvulnerableSeconds - DeltaSeconds);
    UpdateJumpEffects(DeltaSeconds);
    if (bRunOver) return;

    SurvivalSeconds += DeltaSeconds;
    BufferedDodgeSeconds = FMath::Max(0.f, BufferedDodgeSeconds - DeltaSeconds);
    if (BufferedDodgeSeconds <= 0.f) BufferedLane = -1;
    SaveElapsed += DeltaSeconds;
    if (SaveElapsed >= 20.f) { SaveElapsed = 0.f; SaveBestRun(); }
    if (bLeaping)
    {
        JumpElapsed += DeltaSeconds;
        const float Alpha = JumpElapsed / ClimbRunRules::JumpDuration;
        SetActorLocation(ClimbRunRules::JumpPosition(JumpStart, JumpTarget, Alpha), false);
        if (Alpha >= 1.f) FinishLeap();
    }
    else
    {
        IdleSeconds += DeltaSeconds;
        if (IdleSeconds > 2.2f) Combo = 0;
        if (BufferedLane >= 0 && IdleSeconds >= 0.1f)
        {
            const int32 Lane = BufferedLane;
            BufferedLane = -1;
            LeapToLane(Lane);
        }
        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) || PC->IsInputKeyDown(EKeys::SpaceBar)) LeapUp();
        }
    }
    const float Shake = FMath::Sin(GetWorld()->GetTimeSeconds() * 65.f) * DamageFeedback * 7.f;
    ArcadeCameraBoom->SocketOffset = FVector(0, Shake, JumpFeedback * 10.f);
    ArcadeCamera->SetFieldOfView(FMath::FInterpTo(ArcadeCamera->FieldOfView, bLeaping ? 65.f : 62.f, DeltaSeconds, 6.f));
}

void AEndlessClimber::FinishLeap()
{
    SetActorLocation(JumpTarget);
    bLeaping = false;
    CurrentLane = TargetLane;
    ++CompletedJumps;
    Combo = FMath::Min(Combo + 1, 10);
    const int64 NewBonus = static_cast<int64>(JumpBonus) + 10 * Combo;
    JumpBonus = static_cast<int32>(FMath::Min<int64>(NewBonus, MAX_int32));
    HeightMetres = static_cast<double>(CompletedJumps) * ClimbRunRules::StepHeight / 100.0;
    IdleSeconds = 0.f;
    GetMesh()->SetRelativeRotation(RestMeshRotation);
    PlayClimbAnimation(HangAnimation, true);
    if (CompletedJumps % 25 == 0) Health = FMath::Min(100.f, Health + 10.f);
}

void AEndlessClimber::PlayClimbAnimation(UAnimSequence* Animation, bool bLoop)
{
    if (!Animation) return;
    Animation->bForceRootLock = true;
    if (!Cast<UClimbHandIKAnimInstance>(GetMesh()->GetAnimInstance()))
        GetMesh()->SetAnimInstanceClass(UClimbHandIKAnimInstance::StaticClass());
    if (UAnimSingleNodeInstance* Instance = GetMesh()->GetSingleNodeInstance())
    {
        Instance->SetAnimationAsset(Animation, bLoop, bLoop ? 1.f : Animation->GetPlayLength() / ClimbRunRules::JumpDuration);
        Instance->SetPosition(0.f, false);
        Instance->SetPlaying(true);
        // The capsule owns travel; root motion must not move the mesh away from it.
        Instance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
        Instance->SetPlayRate(bLoop ? 1.f : Animation->GetPlayLength() / ClimbRunRules::JumpDuration);
    }
}

void AEndlessClimber::UpdateJumpEffects(float DeltaSeconds)
{
    ChalkElapsed += DeltaSeconds;
    if (bLeaping && !bRunOver && ChalkElapsed >= 0.045f)
    {
        ChalkElapsed = 0.f;
        UStaticMeshComponent* Chalk = ChalkTrail[NextChalk];
        Chalk->SetWorldLocation(GetActorLocation() + FVector(-8, 0, -48));
        Chalk->SetVisibility(true);
        ChalkLife[NextChalk] = 0.5f;
        NextChalk = (NextChalk + 1) % ChalkTrail.Num();
    }
    for (int32 Index = 0; Index < ChalkTrail.Num(); ++Index)
    {
        ChalkLife[Index] = FMath::Max(0.f, ChalkLife[Index] - DeltaSeconds);
        ChalkTrail[Index]->SetWorldScale3D(FVector(0.055f * ChalkLife[Index] / 0.5f));
        ChalkTrail[Index]->SetVisibility(ChalkLife[Index] > 0.f);
        if (ChalkLife[Index] > 0.f) ChalkTrail[Index]->AddWorldOffset(FVector(-4, 0, -28) * DeltaSeconds);
    }
}

int32 AEndlessClimber::GetScore() const { return ClimbRunRules::Score(HeightMetres, SurvivalSeconds, JumpBonus); }

float AEndlessClimber::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* Causer)
{
    const float Before = Health;
    ApplyRockHit(DamageAmount);
    return Before - Health;
}

void AEndlessClimber::ApplyRockHit(float Damage)
{
    if (bRunOver || bRunPaused || InvulnerableSeconds > 0.f || !FMath::IsFinite(Damage) || Damage <= 0.f) return;
    Health = ClimbRunRules::HealthAfterHit(Health, Damage);
    DamageFeedback = 1.f;
    InvulnerableSeconds = ClimbRunRules::HitGraceSeconds;
    Combo = 0;
    if (Health <= 0.f)
    {
        bRunOver = true;
        bLeaping = false;
        GetMesh()->SetRelativeRotation(RestMeshRotation);
        PlayClimbAnimation(HangAnimation, true);
        SaveBestRun();
    }
}

void AEndlessClimber::SaveBestRun()
{
    if (!GetWorld() || !GetWorld()->IsGameWorld()) return;
    if (FParse::Param(FCommandLine::Get(), TEXT("VynixCapture"))) return;
    const int32 Score = GetScore();
    if (Score <= BestScore && HeightMetres <= BestHeight) return;
    if (UClimbSaveGame* Saved = Cast<UClimbSaveGame>(UGameplayStatics::CreateSaveGameObject(UClimbSaveGame::StaticClass())))
    {
        Saved->BestScore = FMath::Max(BestScore, Score);
        Saved->BestHeight = FMath::Max(BestHeight, static_cast<float>(HeightMetres));
        if (UGameplayStatics::SaveGameToSlot(Saved, SaveSlot, 0))
        {
            BestScore = Saved->BestScore;
            BestHeight = Saved->BestHeight;
        }
        else UE_LOG(LogTemp, Warning, TEXT("Vynix: could not save the best run; will retry."));
    }
}

void AEndlessClimber::ToggleRunPause()
{
    if (bRunOver) return;
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        const bool bWantPause = !bRunPaused;
        if (PC->SetPause(bWantPause))
        {
            bRunPaused = bWantPause;
            BufferedLane = -1;
        }
    }
}

void AEndlessClimber::RestartRun()
{
    SaveBestRun();
    UGameplayStatics::SetGamePaused(this, false);
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void AEndlessClimber::EndPlay(const EEndPlayReason::Type Reason)
{
    SaveBestRun();
    Super::EndPlay(Reason);
}
