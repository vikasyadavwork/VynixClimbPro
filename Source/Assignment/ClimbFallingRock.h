#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClimbFallingRock.generated.h"

class AEndlessClimber;
class USphereComponent;
class UStaticMeshComponent;

/** A small Chaos-simulated rock. Decorative cliff geometry never blocks its route. */
UCLASS()
class ASSIGNMENT_API AClimbFallingRock : public AActor
{
	GENERATED_BODY()

public:
	AClimbFallingRock();
	virtual void Tick(float DeltaSeconds) override;
	void InitializeRock(AEndlessClimber* InClimber, float InDamage, float InRadius, float InGravityScale = 1.f);
	bool HasPassedPlayer() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Rock")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Rock")
	TObjectPtr<UStaticMeshComponent> RockMesh;

	UPROPERTY()
	TWeakObjectPtr<AEndlessClimber> Climber;

	UFUNCTION()
	void OnRockOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryHitPlayer();
	FVector PreviousLocation = FVector::ZeroVector;
	FVector PreviousPlayerLocation = FVector::ZeroVector;
	FVector PausedVelocity = FVector::ZeroVector;
	FVector PausedAngularVelocity = FVector::ZeroVector;
	float Damage = 25.f;
	float Radius = 23.f;
	float GravityScale = 1.f;
	float Age = 0.f;
	bool bHasHitPlayer = false;
	bool bPhysicsPaused = false;
};
