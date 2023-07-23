#include "../ClimbRunRules.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../ClimbFallingRock.h"
#include "../EndlessClimber.h"
#include "../EndlessClimbWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#include <limits>

namespace ClimbTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** A non-game world keeps automation from overwriting the player's best-run save. */
	struct FPreviewRun
	{
		UWorld* World = nullptr;
		AEndlessClimber* Climber = nullptr;
		AEndlessClimbWorld* Arena = nullptr;

		bool Create(bool bWithArena)
		{
			if (!GEngine)
			{
				return false;
			}
			UWorld::InitializationValues Initialization;
			Initialization.AllowAudioPlayback(false).RequiresHitProxies(false).CreateNavigation(false)
				.CreateAISystem(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false)
				.SetTransactional(false).CreateFXSystem(false);
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ClimbAutomation"));
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false, Name, GetTransientPackage(), true,
				ERHIFeatureLevel::Num, &Initialization);
			if (!World)
			{
				return false;
			}
			GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Climber = World->SpawnActor<AEndlessClimber>(AEndlessClimber::StaticClass(), FVector::ZeroVector,
				FRotator::ZeroRotator, Spawn);
			if (!Climber)
			{
				return false;
			}
			Climber->DispatchBeginPlay();
			if (bWithArena)
			{
				Arena = World->SpawnActor<AEndlessClimbWorld>(AEndlessClimbWorld::StaticClass(), FVector::ZeroVector,
					FRotator::ZeroRotator, Spawn);
				if (!Arena)
				{
					return false;
				}
				Arena->DispatchBeginPlay();
			}
			return true;
		}

		TArray<AClimbFallingRock*> Rocks() const
		{
			TArray<AClimbFallingRock*> Result;
			for (TActorIterator<AClimbFallingRock> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					Result.Add(*It);
				}
			}
			return Result;
		}

		~FPreviewRun()
		{
			if (World)
			{
				// Mirror engine actor-test cleanup, including EndPlay for manually started actors.
				TArray<AActor*> Actors;
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					Actors.Add(*It);
				}
				for (AActor* Actor : Actors)
				{
					if (IsValid(Actor) && Actor->HasActorBegunPlay())
					{
						Actor->RouteEndPlay(EEndPlayReason::Quit);
					}
				}
				World->DestroyWorld(false);
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World);
				}
				World->RemoveFromRoot();
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbJumpRulesTest, "VynixClimb.Rules.JumpTrajectory", ClimbTests::Flags)

