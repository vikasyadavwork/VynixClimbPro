#include "ClimbCapture.h"

#if !UE_BUILD_SHIPPING
#include "EndlessClimber.h"
#include "EndlessClimbWorld.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif
#endif

void StartClimbCapture(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("VynixCapture"))) return;
    const TWeakObjectPtr<UWorld> WeakWorld(World);
    const auto Schedule = [World](float Delay, TFunction<void()> Action)
    {
        FTimerHandle Handle;
        World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(MoveTemp(Action)), Delay, false);
    };
#if WITH_EDITOR
    // Let the first view request MetaHuman textures and hair, then finish the editor
    // compilation queue before recording. Restart so warmup does not count as play time.
    static bool bWarmedAssets = false;
    if (!bWarmedAssets)
    {
        bWarmedAssets = true;
        Schedule(2.f, [WeakWorld]
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
            if (UWorld* ActiveWorld = WeakWorld.Get())
                UGameplayStatics::OpenLevel(ActiveWorld, FName(*UGameplayStatics::GetCurrentLevelName(ActiveWorld, true)));
        });
        return;
    }
#endif
    if (FParse::Param(FCommandLine::Get(), TEXT("VynixJumpCapture")))
    {
        FApp::SetUseFixedTimeStep(true);
        FApp::SetFixedDeltaTime(1.0 / 60.0);
        // Side views at early/middle/late poses expose mesh penetration that the
        // normal gameplay camera and capsule-only checks cannot show.
        const TSharedRef<TWeakObjectPtr<ACameraActor>> Camera = MakeShared<TWeakObjectPtr<ACameraActor>>();
        for (int32 Jump = 0; Jump < 3; ++Jump)
        {
            const float StartTime = 1.f + Jump * 2.f;
            Schedule(StartTime, [WeakWorld, Jump]
            {
                if (UWorld* ActiveWorld = WeakWorld.Get())
                    for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It)
                        It->LeapToLane(Jump == 0 ? 1 : 0);
            });
            for (int32 Frame = 0; Frame < 3; ++Frame)
            {
                Schedule(StartTime + 0.12f + Frame * 0.16f, [WeakWorld, Camera, Jump, Frame]
                {
                    if (UWorld* ActiveWorld = WeakWorld.Get())
                        for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It)
                            if (APlayerController* PC = Cast<APlayerController>(It->GetController()))
                            {
                                if (!Camera->IsValid()) *Camera = ActiveWorld->SpawnActor<ACameraActor>();
                                const FVector Position = It->GetActorLocation() + FVector(-180, -620, 100);
                                Camera->Get()->SetActorLocation(Position);
                                Camera->Get()->SetActorRotation((It->GetActorLocation() + FVector(10, 0, 50) - Position).Rotation());
                                Camera->Get()->GetCameraComponent()->SetFieldOfView(40.f);
                                TInlineComponentArray<UCameraComponent*> PlayerCameras(*It);
                                for (UCameraComponent* PlayerCamera : PlayerCameras)
                                    if (PlayerCamera->IsActive()) Camera->Get()->GetCameraComponent()->PostProcessSettings = PlayerCamera->PostProcessSettings;
                                PC->SetViewTarget(Camera->Get());
                                FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("Vynix_Jump_%d_%d.png"), Jump, Frame), true, false);
                            }
                });
            }
        }
        Schedule(7.f, [] { FPlatformMisc::RequestExit(false); });
        return;
    }
    static bool bCheckingRestart = false;
    Schedule(0.25f, [WeakWorld]
    {
        if (UWorld* ActiveWorld = WeakWorld.Get())
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It)
            {
                UE_LOG(LogTemp, Display, TEXT("VYNIX_CAPTURE: player=%s mesh=%s grip=%s handOffset=%.2f"),
                    *It->GetClass()->GetPathName(), *GetPathNameSafe(It->GetMesh()->GetSkeletalMeshAsset()),
                    *It->GetActorLocation().ToString(), It->GetHangHandHeight());
                if (!It->GetClass()->GetPathName().Contains(TEXT("BP_EndlessPayton")))
                    UE_LOG(LogTemp, Error, TEXT("VYNIX_CAPTURE: the default player is not Payton"));
            }
    });
    if (bCheckingRestart)
    {
        Schedule(0.5f, [WeakWorld]
        {
            if (UWorld* ActiveWorld = WeakWorld.Get())
            {
                for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It)
                {
                    if (It->GetHealth() != 100.f || It->GetRunHeight() != 0.f || It->IsRunOver() || It->IsRunPaused())
                    {
                        UE_LOG(LogTemp, Error, TEXT("VYNIX_CAPTURE: restart did not reset the run"));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Display, TEXT("VYNIX_CAPTURE: restart reset health, height and run state"));
                    }
                }
                FScreenshotRequest::RequestScreenshot(TEXT("Vynix_07_Restart.png"), true, false);
            }
        });
        Schedule(2.f, [] { FPlatformMisc::RequestExit(false); });
        return;
    }
    const TSharedRef<TWeakObjectPtr<ACameraActor>> ContactCamera = MakeShared<TWeakObjectPtr<ACameraActor>>();
    Schedule(1.2f, [WeakWorld, ContactCamera]
    {
        if (UWorld* ActiveWorld = WeakWorld.Get())
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It)
                if (APlayerController* PC = Cast<APlayerController>(It->GetController()))
                {
                    ACameraActor* Camera = ActiveWorld->SpawnActor<ACameraActor>();
                    *ContactCamera = Camera;
                    const FVector Position = It->GetActorLocation() + FVector(-320, -380, 140);
                    Camera->SetActorLocation(Position);
                    Camera->SetActorRotation((It->GetActorLocation() + FVector(20, 0, 90) - Position).Rotation());
                    Camera->GetCameraComponent()->SetFieldOfView(32.f);
                    TInlineComponentArray<UCameraComponent*> PlayerCameras(*It);
                    for (UCameraComponent* PlayerCamera : PlayerCameras)
                        if (PlayerCamera->IsActive()) Camera->GetCameraComponent()->PostProcessSettings = PlayerCamera->PostProcessSettings;
                    PC->SetViewTarget(Camera);
                }
    });
    Schedule(1.5f, [] { FScreenshotRequest::RequestScreenshot(TEXT("Vynix_08_HandContact.png"), true, false); });
    Schedule(1.8f, [WeakWorld, ContactCamera]
    {
        if (UWorld* ActiveWorld = WeakWorld.Get())
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It)
                if (APlayerController* PC = Cast<APlayerController>(It->GetController())) PC->SetViewTarget(*It);
        if (ContactCamera->IsValid()) ContactCamera->Get()->Destroy();
    });
    Schedule(3.f, [] { FScreenshotRequest::RequestScreenshot(TEXT("Vynix_01_Ascent.png"), true, false); });
    Schedule(3.2f, [WeakWorld]
    {
        if (UWorld* ActiveWorld = WeakWorld.Get())
        {
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It)
            {
                const TWeakObjectPtr<AEndlessClimber> Player(*It);
                const float FrozenTime = It->GetSurvivalSeconds();
                It->ToggleRunPause();
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Player, FrozenTime](float)
                {
                    if (Player.IsValid())
                    {
                        if (!Player->IsRunPaused() || Player->GetSurvivalSeconds() != FrozenTime)
                            UE_LOG(LogTemp, Error, TEXT("VYNIX_CAPTURE: pause did not freeze the run"));
                        FScreenshotRequest::RequestScreenshot(TEXT("Vynix_06_Pause.png"), true, false);
                    }
                    return false;
                }), 0.4f);
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Player](float)
                {
                    if (Player.IsValid())
                    {
                        Player->ToggleRunPause();
                        UE_LOG(LogTemp, Display, TEXT("VYNIX_CAPTURE: pause/resume checked"));
                    }
                    return false;
                }), 1.f);
            }
        }
    });
    Schedule(4.f, [WeakWorld]
    {
        if (UWorld* ActiveWorld = WeakWorld.Get())
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It) It->LeapToLane(1);
    });
    Schedule(4.25f, [] { FScreenshotRequest::RequestScreenshot(TEXT("Vynix_02_Leap.png"), true, false); });
    Schedule(5.f, [] { FScreenshotRequest::RequestScreenshot(TEXT("Vynix_03_Warning.png"), true, false); });
    Schedule(7.f, [WeakWorld]
    {
        if (UWorld* ActiveWorld = WeakWorld.Get())
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It) It->ApplyRockHit(55.f);
    });
    Schedule(7.15f, [] { FScreenshotRequest::RequestScreenshot(TEXT("Vynix_04_Health.png"), true, false); });
    Schedule(9.f, [WeakWorld]
    {
        if (UWorld* ActiveWorld = WeakWorld.Get())
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It) It->ApplyRockHit(100.f);
    });
    Schedule(10.f, [] { FScreenshotRequest::RequestScreenshot(TEXT("Vynix_05_GameOver.png"), true, false); });
    Schedule(11.f, [WeakWorld]
    {
        bCheckingRestart = true;
        if (UWorld* ActiveWorld = WeakWorld.Get())
            for (TActorIterator<AEndlessClimber> It(ActiveWorld); It; ++It) It->RestartRun();
    });
#endif
}
