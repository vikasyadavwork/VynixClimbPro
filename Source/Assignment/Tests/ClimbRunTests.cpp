#include "../ClimbRunRules.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../ClimbFallingRock.h"
#include "../EndlessClimber.h"
#include "../EndlessClimbWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
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

		bool Create(bool bWithArena, UClass* CharacterClass = nullptr, FVector LedgeSize = FVector(105, 175, 26))
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
			Climber = World->SpawnActor<AEndlessClimber>(CharacterClass ? CharacterClass : AEndlessClimber::StaticClass(), FVector::ZeroVector,
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
				Arena->LedgeDimensions = LedgeSize;
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
	TestTrue(TEXT("A jump keeps the same distance from the wall"), FMath::IsNearlyEqual(Mid.X, Start.X));
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
		TestTrue(TEXT("Crossing and upward jumps preserve hanging depth at every sample"),
			FMath::IsNearlyEqual(LeftToRight.X, Start.X) && FMath::IsNearlyEqual(Up.X, Start.X));
		const FVector DeeperGrip = End + FVector(-120.f, 0.f, 0.f);
		const float Ease = Alpha * Alpha * (3.f - 2.f * Alpha);
		TestTrue(TEXT("Different ledge depths follow the two grip depths without extra inward movement"),
			FMath::IsNearlyEqual(JumpPosition(Start, DeeperGrip, Alpha).X, FMath::Lerp(Start.X, DeeperGrip.X, Ease), 0.001));
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
	if (!TestTrue(TEXT("Preview climber initializes"), Run.Create(true)))
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbGripBoundsTest, "VynixClimb.Rules.LedgeBounds", ClimbTests::Flags)

bool FClimbGripBoundsTest::RunTest(const FString& Parameters)
{
	// An off-centre pivot with nonuniform scaling must not be treated as a centred 100 cm cube.
	const FBox LocalBounds(FVector(-20, -40, 0), FVector(80, 60, 30));
	const FTransform Transform(FRotator::ZeroRotator, FVector(500, -300, 21000), FVector(2, 3, 0.5));
	const FBox Bounds = LocalBounds.TransformBy(Transform);
	FVector Grip;
	TestTrue(TEXT("A scaled ledge accepts a capsule"), ClimbRunRules::TryHangLocation(Bounds, -10000, 42, 8, 70, Grip));
	TestTrue(TEXT("Depth includes the pivot and scale"), FMath::IsNearlyEqual(Grip.X, 410.0));
	TestTrue(TEXT("Width clamps the grip away from the left corner"), FMath::IsNearlyEqual(Grip.Y, -370.0));
	TestTrue(TEXT("Hands meet the actual top, including thickness"), FMath::IsNearlyEqual(Grip.Z + 70, 21015.0));
	TestTrue(TEXT("The entire capsule clears the front"), Grip.X + 42 <= Bounds.Min.X - 8);
	TestTrue(TEXT("The opposite corner is also clamped"), ClimbRunRules::TryHangLocation(Bounds, 10000, 42, 8, 70, Grip)
		&& FMath::IsNearlyEqual(Grip.Y, Bounds.Max.Y - 50));
	TestFalse(TEXT("A narrow ledge is rejected"), ClimbRunRules::TryHangLocation(FBox(FVector(0, 0, 0), FVector(100, 99, 20)), 50, 42, 8, 70, Grip));
	TestFalse(TEXT("Invalid bounds are rejected"), ClimbRunRules::TryHangLocation(FBox(ForceInit), 0, 42, 8, 70, Grip));
	TestFalse(TEXT("Invalid capsule dimensions are rejected"), ClimbRunRules::TryHangLocation(Bounds, 0, -1, 8, 70, Grip));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbPaytonTest, "VynixClimb.Runtime.PaytonAndLedgeClearance", ClimbTests::Flags)