bool FClimbJumpRulesTest::RunTest(const FString& Parameters)
{
	using namespace ClimbRunRules;
	const FVector Start(0.f, LaneY(0), StartingHeight);
	const FVector End(0.f, LaneY(1), StartingHeight + StepHeight);
	TestTrue(TEXT("Jump starts exactly at its starting grip"), JumpPosition(Start, End, 0.f) == Start);
	TestTrue(TEXT("Jump ends exactly at its target grip"), JumpPosition(Start, End, 1.f) == End);
	TestTrue(TEXT("Negative progress stays exactly at the start"), JumpPosition(Start, End, -1.f) == Start);
	TestTrue(TEXT("Overshot progress stays exactly at the target"), JumpPosition(Start, End, 3.f) == End);
	const FVector Mid = JumpPosition(Start, End, 0.5f);
	TestTrue(TEXT("A crossing jump passes through the lane midpoint"), FMath::IsNearlyZero(Mid.Y));
	TestTrue(TEXT("A jump arcs out from the wall"), Mid.X < Start.X - 30.f);
	TestTrue(TEXT("A jump lifts above a straight interpolation"), Mid.Z > (Start.Z + End.Z) * 0.5f + 30.f);

	for (int32 Sample = 0; Sample <= 20; ++Sample)
	{
		const float Alpha = Sample / 20.f;
		const FVector LeftToRight = JumpPosition(Start, End, Alpha);
		const FVector RightToLeft = JumpPosition(FVector(Start.X, -Start.Y, Start.Z), FVector(End.X, -End.Y, End.Z), Alpha);
		TestTrue(TEXT("Left and right jumps mirror one another"),
			LeftToRight.Equals(FVector(RightToLeft.X, -RightToLeft.Y, RightToLeft.Z), KINDA_SMALL_NUMBER));
		const FVector Up = JumpPosition(Start, Start + FVector(0.f, 0.f, StepHeight), Alpha);
		TestTrue(TEXT("An upward jump stays on its selected lane"), FMath::IsNearlyEqual(Up.Y, Start.Y));
		TestFalse(TEXT("Trajectory remains finite"), LeftToRight.ContainsNaN());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbScoreRulesTest, "VynixClimb.Rules.Score", ClimbTests::Flags)

bool FClimbScoreRulesTest::RunTest(const FString& Parameters)
{
	using namespace ClimbRunRules;
	TestEqual(TEXT("A new run starts at zero"), Score(0.0, 0.0, 0), 0);
	TestEqual(TEXT("Negative inputs cannot create negative scores"), Score(-10.0, -30.0, -100), 0);
	TestEqual(TEXT("Height contributes points in metres"), Score(1.8, 0.0, 0), 180);
	TestEqual(TEXT("Survival time contributes points"), Score(0.0, 10.0, 0), 50);
	TestEqual(TEXT("Jump bonuses are included"), Score(0.0, 0.0, 110), 110);
	TestEqual(TEXT("Contributions combine"), Score(1.8, 10.0, 110), 340);
	TestEqual(TEXT("Displayed score saturates on long runs"), Score(1.e15, 1.e15, MAX_int32), MAX_int32);
	TestEqual(TEXT("Bonus alone cannot overflow the score"), Score(1.0, 1.0, MAX_int32), MAX_int32);
	int32 Last = 0;
	for (int32 Step = 0; Step < 500; ++Step)
	{
		const int32 Next = Score(Step * 1.8, Step * 0.7, Step * 10);
		TestTrue(TEXT("Continued progress never reduces the score"), Next >= Last);
		TestTrue(TEXT("More height never reduces the score"), Score(Step + 1.0, 10.0, 20) >= Score(Step, 10.0, 20));
		TestTrue(TEXT("More survival time never reduces the score"), Score(1.0, Step + 1.0, 20) >= Score(1.0, Step, 20));
		TestTrue(TEXT("More jump bonus never reduces the score"), Score(1.0, 10.0, Step + 1) >= Score(1.0, 10.0, Step));
		Last = Next;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbHealthRulesTest, "VynixClimb.Rules.Health", ClimbTests::Flags)

bool FClimbHealthRulesTest::RunTest(const FString& Parameters)
{
	using namespace ClimbRunRules;
	TestEqual(TEXT("A normal rock removes health"), HealthAfterHit(100.f, 25.f), 75.f);
	TestEqual(TEXT("Damage clamps at zero health"), HealthAfterHit(20.f, 1000.f), 0.f);
	TestEqual(TEXT("Health cannot exceed its maximum after a hit"), HealthAfterHit(200.f, 25.f), MaxHealth);
	TestEqual(TEXT("Zero damage is ignored"), HealthAfterHit(75.f, 0.f), 75.f);
	TestEqual(TEXT("Negative damage cannot heal"), HealthAfterHit(75.f, -25.f), 75.f);
	TestEqual(TEXT("NaN damage is ignored"), HealthAfterHit(75.f, std::numeric_limits<float>::quiet_NaN()), 75.f);
	TestEqual(TEXT("Infinite damage is ignored"), HealthAfterHit(75.f, std::numeric_limits<float>::infinity()), 75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbEndlessRunTest, "VynixClimb.Runtime.EndlessRunAndRecycling", ClimbTests::Flags)

bool FClimbEndlessRunTest::RunTest(const FString& Parameters)
{
	ClimbTests::FPreviewRun Run;
	if (!TestTrue(TEXT("Preview run initializes"), Run.Create(true)))
	{
		return false;
	}
	TArray<UInstancedStaticMeshComponent*> Scenery;
	Run.Arena->GetComponents(Scenery);
	if (!TestTrue(TEXT("Arena contains instanced scenery"), Scenery.Num() >= 3))
	{
		return false;
	}
	TArray<int32> OriginalCounts;
	TArray<FTransform> OriginalFirstInstances;
	for (UInstancedStaticMeshComponent* Component : Scenery)
	{
		OriginalCounts.Add(Component->GetInstanceCount());
		FTransform First;
		Component->GetInstanceTransform(0, First);
		OriginalFirstInstances.Add(First);
		TestTrue(TEXT("Each scenery pool is populated"), Component->GetInstanceCount() > 0);
	}
	Run.Climber->ApplyRockHit(25.f);
	int32 PreviousScore = Run.Climber->GetScore();
	for (int32 Step = 0; Step < 100; ++Step)
	{
		Run.Climber->Tick(0.12f);
		const int32 Lane = Step % 3 == 0 ? Run.Climber->GetCurrentLane() : 1 - Run.Climber->GetCurrentLane();
		Run.Climber->LeapToLane(-1);
		Run.Climber->LeapToLane(2);
		TestFalse(TEXT("Invalid lane requests are rejected"), Run.Climber->IsLeaping());
		Run.Climber->LeapToLane(Lane);
		TestTrue(TEXT("The next grip can be reached"), Run.Climber->IsLeaping());
		Run.Climber->LeapToLane(1 - Lane);
		Run.Climber->Tick(ClimbRunRules::JumpDuration * 0.5f);
		TestTrue(TEXT("The character is airborne halfway through the leap"), Run.Climber->IsLeaping());
		Run.Climber->Tick(ClimbRunRules::JumpDuration * 0.5f + 0.001f);
		TestFalse(TEXT("Each leap completes"), Run.Climber->IsLeaping());
		TestEqual(TEXT("Extra input cannot change the in-flight destination"), Run.Climber->GetCurrentLane(), Lane);
		TestTrue(TEXT("Each completed grip increases the score"), Run.Climber->GetScore() > PreviousScore);
		PreviousScore = Run.Climber->GetScore();
		TestTrue(TEXT("Height follows completed climbs"), FMath::IsNearlyEqual(Run.Climber->GetRunHeight(), (Step + 1) * 1.8f, 0.001f));
		TestTrue(TEXT("The combo remains bounded"), Run.Climber->GetCombo() > 0 && Run.Climber->GetCombo() <= 10);
		if ((Step + 1) % 25 == 0)
		{
			TestEqual(TEXT("Milestone health restores ten points and caps at full"), Run.Climber->GetHealth(),
				FMath::Min(100.f, 75.f + ((Step + 1) / 25) * 10.f));
		}
		// Zero elapsed hazard time isolates recycling from collision or random rock timing.
		Run.Arena->Tick(0.f);
	}
	TestTrue(TEXT("The run continues beyond the entire initial arena"), Run.Climber->GetRunHeight() >= 180.f);
	TestTrue(TEXT("Survival time includes active climbing"), Run.Climber->GetSurvivalSeconds() > 60.f);
	TestFalse(TEXT("Climbing has no fixed finish"), Run.Climber->IsRunOver());
	bool bRecycled = false;
	for (int32 Index = 0; Index < Scenery.Num(); ++Index)
	{
		TestEqual(TEXT("Endless scenery does not grow its instance pools"), Scenery[Index]->GetInstanceCount(), OriginalCounts[Index]);
		FTransform Current;
		Scenery[Index]->GetInstanceTransform(0, Current);
		bRecycled |= !Current.GetLocation().Equals(OriginalFirstInstances[Index].GetLocation());
	}
	TestTrue(TEXT("Existing scenery instances move upward with the player"), bRecycled);
	Run.Climber->Tick(2.3f);
	TestEqual(TEXT("Resting too long clears the clean-jump combo"), Run.Climber->GetCombo(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbDamageIntegrationTest, "VynixClimb.Runtime.DamageGraceAndGameOver", ClimbTests::Flags)

bool FClimbDamageIntegrationTest::RunTest(const FString& Parameters)
{
	ClimbTests::FPreviewRun Run;
	if (!TestTrue(TEXT("Preview climber initializes"), Run.Create(false)))
	{
		return false;
	}
	TestEqual(TEXT("The climber starts with full health"), Run.Climber->GetHealth(), 100.f);
	Run.Climber->ApplyRockHit(-25.f);
	Run.Climber->ApplyRockHit(std::numeric_limits<float>::quiet_NaN());
	TestEqual(TEXT("Invalid actor damage is ignored"), Run.Climber->GetHealth(), 100.f);
	Run.Climber->ApplyRockHit(25.f);
	TestEqual(TEXT("A collision damages the climber"), Run.Climber->GetHealth(), 75.f);
	TestTrue(TEXT("A collision triggers damage feedback"), Run.Climber->GetDamageFeedback() > 0.f);
	Run.Climber->ApplyRockHit(25.f);
	TestEqual(TEXT("Repeated collision callbacks do not stack damage"), Run.Climber->GetHealth(), 75.f);
	Run.Climber->Tick(ClimbRunRules::HitGraceSeconds * 0.5f);
	Run.Climber->ApplyRockHit(25.f);
	TestEqual(TEXT("The grace interval protects the entire early period"), Run.Climber->GetHealth(), 75.f);
	Run.Climber->Tick(ClimbRunRules::HitGraceSeconds * 0.5f + 0.01f);
	Run.Climber->ApplyRockHit(25.f);
	TestEqual(TEXT("A later stone can damage again"), Run.Climber->GetHealth(), 50.f);
	Run.Climber->Tick(ClimbRunRules::HitGraceSeconds + 0.01f);
	Run.Climber->ApplyRockHit(500.f);
	TestEqual(TEXT("Fatal damage clamps health at zero"), Run.Climber->GetHealth(), 0.f);
	TestTrue(TEXT("Fatal damage ends the run"), Run.Climber->IsRunOver());
	const int32 FinalScore = Run.Climber->GetScore();
	const float FinalTime = Run.Climber->GetSurvivalSeconds();
	const FVector FinalLocation = Run.Climber->GetActorLocation();
	Run.Climber->LeapToLane(1);
	Run.Climber->Tick(2.f);
	Run.Climber->ApplyRockHit(25.f);
	TestFalse(TEXT("An ended run cannot start another leap"), Run.Climber->IsLeaping());
	TestEqual(TEXT("Final score stops accumulating"), Run.Climber->GetScore(), FinalScore);
	TestEqual(TEXT("Final survival time stops accumulating"), Run.Climber->GetSurvivalSeconds(), FinalTime);
	TestTrue(TEXT("The final grip stays stable"), Run.Climber->GetActorLocation().Equals(FinalLocation));
	TestTrue(TEXT("Best score includes the completed run"), Run.Climber->GetBestScore() >= FinalScore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbRockfallDirectorTest, "VynixClimb.Runtime.RockfallTelegraph", ClimbTests::Flags)

bool FClimbRockfallDirectorTest::RunTest(const FString& Parameters)
{
	ClimbTests::FPreviewRun Run;
	if (!TestTrue(TEXT("Preview arena initializes"), Run.Create(true)))
	{
		return false;
	}
	TestEqual(TEXT("The starting grip has no immediate rock warning"), Run.Arena->GetWarningLane(), -1);
	TestEqual(TEXT("No rock exists before its warning"), Run.Rocks().Num(), 0);
	Run.Arena->Tick(4.49f);
	TestEqual(TEXT("The first rock allows a startup grace period"), Run.Arena->GetWarningLane(), -1);
	Run.Arena->Tick(0.02f);

	for (int32 Cycle = 0; Cycle < 5; ++Cycle)
	{
		const int32 ThreatenedLane = Run.Arena->GetWarningLane();
		TestTrue(TEXT("Exactly one valid lane is warned"), ThreatenedLane == 0 || ThreatenedLane == 1);
		const float Duration = Run.Arena->GetWarningDuration();
		TestTrue(TEXT("Warnings leave time for a complete escape jump"), Duration > ClimbRunRules::JumpDuration + 0.5f);
		TestTrue(TEXT("Warning duration stays in its designed bounds"), Duration >= 1.5f && Duration <= 1.9f);
		TestEqual(TEXT("The warning begins at full duration"), Run.Arena->GetWarningRemaining(), Duration);
		Run.Arena->Tick(Duration - 0.02f);
		TestEqual(TEXT("No stone releases before its warning finishes"), Run.Rocks().Num(), 0);
		TestTrue(TEXT("The countdown remains positive before release"), Run.Arena->GetWarningRemaining() > 0.f);
		Run.Arena->Tick(0.03f);
		TArray<AClimbFallingRock*> Rocks = Run.Rocks();
		if (!TestEqual(TEXT("A finished warning releases one stone"), Rocks.Num(), 1))
		{
			return false;
		}
		AClimbFallingRock* Rock = Rocks[0];
		if (!Rock->HasActorBegunPlay())
		{
			Rock->DispatchBeginPlay();
		}
		USphereComponent* PhysicsSphere = Rock->FindComponentByClass<USphereComponent>();
		if (TestNotNull(TEXT("The stone has a spherical physics body"), PhysicsSphere))
		{
			TestTrue(TEXT("Stone physics simulation is enabled"), PhysicsSphere->IsSimulatingPhysics());
			TestTrue(TEXT("Stone gravity is enabled"), PhysicsSphere->IsGravityEnabled());
			TestTrue(TEXT("Stone size remains a small obstacle"), PhysicsSphere->GetScaledSphereRadius() >= 18.f && PhysicsSphere->GetScaledSphereRadius() <= 30.f);
		}
		TestTrue(TEXT("The stone appears overhead with falling distance"), Rock->GetActorLocation().Z > Run.Climber->GetActorLocation().Z + 700.f);
		TestTrue(TEXT("The stone belongs to the warned lane"), FMath::IsNearlyEqual(Rock->GetActorLocation().Y,
			Run.Climber->GetRunOrigin().Y + ClimbRunRules::LaneY(ThreatenedLane), 0.01));
		Run.Arena->Tick(8.f);
		TestEqual(TEXT("The director keeps the active stone's lane marked"), Run.Arena->GetWarningLane(), ThreatenedLane);
		TestEqual(TEXT("No second stone releases while the first is active"), Run.Rocks().Num(), 1);

		// Simulate clearance, not a Chaos timestep: this tests director sequencing deterministically.
		Rock->SetActorLocation(Run.Climber->GetActorLocation() + FVector(0.f, 0.f, -1300.f), false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("A rock below the grip is recognized as clear"), Rock->HasPassedPlayer());
		Rock->Destroy();
		Run.Arena->Tick(0.01f);
		TestEqual(TEXT("The warning clears when the previous stone is gone"), Run.Arena->GetWarningLane(), -1);
		TestEqual(TEXT("Finished stones leave no live actors"), Run.Rocks().Num(), 0);
		float Waited = 0.f;
		while (Run.Arena->GetWarningLane() < 0 && Waited < 6.2f)
		{
			Run.Arena->Tick(0.05f);
			Waited += 0.05f;
		}
		TestTrue(TEXT("Subsequent warnings use the random delay bounds"), Waited >= 3.15f && Waited <= 6.1f);
		TestTrue(TEXT("The next hazard is telegraphed before spawning"), Run.Arena->GetWarningLane() >= 0 && Run.Rocks().Num() == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbBufferedDodgeTest, "VynixClimb.Runtime.BufferedDodge", ClimbTests::Flags)

bool FClimbBufferedDodgeTest::RunTest(const FString& Parameters)
{
	ClimbTests::FPreviewRun Run;
	if (!TestTrue(TEXT("Preview climber initializes"), Run.Create(false)))
	{
		return false;
	}
	Run.Climber->Tick(0.12f);
	Run.Climber->LeapToLane(0);
	Run.Climber->Tick(ClimbRunRules::JumpDuration * 0.5f);
	Run.Climber->RequestDodge(1);
	Run.Climber->RequestDodge(0);
	Run.Climber->RequestDodge(1);
	Run.Climber->RequestDodge(-1);
	Run.Climber->RequestDodge(2);
	TestEqual(TEXT("Buffered input does not redirect the current airborne jump"), Run.Climber->GetIntendedLane(), 0);
	Run.Climber->Tick(ClimbRunRules::JumpDuration * 0.5f + 0.001f);
	TestFalse(TEXT("The first jump lands before executing the dodge"), Run.Climber->IsLeaping());
	Run.Climber->Tick(0.12f);
	TestTrue(TEXT("The buffered dodge begins after landing and cooldown"), Run.Climber->IsLeaping());
	TestEqual(TEXT("The last valid dodge wins and invalid lanes cannot overwrite it"), Run.Climber->GetIntendedLane(), 1);
	Run.Climber->Tick(ClimbRunRules::JumpDuration + 0.001f);
	TestEqual(TEXT("The buffered dodge reaches the requested edge"), Run.Climber->GetCurrentLane(), 1);
	Run.Climber->Tick(0.12f);
	TestFalse(TEXT("A consumed dodge is not executed a second time"), Run.Climber->IsLeaping());
	Run.Climber->RequestDodge(-1);
	Run.Climber->RequestDodge(2);
	Run.Climber->Tick(0.12f);
	TestFalse(TEXT("Invalid idle dodge input cannot start a jump"), Run.Climber->IsLeaping());
	TestEqual(TEXT("Invalid input keeps the current edge"), Run.Climber->GetCurrentLane(), 1);

	Run.Climber->LeapToLane(1);
	Run.Climber->Tick(ClimbRunRules::JumpDuration + 0.001f);
	Run.Climber->RequestDodge(0);
	Run.Climber->Tick(0.05f);
	TestFalse(TEXT("A queued dodge respects the landing cooldown"), Run.Climber->IsLeaping());
	Run.Climber->Tick(0.06f);
	TestTrue(TEXT("A dodge requested during cooldown executes when ready"), Run.Climber->IsLeaping());
	TestEqual(TEXT("The cooldown-buffered dodge targets the other edge"), Run.Climber->GetIntendedLane(), 0);
	Run.Climber->Tick(ClimbRunRules::JumpDuration + 0.001f);
	Run.Climber->Tick(0.12f);
	TestEqual(TEXT("The final dodge lands on the left edge"), Run.Climber->GetCurrentLane(), 0);
	TestFalse(TEXT("The cooldown buffer is also consumed exactly once"), Run.Climber->IsLeaping());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
