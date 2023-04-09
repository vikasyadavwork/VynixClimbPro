#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EndlessClimber.generated.h"

class UAnimSequence;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/** A two-lane arcade climber. The original free-climbing character is still available. */
UCLASS()
class ASSIGNMENT_API AEndlessClimber : public ACharacter
{
    GENERATED_BODY()
public:
    AEndlessClimber();
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

    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> FollowCamera;
    UPROPERTY() TObjectPtr<UAnimSequence> HangAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> LeftAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> RightAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> UpAnimation;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ChalkTrail;

    FVector RunOrigin = FVector(0, 0, 20000);
    FVector JumpStart = FVector::ZeroVector;
    FVector JumpTarget = FVector::ZeroVector;
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
};
