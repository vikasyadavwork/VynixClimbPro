#include "EndlessClimbHUD.h"

#include "EndlessClimber.h"
#include "EndlessClimbWorld.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Fonts/SlateFontInfo.h"

namespace ClimbHUD
{
	const FLinearColor Ink(0.035f, 0.055f, 0.069f, 1.f);
	const FLinearColor Paper(0.94f, 0.94f, 0.86f, 1.f);
	const FLinearColor Muted(0.57f, 0.66f, 0.65f, 1.f);
	const FLinearColor Teal(0.22f, 0.91f, 0.75f, 1.f);
	const FLinearColor Gold(1.f, 0.72f, 0.34f, 1.f);
	const FLinearColor Red(1.f, 0.22f, 0.24f, 1.f);
	const FName PauseButton(TEXT("PauseRun"));
	const FName ResumeButton(TEXT("ResumeRun"));
	const FName RestartButton(TEXT("RestartRun"));
}

void AEndlessClimbHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine || !PlayerOwner)
	{
		return;
	}

	AEndlessClimber* Climber = Cast<AEndlessClimber>(PlayerOwner->GetPawn());
	if (!Climber)
	{
		return;
	}
	Player = Climber;
	if (!WorldActor.IsValid())
	{
		for (TActorIterator<AEndlessClimbWorld> It(GetWorld()); It; ++It)
		{
			WorldActor = *It;
			break;
		}
	}

	// Use real time to keep menu buttons and pause animations responsive while the world is paused.
	const double Now = FPlatformTime::Seconds();
	const float DeltaTime = LastDrawTime > 0.0 ? FMath::Clamp(static_cast<float>(Now - LastDrawTime), 0.f, 0.1f) : 0.f;
	LastDrawTime = Now;
	AnimationTime += DeltaTime;
	UIScale = FMath::Max(0.1f, FMath::Min(Canvas->SizeX / 1600.f, Canvas->SizeY / 900.f));
	ViewWidth = Canvas->SizeX / UIScale;
	ViewHeight = Canvas->SizeY / UIScale;

	const float Health = Climber->GetHealth();
	const bool bNewRun = Climber->GetSurvivalSeconds() + 0.1f < PreviousSurvival;
	if (!bInitializedHealth || bNewRun)
	{
		DisplayHealth = TrailingHealth = PreviousHealth = Health;
		PreviousScore = Climber->GetScore();
		ScorePulse = LandingPulse = DamageHold = 0.f;
		PreviousJump = 0.f;
		bInitializedHealth = true;
	}
	if (Health < PreviousHealth)
	{
		DamageHold = 0.55f;
	}
	PreviousHealth = Health;
	PreviousSurvival = Climber->GetSurvivalSeconds();
	DisplayHealth = FMath::FInterpTo(DisplayHealth, Health, DeltaTime, 12.f);
	DamageHold = FMath::Max(0.f, DamageHold - DeltaTime);
	if (DamageHold <= 0.f)
	{
		TrailingHealth = FMath::FInterpTo(TrailingHealth, Health, DeltaTime, 3.f);
	}
	TrailingHealth = FMath::Max(TrailingHealth, DisplayHealth);
	if (Climber->GetScore() > PreviousScore)
	{
		ScorePulse = 1.f;
	}
	PreviousScore = Climber->GetScore();
	ScorePulse = FMath::Max(0.f, ScorePulse - DeltaTime * 2.5f);
	const float Jump = Climber->GetJumpFeedback();
	if (PreviousJump > 0.01f && Jump <= 0.01f && !Climber->IsRunOver())
	{
		LandingPulse = 1.f;
	}
	PreviousJump = Jump;
	LandingPulse = FMath::Max(0.f, LandingPulse - DeltaTime * 2.4f);

	DrawFeedback(*Climber);
	DrawRunStatus(*Climber);
	DrawHealth(*Climber);
	if (WorldActor.IsValid() && !Climber->IsRunOver() && !Climber->IsRunPaused())
	{
		DrawHazard(*Climber, *WorldActor.Get());
	}
	if (Climber->IsRunOver() || Climber->IsRunPaused())
	{
		DrawOverlay(*Climber);
	}
	else
	{
		Button(ClimbHUD::PauseButton, TEXT("II"), ViewWidth - 80.f, 30.f, 50.f, 46.f, ClimbHUD::Muted);
		Label(TEXT("A / LEFT     LEFT EDGE      |      D / RIGHT     RIGHT EDGE"), ViewWidth * 0.5f,
			ViewHeight - 63.f, 15.f, ClimbHUD::Paper, true);
		Label(TEXT("W / SPACE     CLIMB UP          P / ESC     PAUSE          R     RESTART"), ViewWidth * 0.5f,
			ViewHeight - 36.f, 13.f, ClimbHUD::Muted, true);
	}
}

