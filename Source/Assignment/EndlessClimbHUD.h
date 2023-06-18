#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EndlessClimbHUD.generated.h"

class AEndlessClimber;
class AEndlessClimbWorld;

/** Resolution-independent HUD. All artwork is drawn in Canvas, so no widget assets are required. */
UCLASS()
class ASSIGNMENT_API AEndlessClimbHUD : public AHUD
{

	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

private:
	void DrawRunStatus(const AEndlessClimber& Climber);
	void DrawHealth(const AEndlessClimber& Climber);
	void DrawHazard(const AEndlessClimber& Climber, const AEndlessClimbWorld& ClimbWorld);
	void DrawFeedback(const AEndlessClimber& Climber);
	void DrawOverlay(const AEndlessClimber& Climber);
	void Button(FName Name, const FString& Text, float X, float Y, float Width, float Height,
		const FLinearColor& Accent);
	void Panel(float X, float Y, float Width, float Height, float Opacity = 0.88f);
	void Rect(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void Line(float X1, float Y1, float X2, float Y2, const FLinearColor& Color, float Thickness = 1.f);
	void Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color,
		bool bCentered = false);
	void Chevron(float X, float Y, bool bPointRight, float Size, const FLinearColor& Color);
	static FString ClockText(float Seconds);

	TWeakObjectPtr<AEndlessClimber> Player;
	TWeakObjectPtr<AEndlessClimbWorld> WorldActor;
	float UIScale = 1.f;
	float ViewWidth = 1600.f;
	float ViewHeight = 900.f;
	float DisplayHealth = 100.f;
	float TrailingHealth = 100.f;
	float PreviousHealth = 100.f;
	float DamageHold = 0.f;
	float ScorePulse = 0.f;
	float LandingPulse = 0.f;
	float PreviousJump = 0.f;
	float PreviousSurvival = 0.f;
	int32 PreviousScore = 0;
	bool bInitializedHealth = false;
	double LastDrawTime = 0.0;
	double AnimationTime = 0.0;
};