bool FClimbPaytonTest::RunTest(const FString& Parameters)
{
	UClass* PaytonClass = LoadClass<AEndlessClimber>(nullptr, TEXT("/Game/Endless/Payton/BP_EndlessPayton.BP_EndlessPayton_C"));
	if (!TestNotNull(TEXT("The playable Payton Blueprint loads"), PaytonClass)) return false;
	for (const FVector Size : { FVector(105, 175, 26), FVector(210, 250, 45), FVector(60, 130, 18) })
	{
		ClimbTests::FPreviewRun Run;
		if (!TestTrue(TEXT("Payton starts in the resized arena"), Run.Create(true, PaytonClass, Size))) return false;
		USkeletalMeshComponent* Body = Run.Climber->GetMesh();
		TestTrue(TEXT("The original Payton body is used"), Body->GetSkeletalMeshAsset()->GetPathName().Contains(TEXT("/MetaHumans/Payton/")));
		UAnimSingleNodeInstance* Animation = Body->GetSingleNodeInstance();
		if (!TestNotNull(TEXT("Payton has a running climbing animation"), Animation)) return false;
		TestTrue(TEXT("Clips are retargeted to Payton's actual skeleton"), Animation->GetCurrentAsset()->GetSkeleton() == Body->GetSkeletalMeshAsset()->GetSkeleton());
		int32 FollowingClothes = 0;
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Run.Climber);
		for (USkeletalMeshComponent* Part : Parts)
		{
			if (Part != Body && Part->LeaderPoseComponent.Get() == Body) ++FollowingClothes;
		}
		TestTrue(TEXT("Clothing follows the animated body"), FollowingClothes >= 3);
		const float Radius = Run.Climber->GetCapsuleComponent()->GetScaledCapsuleRadius();
		for (int32 Step = 0; Step < 85; ++Step)
		{
			FBox Bounds;
			if (!TestTrue(TEXT("The current recycled ledge can be found"), Run.Arena->GetLedgeBounds(Step, Run.Climber->GetCurrentLane(), Bounds))) return false;
			TestTrue(TEXT("Generated ledges use the requested physical size"), Bounds.GetSize().Equals(Size, 0.02));
			TestTrue(TEXT("The capsule is outside the ledge front"), Run.Climber->GetActorLocation().X + Radius <= Bounds.Min.X - 7.9);
			TestTrue(TEXT("The hanging hand height meets the ledge top"), FMath::IsNearlyEqual(Run.Climber->GetActorLocation().Z + Run.Climber->GetHangHandHeight(), Bounds.Max.Z, 0.02));
			Run.Climber->Tick(0.12f);
			Run.Climber->LeapToLane(1 - Run.Climber->GetCurrentLane());
			TestTrue(TEXT("A valid ledge starts a jump"), Run.Climber->IsLeaping());
			for (int32 Frame = 0; Frame < 12; ++Frame)
			{
				Run.Climber->Tick(ClimbRunRules::JumpDuration / 12.f + 0.0001f);
				TestTrue(TEXT("The complete jump stays outside the ledges"), Run.Climber->GetActorLocation().X + Radius <= Bounds.Min.X - 7.9);
			}
			TestFalse(TEXT("Payton lands after each jump"), Run.Climber->IsLeaping());
			Run.Arena->Tick(0.f);
		}
		// Rock spawns must follow the new depth, including much deeper ledges.
		Run.Arena->Tick(4.51f);
		Run.Arena->Tick(Run.Arena->GetWarningDuration() + 0.01f);
		if (TestEqual(TEXT("The new geometry still releases a warned rock"), Run.Rocks().Num(), 1))
			TestTrue(TEXT("Rock depth matches the actual player grip"), FMath::IsNearlyEqual(Run.Rocks()[0]->GetActorLocation().X, Run.Climber->GetActorLocation().X, 0.01));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbAnimatedJumpClearanceTest, "VynixClimb.Runtime.AnimatedJumpClearance", ClimbTests::Flags)