void AEndlessClimbHUD::DrawRunStatus(const AEndlessClimber& Climber)
{
	using namespace ClimbHUD;
	Panel(30.f, 30.f, 330.f, 156.f);
	Rect(30.f, 30.f, 4.f, 156.f, Teal);
	Label(TEXT("VYNIX / ENDLESS ASCENT"), 52.f, 47.f, 15.f, Teal);
	Label(FString::Printf(TEXT("%06d"), Climber.GetScore()), 50.f, 74.f,
		49.f + ScorePulse * 3.f, Paper);
	Label(TEXT("SCORE"), 54.f, 138.f, 12.f, Muted);
	Label(FString::Printf(TEXT("BEST  %06d"), Climber.GetBestScore()), 180.f, 138.f, 15.f, Gold);
	Rect(52.f, 165.f, 284.f * (0.3f + ScorePulse * 0.7f), 2.f, FLinearColor(Teal.R, Teal.G, Teal.B, 0.45f));

	const float StatsX = ViewWidth - 310.f;
	Panel(StatsX, 94.f, 280.f, 92.f);
	Label(TEXT("ALTITUDE"), StatsX + 19.f, 109.f, 12.f, Muted);
	Label(TEXT("SURVIVED"), StatsX + 156.f, 109.f, 12.f, Muted);
	Label(FString::Printf(TEXT("%.0f m"), Climber.GetRunHeight()), StatsX + 18.f, 135.f, 27.f, Paper);
	Label(ClockText(Climber.GetSurvivalSeconds()), StatsX + 155.f, 135.f, 27.f, Paper);
	Line(StatsX + 138.f, 111.f, StatsX + 138.f, 165.f, FLinearColor(0.2f, 0.3f, 0.3f, 0.7f));

	if (Climber.GetCombo() > 1 && !Climber.IsRunOver())
	{
		Panel(30.f, 198.f, 212.f, 40.f, 0.75f);
		Label(FString::Printf(TEXT("%d  CLEAN JUMPS"), Climber.GetCombo()), 48.f, 208.f, 16.f, Gold);
	}
}

void AEndlessClimbHUD::DrawHealth(const AEndlessClimber& Climber)
{
	using namespace ClimbHUD;
	const float X = 30.f;
	const float Y = ViewHeight - 128.f;
	const float MaxHealth = FMath::Max(Climber.GetMaxHealth(), 1.f);
	const float HealthFraction = FMath::Clamp(DisplayHealth / MaxHealth, 0.f, 1.f);
	const bool bLowHealth = Climber.GetHealth() <= MaxHealth * 0.3f && !Climber.IsRunOver();
	const float Pulse = 0.65f + 0.35f * FMath::Sin(static_cast<float>(AnimationTime) * 7.f);
	const FLinearColor Fill = bLowHealth ? Red : Teal;
	Panel(X, Y, 330.f, 98.f);
	Label(bLowHealth ? TEXT("HEALTH / CRITICAL") : TEXT("HEALTH"), X + 20.f, Y + 16.f, 13.f,
		bLowHealth ? Red : Muted);
	Label(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Climber.GetHealth()), FMath::RoundToInt(MaxHealth)),
		X + 221.f, Y + 13.f, 18.f, Paper);
	Rect(X + 20.f, Y + 49.f, 290.f, 12.f, FLinearColor(0.12f, 0.2f, 0.22f, 1.f));
	Rect(X + 20.f, Y + 49.f, 290.f * FMath::Clamp(TrailingHealth / MaxHealth, 0.f, 1.f), 12.f, Gold);
	Rect(X + 20.f, Y + 49.f, 290.f * HealthFraction, 12.f,
		FLinearColor(Fill.R, Fill.G, Fill.B, bLowHealth ? Pulse : 1.f));
	for (int32 Index = 1; Index < 4; ++Index)
	{
		Rect(X + 20.f + 290.f * Index / 4.f, Y + 49.f, 2.f, 12.f, Ink);
	}
	Label(bLowHealth ? TEXT("WATCH THE RED WARNINGS") : TEXT("DODGE ROCKS. KEEP CLIMBING."),
		X + 20.f, Y + 74.f, 11.f, bLowHealth ? Red : Muted);
	if (bLowHealth)
	{
		Rect(0.f, 0.f, 5.f, ViewHeight, FLinearColor(Red.R, Red.G, Red.B, Pulse));
		Rect(ViewWidth - 5.f, 0.f, 5.f, ViewHeight, FLinearColor(Red.R, Red.G, Red.B, Pulse));
	}
}

