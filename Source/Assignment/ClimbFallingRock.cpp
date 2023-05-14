#include "ClimbFallingRock.h"

#include "EndlessClimber.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AClimbFallingRock::AClimbFallingRock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("PhysicsSphere"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(Radius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_PhysicsBody);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetSimulatePhysics(true);
	Collision->SetEnableGravity(true);
	Collision->SetUseCCD(true);
	Collision->SetLinearDamping(0.05f);
	Collision->SetAngularDamping(0.12f);

	RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoundStone"));
	RockMesh->SetupAttachment(Collision);
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RockMesh->SetRelativeScale3D(FVector(Radius / 50.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		RockMesh->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StoneMaterial(TEXT("/Game/Endless/Materials/M_ClimbSolid.M_ClimbSolid"));
	if (StoneMaterial.Succeeded())
	{
		RockMesh->SetMaterial(0, StoneMaterial.Object);
	}
}

void AClimbFallingRock::BeginPlay()
{
	Super::BeginPlay();
	PreviousLocation = GetActorLocation();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AClimbFallingRock::OnRockOverlap);
	if (UMaterialInstanceDynamic* Material = RockMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		const FLinearColor StoneColor(0.38f, 0.20f, 0.12f);
		Material->SetVectorParameterValue(TEXT("Base Color"), StoneColor);
		Material->SetVectorParameterValue(TEXT("Color"), StoneColor);
		Material->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
	}
}

void AClimbFallingRock::InitializeRock(AEndlessClimber* InClimber, float InDamage, float InRadius, float InGravityScale)
{
	Climber = InClimber;
	Damage = FMath::Max(0.f, InDamage);
	Radius = FMath::Clamp(InRadius, 18.f, 30.f);
	GravityScale = FMath::Clamp(InGravityScale, 0.5f, 1.5f);
	Collision->SetSphereRadius(Radius, true);
	RockMesh->SetRelativeScale3D(FVector(Radius / 50.f));
	Collision->SetMassOverrideInKg(NAME_None, 1.8f, true);
	Collision->SetPhysicsLinearVelocity(FVector(0.f, 0.f, -90.f));
	Collision->SetPhysicsAngularVelocityInDegrees(FVector(115.f, 45.f, 65.f));
	PreviousLocation = GetActorLocation();
	PreviousPlayerLocation = InClimber ? InClimber->GetActorLocation() : FVector::ZeroVector;
}

void AClimbFallingRock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AEndlessClimber* Player = Climber.Get();
	if (!Player || Player->IsRunOver())
	{
		Destroy();
		return;
	}

	if (Player->IsRunPaused())
	{
		if (!bPhysicsPaused)
		{
			PausedVelocity = Collision->GetPhysicsLinearVelocity();
			PausedAngularVelocity = Collision->GetPhysicsAngularVelocityInDegrees();
			Collision->SetSimulatePhysics(false);
			bPhysicsPaused = true;
		}
		return;
	}
	if (bPhysicsPaused)
	{
		Collision->SetSimulatePhysics(true);
		Collision->SetPhysicsLinearVelocity(PausedVelocity);
		Collision->SetPhysicsAngularVelocityInDegrees(PausedAngularVelocity);
		bPhysicsPaused = false;
		PreviousLocation = GetActorLocation();
		PreviousPlayerLocation = Player->GetActorLocation();
	}

	Age += DeltaSeconds;
	if (!FMath::IsNearlyEqual(GravityScale, 1.f))
	{
		Collision->AddForce(FVector(0.f, 0.f, GetWorld()->GetGravityZ() * (GravityScale - 1.f)), NAME_None, true);
	}

	// Chaos overlap events cover ordinary movement. This sweep also catches a
	// fast stone crossing the capsule between frames while the climber jumps.
	// Work relative to both moving bodies so a late jump never collides with
	// a place the stone occupied before the player reached it.
	if (!bHasHitPlayer)
	{
		const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
		const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
		const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
		const FVector PlayerLocation = Player->GetActorLocation();
		const FVector CapsuleOffset(0.f, 0.f, FMath::Max(0.f, CapsuleHalfHeight - CapsuleRadius));
		FVector RockClosest, CapsuleClosest;
		FMath::SegmentDistToSegmentSafe(PreviousLocation - PreviousPlayerLocation,
			GetActorLocation() - PlayerLocation, -CapsuleOffset, CapsuleOffset, RockClosest, CapsuleClosest);
		if (FVector::DistSquared(RockClosest, CapsuleClosest) <= FMath::Square(Radius + CapsuleRadius))
		{
			TryHitPlayer();
		}
	}
	PreviousLocation = GetActorLocation();
	PreviousPlayerLocation = Player->GetActorLocation();

	// A manual active-time lifetime respects pause and keeps the arena bounded.
	if (Age > 7.f || GetActorLocation().Z < Player->GetActorLocation().Z - 1200.f)
	{
		Destroy();
	}
}

void AClimbFallingRock::OnRockOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == Climber.Get())
	{
		TryHitPlayer();
	}
}

void AClimbFallingRock::TryHitPlayer()
{
	AEndlessClimber* Player = Climber.Get();
	if (!bHasHitPlayer && Player && !Player->IsRunOver() && !Player->IsRunPaused())
	{
		bHasHitPlayer = true;
		Player->ApplyRockHit(Damage);
	}
}

bool AClimbFallingRock::HasPassedPlayer() const
{
	const AEndlessClimber* Player = Climber.Get();
	return !Player || IsActorBeingDestroyed() || GetActorLocation().Z < Player->GetActorLocation().Z - 180.f;
}
