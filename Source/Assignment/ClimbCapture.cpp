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
#include "TimerManager.h"
#include "Containers/Ticker.h"
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
    static bool bCheckingRestart = false;
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