void AEndlessClimbHUD::DrawHazard(const AEndlessClimber& Climber, const AEndlessClimbWorld& ClimbWorld)
{
	using namespace ClimbHUD;
	const int32 Lane = ClimbWorld.GetWarningLane();
	if (Lane < 0)
	{
		return;
	}
	const float Remaining = FMath::Max(0.f, ClimbWorld.GetWarningRemaining());
	const float Duration = FMath::Max(0.01f, ClimbWorld.GetWarningDuration());
	const float Pulse = 0.55f + 0.45f * FMath::Abs(FMath::Sin(static_cast<float>(AnimationTime) * 8.f));
	const bool bDanger = Lane == Climber.GetCurrentLane() || Lane == Climber.GetIntendedLane();
	const float Width = 438.f;
	const float X = (ViewWidth - Width) * 0.5f;
	const float Y = 42.f;
	Panel(X, Y, Width, 122.f, 0.94f);
	Rect(X, Y, Width, 3.f, FLinearColor(Red.R, Red.G, Red.B, Pulse));
	Label(Lane == 0 ? TEXT("!  ROCKFALL / LEFT EDGE") : TEXT("!  ROCKFALL / RIGHT EDGE"),
		X + Width * 0.5f, Y + 17.f, 21.f, Red, true);
	const FString Direction = Lane == 0 ? TEXT("JUMP RIGHT  [ D / RIGHT ]") : TEXT("JUMP LEFT  [ A / LEFT ]");
	Label(bDanger ? Direction : TEXT("SAFE EDGE / HOLD YOUR LINE"),
		X + Width * 0.5f, Y + 52.f, 18.f, bDanger ? Paper : Teal, true);
	Label(Remaining > 0.f ? FString::Printf(TEXT("FALLING IN %.1f s"), Remaining) : TEXT("STONE FALLING"), X + 22.f, Y + 91.f, 12.f, Muted);
	Rect(X + 188.f, Y + 95.f, Width - 210.f, 5.f, FLinearColor(0.25f, 0.09f, 0.1f, 1.f));
	Rect(X + 188.f, Y + 95.f, (Width - 210.f) * FMath::Clamp(Remaining / Duration, 0.f, 1.f), 5.f, Red);

	// The threatened side remains identifiable without having to read the text.
	const float EdgeX = Lane == 0 ? 0.f : ViewWidth - 13.f;
	Rect(EdgeX, ViewHeight * 0.24f, 13.f, ViewHeight * 0.47f, FLinearColor(Red.R, Red.G, Red.B, Pulse * 0.8f));
	if (bDanger)
	{
		const float ArrowX = ViewWidth * 0.5f + (Lane == 0 ? 88.f : -88.f);
		Chevron(ArrowX, ViewHeight * 0.62f, Lane == 0, 19.f, FLinearColor(Paper.R, Paper.G, Paper.B, Pulse));
		Chevron(ArrowX + (Lane == 0 ? 22.f : -22.f), ViewHeight * 0.62f, Lane == 0, 19.f,
			FLinearColor(Red.R, Red.G, Red.B, Pulse));
	}
}