bool FClimbAnimatedJumpClearanceTest::RunTest(const FString& Parameters)
{
    bool bAllClear = true;
	UClass* PaytonClass = LoadClass<AEndlessClimber>(nullptr, TEXT("/Game/Endless/Payton/BP_EndlessPayton.BP_EndlessPayton_C"));
	if (!TestNotNull(TEXT("Payton loads for the animated clearance test"), PaytonClass)) return false;
	const TArray<FName> BodyBones = {
		TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"),
		TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
		TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
		TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r")
	};
	for (const FVector Size : { FVector(105, 175, 26), FVector(210, 250, 45), FVector(60, 130, 18) })
	{
		ClimbTests::FPreviewRun Run;
		if (!TestTrue(TEXT("The animated clearance arena initializes"), Run.Create(true, PaytonClass, Size))) return false;
		USkeletalMeshComponent* Mesh = Run.Climber->GetMesh();
		for (FName Bone : BodyBones)
		{
			if (!TestTrue(*FString::Printf(TEXT("Payton contains clearance bone %s"), *Bone.ToString()), Mesh->DoesSocketExist(Bone))) return false;
		}
		Mesh->TickAnimation(0.f, false);
		Mesh->RefreshBoneTransforms();
		const double HangingActorX = Run.Climber->GetActorLocation().X;
		const double HangingPelvisX = Mesh->GetSocketLocation(TEXT("pelvis")).X;
		const FVector HangingPelvisOffset = Mesh->GetSocketLocation(TEXT("pelvis")) - Run.Climber->GetActorLocation();
		// Right, left, then up exercises all three retargeted jump clips. Tick the actual
		// animation as well as the actor: a clear capsule alone does not prove the body is clear.
		for (int32 Lane : { 1, 0, 0 })
		{
			Run.Climber->Tick(0.12f);
			FBox Bounds;
			if (!TestTrue(TEXT("The departure ledge exists"), Run.Arena->GetLedgeBounds(
				Run.Climber->GetCompletedJumps(), Run.Climber->GetCurrentLane(), Bounds))) return false;
			Run.Climber->LeapToLane(Lane);
			if (!TestTrue(TEXT("The requested animated jump starts"), Run.Climber->IsLeaping())) return false;
			UAnimSingleNodeInstance* Animation = Mesh->GetSingleNodeInstance();
			if (!TestNotNull(TEXT("The jump has an animation instance"), Animation)) return false;
			const FString Clip = Animation->GetCurrentAsset()->GetName();
			double WorstPelvisDrift = 0.0;
			FVector WorstPelvisOffsetError = FVector::ZeroVector;
			int32 WorstPelvisFrame = 0;
			double DeepestBoneX = -std::numeric_limits<double>::max();
			FName DeepestBone;
			int32 DeepestFrame = 0;
			constexpr int32 Frames = 72;
			const float FrameSeconds = ClimbRunRules::JumpDuration / Frames;
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				if (Frame > 0) Run.Climber->Tick(FrameSeconds);
				Mesh->TickAnimation(Frame == 0 ? 0.f : FrameSeconds, false);
				Mesh->RefreshBoneTransforms();
				if (!TestTrue(TEXT("The actor maintains hanging depth throughout the jump"),
					FMath::IsNearlyEqual(Run.Climber->GetActorLocation().X, HangingActorX, 0.01))) return false;
				WorstPelvisDrift = FMath::Max(WorstPelvisDrift,
					FMath::Abs(Mesh->GetSocketLocation(TEXT("pelvis")).X - HangingPelvisX));
				const FVector OffsetError = Mesh->GetSocketLocation(TEXT("pelvis"))
					- Run.Climber->GetActorLocation() - HangingPelvisOffset;
				if (OffsetError.SizeSquared() > WorstPelvisOffsetError.SizeSquared())
				{
					WorstPelvisOffsetError = OffsetError;
					WorstPelvisFrame = Frame;
				}
				for (FName Bone : BodyBones)
				{
					const double BoneX = Mesh->GetSocketLocation(Bone).X;
					if (BoneX > DeepestBoneX)
					{
						DeepestBoneX = BoneX;
						DeepestBone = Bone;
						DeepestFrame = Frame;
					}
				}
			}
			const FString Context = FString::Printf(TEXT("%s, ledge %s"), *Clip, *Size.ToString());
			const bool bStableDepth = TestTrue(*FString::Printf(TEXT("The animated pelvis keeps hanging depth (%s; maximum drift %.3f cm)"),
				*Context, WorstPelvisDrift), WorstPelvisDrift < 0.1);
			const bool bFollowsActor = TestTrue(*FString::Printf(TEXT("The animated pelvis follows the actor without adding the clip's travel (%s; frame %d offset error %s)"),
				*Context, WorstPelvisFrame, *WorstPelvisOffsetError.ToString()), WorstPelvisOffsetError.Size() < 0.1);
			const bool bClearPose = TestTrue(*FString::Printf(TEXT("Animated body and major limb joints stay outside the ledge front (%s; %s at frame %d: X %.3f, front %.3f)"),
				*Context, *DeepestBone.ToString(), DeepestFrame, DeepestBoneX, Bounds.Min.X), DeepestBoneX < Bounds.Min.X);
            if (!bStableDepth || !bFollowsActor || !bClearPose) bAllClear = false;
			Run.Climber->Tick(FrameSeconds + 0.001f);
			Mesh->TickAnimation(0.f, false);
			Mesh->RefreshBoneTransforms();
			TestFalse(TEXT("The complete clip reaches the next grip"), Run.Climber->IsLeaping());
			TestEqual(TEXT("The jump reaches the requested lane"), Run.Climber->GetCurrentLane(), Lane);
			if (!Run.Arena->GetLedgeBounds(Run.Climber->GetCompletedJumps(), Lane, Bounds)) return false;
			for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
			{
				const FVector Tip = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("middle_03_%s"), Side)));
				const FVector Parent = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("middle_02_%s"), Side)));
				const FVector Contact = Tip + (Tip - Parent).GetSafeNormal() * 2.f * Mesh->GetComponentScale().GetAbsMax();
				if (!TestTrue(TEXT("Landing restores fingertip contact at the front lip"),
					FMath::Abs(Contact.X - (Bounds.Min.X - 2.5)) < 2.0 && FMath::Abs(Contact.Z - (Bounds.Max.Z + 2.5)) < 2.0)) return false;
			}
			Run.Arena->Tick(0.f);
		}
	}
	return bAllClear;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimbHandContactTest, "VynixClimb.Runtime.HandContactDuringIdle", ClimbTests::Flags)

