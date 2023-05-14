#include "EndlessClimbWorld.h"

#include "ClimbFallingRock.h"
#include "EndlessClimber.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEndlessClimbWorld::AEndlessClimbWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ArenaRoot")));
	Cliff = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CliffSections"));
	Ledges = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ClimbingLedges"));
	LedgeEdges = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LedgeHighlights"));
	Outcrops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CliffOutcrops"));
	Seams = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CliffSeams"));
	WarningBeacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RedWarningBeacon"));
	WarningLeftRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningLeftRail"));
	WarningRightRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningRightRail"));
	WarningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WarningLight"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CliffRock(TEXT("/Game/StarterContent/Props/SM_Rock.SM_Rock"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Endless/Materials/M_ClimbSolid.M_ClimbSolid"));
	SolidMaterial = Material.Object;
	for (UInstancedStaticMeshComponent* Component : { Cliff.Get(), Ledges.Get(), LedgeEdges.Get(), Outcrops.Get(), Seams.Get() })
	{
		Component->SetupAttachment(GetRootComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		if (Cube.Succeeded())
		{
			Component->SetStaticMesh(Cube.Object);
		}
	}
	if (Sphere.Succeeded())
	{
		WarningBeacon->SetStaticMesh(Sphere.Object);
	}
	if (CliffRock.Succeeded()) Outcrops->SetStaticMesh(CliffRock.Object);
	for (UStaticMeshComponent* Component : { WarningBeacon.Get(), WarningLeftRail.Get(), WarningRightRail.Get() })
	{
		Component->SetupAttachment(GetRootComponent());
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetVisibility(false);
	}
	if (Cube.Succeeded())
	{
		WarningLeftRail->SetStaticMesh(Cube.Object);
		WarningRightRail->SetStaticMesh(Cube.Object);
	}
	WarningLight->SetupAttachment(GetRootComponent());
	WarningLight->SetLightColor(FLinearColor(1.f, 0.018f, 0.006f));
	WarningLight->SetAttenuationRadius(640.f);
	WarningLight->SetCastShadows(false);
	WarningLight->SetVisibility(false);
}

void AEndlessClimbWorld::BeginPlay()
{
	Super::BeginPlay();
	HazardRandom.GenerateNewSeed();
	FindClimber();
}

void AEndlessClimbWorld::FindClimber()
{
	if (Climber.IsValid())
	{
		return;
	}
	for (TActorIterator<AEndlessClimber> It(GetWorld()); It; ++It)
	{
		Climber = *It;
		SetActorLocation(It->GetRunOrigin());
		BuildArena();
		return;
	}
}

UMaterialInterface* AEndlessClimbWorld::MakeColoredMaterial(const FLinearColor& Color)
{
	if (SolidMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(SolidMaterial, this);
		Material->SetVectorParameterValue(TEXT("Base Color"), Color);
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetScalarParameterValue(TEXT("Roughness"), 0.86f);
		return Material;
	}
	return nullptr;
}

void AEndlessClimbWorld::BuildArena()
{
	if (bArenaBuilt)
	{
		return;
	}
	Cliff->SetMaterial(0, MakeColoredMaterial(FLinearColor(0.075f, 0.13f, 0.17f)));
	Ledges->SetMaterial(0, MakeColoredMaterial(FLinearColor(0.27f, 0.40f, 0.40f)));
	LedgeEdges->SetMaterial(0, MakeColoredMaterial(FLinearColor(0.30f, 0.88f, 0.72f)));
	Outcrops->SetMaterial(0, MakeColoredMaterial(FLinearColor(0.10f, 0.20f, 0.23f)));
	Seams->SetMaterial(0, MakeColoredMaterial(FLinearColor(0.030f, 0.065f, 0.085f)));
	UMaterialInterface* WarningMaterial = MakeColoredMaterial(FLinearColor(1.f, 0.025f, 0.01f));
	WarningBeacon->SetMaterial(0, WarningMaterial);
	WarningLeftRail->SetMaterial(0, WarningMaterial);
	WarningRightRail->SetMaterial(0, WarningMaterial);

	SectionIndices.SetNum(SectionCount);
	for (int32 Slot = 0; Slot < SectionCount; ++Slot)
	{
		UpdateSection(Slot, Slot - 4, true);
	}
	bArenaBuilt = true;
}