void AEndlessClimbHUD::DrawFeedback(const AEndlessClimber& Climber)
{
	using namespace ClimbHUD;
	const float Damage = FMath::Clamp(Climber.GetDamageFeedback(), 0.f, 1.f);
	if (Damage > 0.f)
	{
		Rect(0.f, 0.f, ViewWidth, ViewHeight, FLinearColor(Red.R, 0.02f, 0.025f, Damage * 0.13f));
		const FLinearColor Edge(Red.R, Red.G, Red.B, Damage * 0.8f);
		Rect(0.f, 0.f, ViewWidth, 7.f, Edge);
		Rect(0.f, ViewHeight - 7.f, ViewWidth, 7.f, Edge);
		Rect(0.f, 0.f, 7.f, ViewHeight, Edge);
		Rect(ViewWidth - 7.f, 0.f, 7.f, ViewHeight, Edge);
		Label(TEXT("ROCK HIT"), ViewWidth * 0.5f, ViewHeight * 0.71f - (1.f - Damage) * 24.f,
			20.f, FLinearColor(Red.R, Red.G, Red.B, Damage), true);
	}
	const float Jump = FMath::Clamp(Climber.GetJumpFeedback(), 0.f, 1.f);
	if (Jump > 0.f && !Climber.IsRunOver())
	{
		const float Offset = (1.f - Jump) * 35.f;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float X = ViewWidth * 0.5f + (Index - 1) * 21.f;
			Line(X, ViewHeight * 0.78f + Offset, X, ViewHeight * 0.78f + Offset + 20.f * Jump,
				FLinearColor(Teal.R, Teal.G, Teal.B, Jump * 0.45f), 2.f);
		}
	}
	if (LandingPulse > 0.f && Damage <= 0.f && !Climber.IsRunOver())
	{
		Label(TEXT("GOOD GRIP"), ViewWidth * 0.5f, ViewHeight * 0.72f - (1.f - LandingPulse) * 18.f,
			14.f, FLinearColor(Teal.R, Teal.G, Teal.B, LandingPulse * 0.8f), true);
	}
}

void AEndlessClimbHUD::DrawOverlay(const AEndlessClimber& Climber)
{
	using namespace ClimbHUD;
	const bool bOver = Climber.IsRunOver();
	Rect(0.f, 0.f, ViewWidth, ViewHeight, FLinearColor(Ink.R, Ink.G, Ink.B, 0.79f));
	const float Width = 570.f;
	const float Height = bOver ? 444.f : 354.f;
	const float X = (ViewWidth - Width) * 0.5f;
	const float Y = (ViewHeight - Height) * 0.5f;
	const float Center = ViewWidth * 0.5f;
	const FLinearColor Accent = bOver ? Gold : Teal;
	Panel(X, Y, Width, Height, 0.98f);
	Rect(X, Y, Width, 4.f, Accent);
	Label(TEXT("VYNIX / ENDLESS ASCENT"), Center, Y + 32.f, 14.f, Accent, true);
	Label(bOver ? TEXT("ASCENT COMPLETE") : TEXT("TAKE A BREATHER"), Center, Y + 75.f, 36.f, Paper, true);
	Label(bOver ? TEXT("Every climb takes you a little further.") : TEXT("Your climb will be here when you are ready."),
		Center, Y + 126.f, 17.f, Muted, true);
	if (bOver)
	{
		Label(FString::Printf(TEXT("%06d"), Climber.GetScore()), Center, Y + 172.f, 52.f, Paper, true);
		Label(FString::Printf(TEXT("%.0f m CLIMBED     /     %s SURVIVED"), Climber.GetRunHeight(),
			*ClockText(Climber.GetSurvivalSeconds())), Center, Y + 241.f, 15.f, Muted, true);
		const bool bBest = Climber.GetScore() > 0 && Climber.GetScore() >= Climber.GetBestScore();
		Label(bBest ? TEXT("PERSONAL BEST") : FString::Printf(TEXT("PERSONAL BEST  %06d"), Climber.GetBestScore()),
			Center, Y + 277.f, 15.f, Gold, true);
		Button(RestartButton, TEXT("CLIMB AGAIN   [ R / GAMEPAD A ]"), X + 85.f, Y + 332.f, 400.f, 58.f, Teal);
		Label(TEXT("Switch edges before the rocks fall."), Center, Y + 410.f, 13.f, Muted, true);
	}
	else
	{
		Button(ResumeButton, TEXT("KEEP CLIMBING   [ P / ESC ]"), X + 65.f, Y + 188.f, 440.f, 56.f, Teal);
		Button(RestartButton, TEXT("START AGAIN   [ R ]"), X + 65.f, Y + 264.f, 440.f, 48.f, Muted);
	}
}