bool FClimbHandContactTest::RunTest(const FString& Parameters)
{
	UClass* PaytonClass = LoadClass<AEndlessClimber>(nullptr, TEXT("/Game/Endless/Payton/BP_EndlessPayton.BP_EndlessPayton_C"));
	ClimbTests::FPreviewRun Run;
	if (!TestNotNull(TEXT("Payton loads"), PaytonClass) || !Run.Create(true, PaytonClass)) return false;
	USkeletalMeshComponent* Mesh = Run.Climber->GetMesh();
	for (int32 Grip = 0; Grip < 3; ++Grip)
	{
		FBox Bounds;
		Run.Arena->GetLedgeBounds(Grip, Run.Climber->GetCurrentLane(), Bounds);
		// Evaluate actual animated bones over the idle cycle, not only the capsule or first pose.
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			Mesh->TickAnimation(0.05f, false);
			Mesh->RefreshBoneTransforms();
			for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
			{
				const FVector Tip = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("middle_03_%s"), Side)));
				const FVector Parent = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("middle_02_%s"), Side)));
				const FVector Contact = Tip + (Tip - Parent).GetSafeNormal() * 2.f * Mesh->GetComponentScale().GetAbsMax();
				const FVector Wrist = Mesh->GetSocketLocation(FName(*FString::Printf(TEXT("hand_%s"), Side)));
				TestTrue(TEXT("The wrist remains outside the ledge face"), Wrist.X < Bounds.Min.X - 4.f);
				if (!TestTrue(TEXT("Fingertips remain at the front lip"), FMath::Abs(Contact.X - (Bounds.Min.X - 2.5)) < 2.0)
					|| !TestTrue(TEXT("Fingertips stay at the top surface throughout idle"), FMath::Abs(Contact.Z - (Bounds.Max.Z + 2.5)) < 2.0))
				{
					AddInfo(FString::Printf(TEXT("Contact %s; lip X %.2f, top Z %.2f"), *Contact.ToString(), Bounds.Min.X, Bounds.Max.Z));
					return false;
				}
			}
		}
		Run.Climber->LeapToLane(1 - Run.Climber->GetCurrentLane());
		FBox Unused;
		TestFalse(TEXT("Hands release during a jump"), Run.Climber->GetHandLedgeBounds(Unused));
		Run.Climber->Tick(ClimbRunRules::JumpDuration + 0.01f);
		Run.Climber->Tick(0.12f);
		Run.Arena->Tick(0.f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
