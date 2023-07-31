#pragma once

#include "CoreMinimal.h"
#include "MyCharacter.h"
#include "EndlessClimber.generated.h"

class UAnimSequence;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class AEndlessClimbWorld;

/** A two-lane arcade climber. The original free-climbing character is still available. */
UCLASS()
class ASSIGNMENT_API AEndlessClimber : public AMyCharacter
{
    GENERATED_BODY()
public:
    AEndlessClimber(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintPure) int32 GetScore() const;
    UFUNCTION(BlueprintPure) int32 GetBestScore() const { return FMath::Max(BestScore, GetScore()); }
    UFUNCTION(BlueprintPure) float GetRunHeight() const { return HeightMetres; }
    UFUNCTION(BlueprintPure) float GetSurvivalSeconds() const { return SurvivalSeconds; }
    UFUNCTION(BlueprintPure) float GetHealth() const { return Health; }
    UFUNCTION(BlueprintPure) float GetMaxHealth() const { return 100.f; }
    UFUNCTION(BlueprintPure) bool IsRunOver() const { return bRunOver; }
    UFUNCTION(BlueprintPure) bool IsRunPaused() const { return bRunPaused; }
    UFUNCTION(BlueprintPure) float GetJumpFeedback() const { return JumpFeedback; }
    UFUNCTION(BlueprintPure) float GetDamageFeedback() const { return DamageFeedback; }
    UFUNCTION(BlueprintPure) int32 GetCombo() const { return Combo; }
    UFUNCTION(BlueprintPure) int32 GetCurrentLane() const { return CurrentLane; }
    UFUNCTION(BlueprintPure) int32 GetIntendedLane() const { return bLeaping ? TargetLane : CurrentLane; }
    UFUNCTION(BlueprintPure) FVector GetRunOrigin() const { return RunOrigin; }
    UFUNCTION(BlueprintPure) bool IsLeaping() const { return bLeaping; }
    int32 GetCompletedJumps() const { return CompletedJumps; }
    float GetHangHandHeight() const { return HangHandHeight; }
    FVector GetHangPelvisOffset() const { return HangPelvisOffset; }
    bool IsGripReady() const { return bGripReady; }
    void AttachToArena(AEndlessClimbWorld* Arena);
    bool CalculateGrip(const FBox& LedgeBounds, float DesiredY, FVector& OutLocation) const;
    bool GetHandLedgeBounds(FBox& OutBounds) const;
    bool GetJumpLedgeBounds(FBox& OutBounds) const;
    UFUNCTION(BlueprintCallable) void ApplyRockHit(float Damage);
    UFUNCTION(BlueprintCallable) void RestartRun();
    UFUNCTION(BlueprintCallable) void ToggleRunPause();
    UFUNCTION(BlueprintCallable) void LeapToLane(int32 Lane);
    UFUNCTION(BlueprintCallable) void RequestDodge(int32 Lane);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void LeapLeft();
    void LeapRight();
    void LeapUp();
    void GamepadConfirm();
    void TouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
    void FinishLeap();
    void PlayClimbAnimation(UAnimSequence* Animation, bool bLoop);
    void SaveBestRun();
    void UpdateJumpEffects(float DeltaSeconds);

    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> ArcadeCameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ArcadeCamera;
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Animation") TObjectPtr<UAnimSequence> HangAnimation;
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Animation") TObjectPtr<UAnimSequence> LeftAnimation;
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Animation") TObjectPtr<UAnimSequence> RightAnimation;
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Animation") TObjectPtr<UAnimSequence> UpAnimation;
    UPROPERTY(EditDefaultsOnly, Category = "Climb|Grip", meta = (ClampMin = "0")) float GripClearance = 8.f;
    UPROPERTY() TWeakObjectPtr<AEndlessClimbWorld> ClimbArena;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ChalkTrail;

    FVector RunOrigin = FVector(0, 0, 20000);
    FVector JumpStart = FVector::ZeroVector;
    FVector JumpTarget = FVector::ZeroVector;
    FRotator RestMeshRotation = FRotator(0, -90, 0);
    float HangHandHeight = 70.f;
    FVector HangPelvisOffset = FVector::ZeroVector;
    float Health = 100.f;
    double HeightMetres = 0.0;
    double SurvivalSeconds = 0.0;
    float JumpElapsed = 0.f;
    float IdleSeconds = 0.f;
    float InvulnerableSeconds = 0.f;
    float JumpFeedback = 0.f;
    float DamageFeedback = 0.f;
    float ChalkElapsed = 0.f;
    float SaveElapsed = 0.f;
    float BufferedDodgeSeconds = 0.f;
    int32 BufferedLane = -1;
    int32 CurrentLane = 0;
    int32 TargetLane = 0;
    int32 CompletedJumps = 0;
    int32 Combo = 0;
    int32 JumpBonus = 0;
    int32 BestScore = 0;
    float BestHeight = 0.f;
    int32 NextChalk = 0;
    TArray<float> ChalkLife;
    bool bLeaping = false;
    bool bRunOver = false;
    bool bRunPaused = false;
    bool bGripReady = false;
};