void AEndlessClimbHUD::Button(FName Name, const FString& Text, float X, float Y, float Width, float Height,
	const FLinearColor& Accent)
{
	float MouseX = -1.f;
	float MouseY = -1.f;
	const bool bHasMouse = PlayerOwner && PlayerOwner->GetMousePosition(MouseX, MouseY);
	const bool bHovered = bHasMouse && MouseX >= X * UIScale && MouseX <= (X + Width) * UIScale
		&& MouseY >= Y * UIScale && MouseY <= (Y + Height) * UIScale;
	Rect(X, Y, Width, Height, FLinearColor(Accent.R * 0.14f, Accent.G * 0.14f, Accent.B * 0.14f,
		bHovered ? 1.f : 0.88f));
	const FLinearColor Border(Accent.R, Accent.G, Accent.B, bHovered ? 1.f : 0.6f);
	Line(X, Y, X + Width, Y, Border);
	Line(X, Y + Height, X + Width, Y + Height, Border);
	Line(X, Y, X, Y + Height, Border);
	Line(X + Width, Y, X + Width, Y + Height, Border);
	Label(Text, X + Width * 0.5f, Y + (Height - 18.f) * 0.5f, 18.f, bHovered ? ClimbHUD::Paper : Accent, true);
	AddHitBox(FVector2D(X, Y) * UIScale, FVector2D(Width, Height) * UIScale, Name, true, 10);
}

void AEndlessClimbHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	AEndlessClimber* Climber = Player.Get();
	if (!Climber)
	{
		return;
	}
	if (BoxName == ClimbHUD::RestartButton)
	{
		Climber->RestartRun();
	}
	else if ((BoxName == ClimbHUD::PauseButton || BoxName == ClimbHUD::ResumeButton) && !Climber->IsRunOver())
	{
		Climber->ToggleRunPause();
	}
}

void AEndlessClimbHUD::Panel(float X, float Y, float Width, float Height, float Opacity)
{
	Rect(X + 3.f, Y + 5.f, Width, Height, FLinearColor(0.f, 0.f, 0.f, 0.15f));
	Rect(X, Y, Width, Height, FLinearColor(ClimbHUD::Ink.R, ClimbHUD::Ink.G, ClimbHUD::Ink.B, Opacity));
	Line(X, Y + Height, X + Width, Y + Height, FLinearColor(0.3f, 0.45f, 0.43f, 0.25f));
}

void AEndlessClimbHUD::Rect(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	if (Width > 0.f && Height > 0.f)
	{
		DrawRect(Color, X * UIScale, Y * UIScale, Width * UIScale, Height * UIScale);
	}
}

void AEndlessClimbHUD::Line(float X1, float Y1, float X2, float Y2, const FLinearColor& Color, float Thickness)
{
	DrawLine(X1 * UIScale, Y1 * UIScale, X2 * UIScale, Y2 * UIScale, Color, Thickness * UIScale);
}

void AEndlessClimbHUD::Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color,
	bool bCentered)
{
	// Rasterize at the actual display size instead of enlarging a small bitmap font.
	UFont* RuntimeFont = GEngine->GetMediumFont();
	if (!RuntimeFont) return;
	FSlateFontInfo Font = RuntimeFont->GetLegacySlateFontInfo();
	Font.FontObject = RuntimeFont; // Canvas requires a UFont even when using the Slate cache.
	Font.Size = FMath::Max(7, FMath::RoundToInt(Size * UIScale * 0.75f));
	FCanvasTextItem Item(FVector2D(X * UIScale, Y * UIScale), FText::FromString(Text), Font, Color);
	Item.bCentreX = bCentered;
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.5f), FVector2D(1.f, 1.f) * UIScale);
	Canvas->DrawItem(Item);
}

void AEndlessClimbHUD::Chevron(float X, float Y, bool bPointRight, float Size, const FLinearColor& Color)
{
	const float Direction = bPointRight ? 1.f : -1.f;
	Line(X - Direction * Size * 0.6f, Y - Size, X + Direction * Size * 0.4f, Y, Color, 3.f);
	Line(X + Direction * Size * 0.4f, Y, X - Direction * Size * 0.6f, Y + Size, Color, 3.f);
}

FString AEndlessClimbHUD::ClockText(float Seconds)
{
	const int32 WholeSeconds = FMath::Max(0, FMath::FloorToInt(Seconds));
	const int32 Minutes = WholeSeconds / 60;
	if (Minutes >= 60)
	{
		return FString::Printf(TEXT("%d:%02d:%02d"), Minutes / 60, Minutes % 60, WholeSeconds % 60);
	}
	return FString::Printf(TEXT("%02d:%02d"), Minutes, WholeSeconds % 60);
}