void AEndlessClimbWorld::UpdateSection(int32 Slot, int32 LogicalIndex, bool bAddInstances)
{
	SectionIndices[Slot] = LogicalIndex;
	const double Bottom = static_cast<double>(LogicalIndex) * SectionHeight;
	// Unsigned arithmetic deliberately wraps the decorative hash during long runs.
	const uint32 DecorationSeed = (static_cast<uint32>(LogicalIndex) * 7919u + 2023u) & 0x7fffffffu;
	FRandomStream DecorationRandom(static_cast<int32>(DecorationSeed));
	const auto PutInstance = [bAddInstances](UInstancedStaticMeshComponent* Component, int32 Index,
		const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator(0., 0., 0.))
	{
		const FTransform Transform(Rotation, Location, Scale);
		if (bAddInstances)
		{
			Component->AddInstance(Transform);
		}
		else
		{
			Component->UpdateInstanceTransform(Index, Transform, false, false, true);
		}
	};

	PutInstance(Cliff, Slot, FVector(155.f, 0.f, Bottom + SectionHeight * .5f), FVector(1.6f, 11.f, 7.2f));
	for (int32 Step = 0; Step < StepsPerSection; ++Step)
	{
		// The character starts at origin + 300; the capsule's feet are 96 lower.
		const double FootHeight = Bottom + 204.f + Step * StepHeight;
		for (int32 Lane = 0; Lane < 2; ++Lane)
		{
			const int32 Index = Slot * StepsPerSection * 2 + Step * 2 + Lane;
			const float LaneY = Lane == 0 ? -190.f : 190.f;
			PutInstance(Ledges, Index, FVector(43.f, LaneY, FootHeight - 13.f), FVector(1.05f, 1.75f, .26f));
			PutInstance(LedgeEdges, Index, FVector(-11.f, LaneY, FootHeight - 5.f), FVector(.035f, 1.67f, .055f));
		}
		PutInstance(Seams, Slot * StepsPerSection + Step,
			FVector(73.8f, DecorationRandom.FRandRange(-30.f, 30.f), Bottom + Step * StepHeight + 112.f),
			FVector(.018f, 10.5f, .022f), FRotator(0.f, 0.f, DecorationRandom.FRandRange(-3.f, 3.f)));
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Side = Index % 2 == 0 ? -1.f : 1.f;
		const FBoxSphereBounds Bounds = Outcrops->GetStaticMesh()->GetBounds();
		const FVector Size = Bounds.BoxExtent * 2.0;
		const FVector RockScale = FVector(DecorationRandom.FRandRange(200.f, 340.f),
			DecorationRandom.FRandRange(250.f, 400.f), 480.f) / Size;
		const FRotator RockRotation(0.f, 0.f, DecorationRandom.FRandRange(-15.f, 15.f));
		const FVector Center(110.f, Side * DecorationRandom.FRandRange(500.f, 585.f), Bottom + (Index / 2) * 360.f + 180.f);
		PutInstance(Outcrops, Slot * 4 + Index,
			Center - RockRotation.RotateVector(Bounds.Origin * RockScale), RockScale, RockRotation);
	}
}

void AEndlessClimbWorld::UpdateArena()
{
	const AEndlessClimber* Player = Climber.Get();
	if (!Player)
	{
		return;
	}
	const int32 LowestSection = FMath::FloorToInt((Player->GetActorLocation().Z - GetActorLocation().Z) / SectionHeight) - 4;
	bool bRecycled = false;
	for (int32 Slot = 0; Slot < SectionCount; ++Slot)
	{
		const int32 OldIndex = SectionIndices[Slot];
		if (OldIndex < LowestSection || OldIndex >= LowestSection + SectionCount)
		{
			const int32 NewIndex = LowestSection + ((OldIndex - LowestSection) % SectionCount + SectionCount) % SectionCount;
			UpdateSection(Slot, NewIndex, false);
			bRecycled = true;
		}
	}
	if (bRecycled)
	{
		for (UInstancedStaticMeshComponent* Component : { Cliff.Get(), Ledges.Get(), LedgeEdges.Get(), Outcrops.Get(), Seams.Get() })
		{
			Component->MarkRenderStateDirty();
		}
	}
}

void AEndlessClimbWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FindClimber();
	AEndlessClimber* Player = Climber.Get();
	if (!Player || !bArenaBuilt || Player->IsRunPaused())
	{
		return;
	}
	UpdateArena();
	if (Player->IsRunOver())
	{
		WarningLane = -1;
		WarningRemaining = 0.f;
		UpdateWarningVisuals(0.f);
		if (ActiveRock.IsValid())
		{
			ActiveRock->Destroy();
		}
		return;
	}

	if (WarningLane >= 0)
	{
		if (!bWaitingForRock)
		{
			WarningRemaining = FMath::Max(0.f, WarningRemaining - DeltaSeconds);
			if (WarningRemaining <= 0.f)
			{
				ReleaseRock();
			}
		}
		else if (!ActiveRock.IsValid() || ActiveRock->HasPassedPlayer())
		{
			// Never begin another lane's warning until the previous stone is clear.
			WarningLane = -1;
			WarningRemaining = 0.f;
			bWaitingForRock = false;
			HazardCountdown = NextHazardDelay();
		}
	}
	else
	{
		HazardCountdown -= DeltaSeconds;
		if (HazardCountdown <= 0.f)
		{
			BeginRockWarning();
		}
	}
	UpdateWarningVisuals(DeltaSeconds);
}

float AEndlessClimbWorld::NextHazardDelay() const
{
	const AEndlessClimber* Player = Climber.Get();
	const float Difficulty = Player ? FMath::Clamp(Player->GetRunHeight() / 180.f, 0.f, 1.f) : 0.f;
	return HazardRandom.FRandRange(3.2f, 6.f) - Difficulty * 1.15f;
}

void AEndlessClimbWorld::BeginRockWarning()
{
	const AEndlessClimber* Player = Climber.Get();
	if (!Player)
	{
		return;
	}
	// Some warnings threaten the occupied ledge; others reward observing the lane.
	WarningLane = HazardRandom.FRand() < .75f ? FMath::Clamp(Player->GetCurrentLane(), 0, 1) : HazardRandom.RandRange(0, 1);
	WarningDuration = FMath::Lerp(1.9f, 1.5f, FMath::Clamp(Player->GetRunHeight() / 180.f, 0.f, 1.f));
	WarningRemaining = WarningDuration;
	WarningAge = 0.f;
	bWaitingForRock = false;
}

void AEndlessClimbWorld::ReleaseRock()
{
	AEndlessClimber* Player = Climber.Get();
	if (!Player)
	{
		return;
	}
	FVector SpawnLocation = Player->GetRunOrigin();
	SpawnLocation.Y += WarningLane == 0 ? -190.f : 190.f;
	SpawnLocation.Z = Player->GetActorLocation().Z + HazardRandom.FRandRange(760.f, 940.f);
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AClimbFallingRock* Rock = GetWorld()->SpawnActor<AClimbFallingRock>(AClimbFallingRock::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Parameters);
	if (Rock)
	{
		Rock->InitializeRock(Player, 25.f, HazardRandom.FRandRange(19.f, 27.f));
		ActiveRock = Rock;
	}
	bWaitingForRock = true;
}

void AEndlessClimbWorld::UpdateWarningVisuals(float DeltaSeconds)
{
	const AEndlessClimber* Player = Climber.Get();
	const bool bVisible = WarningLane >= 0 && Player != nullptr;
	WarningBeacon->SetVisibility(bVisible && !bWaitingForRock);
	WarningLeftRail->SetVisibility(bVisible);
	WarningRightRail->SetVisibility(bVisible);
	WarningLight->SetVisibility(bVisible);
	if (!bVisible)
	{
		return;
	}
	WarningAge += DeltaSeconds;
	const float Pulse = .5f + .5f * FMath::Sin(WarningAge * 13.f);
	const FVector Origin = Player->GetRunOrigin();
	const float LaneY = WarningLane == 0 ? -190.f : 190.f;
	const double Height = Player->GetActorLocation().Z;
	WarningBeacon->SetWorldLocation(FVector(Origin.X - 20.f, Origin.Y + LaneY, Height + 470.f));
	WarningBeacon->SetWorldScale3D(FVector(.20f + Pulse * .09f));
	WarningLeftRail->SetWorldLocation(FVector(Origin.X - 24.f, Origin.Y + LaneY - 83.f, Height + 180.f));
	WarningRightRail->SetWorldLocation(FVector(Origin.X - 24.f, Origin.Y + LaneY + 83.f, Height + 180.f));
	WarningLeftRail->SetWorldScale3D(FVector(.03f, .025f + Pulse * .012f, 5.8f));
	WarningRightRail->SetWorldScale3D(WarningLeftRail->GetComponentScale());
	WarningLight->SetWorldLocation(FVector(Origin.X - 145.f, Origin.Y + LaneY, Height + 160.f));
	WarningLight->SetIntensity(900.f + Pulse * 1700.f);
}

void AEndlessClimbWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveRock.IsValid())
	{
		ActiveRock->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}
