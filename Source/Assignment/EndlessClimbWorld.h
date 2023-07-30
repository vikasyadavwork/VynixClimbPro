#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EndlessClimbWorld.generated.h"

class AClimbFallingRock;
class AEndlessClimber;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

/** Recycled cliff scenery and a single-lane, warned rockfall director. */
UCLASS()
class ASSIGNMENT_API AEndlessClimbWorld : public AActor
{
	GENERATED_BODY()

public:
	AEndlessClimbWorld();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Climb|Warning")
	int32 GetWarningLane() const { return WarningLane; }

	UFUNCTION(BlueprintPure, Category = "Climb|Warning")
	float GetWarningRemaining() const { return WarningRemaining; }

	UFUNCTION(BlueprintPure, Category = "Climb|Warning")
	float GetWarningDuration() const { return WarningDuration; }

	bool GetLedgeBounds(int32 ClimbStep, int32 Lane, FBox& OutBounds) const;
	bool GetGripLocation(int32 ClimbStep, int32 Lane, FVector& OutLocation) const;

	/** Physical size in centimetres: depth (X), width (Y), thickness (Z). */
	UPROPERTY(EditAnywhere, Category = "Climb|Geometry", meta = (ClampMin = "1"))
	FVector LedgeDimensions = FVector(105.f, 175.f, 26.f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FindClimber();
	void BuildArena();
	void UpdateSection(int32 Slot, int32 LogicalIndex, bool bAddInstances);
	void UpdateArena();
	void UpdateWarningVisuals(float DeltaSeconds);
	void BeginRockWarning();
	void ReleaseRock();
	float NextHazardDelay() const;
	UMaterialInterface* MakeColoredMaterial(const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Cliff;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Ledges;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> LedgeEdges;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Outcrops;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Seams;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WarningBeacon;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WarningLeftRail;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WarningRightRail;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> WarningLight;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> SolidMaterial;
	UPROPERTY()
	TWeakObjectPtr<AEndlessClimber> Climber;
	UPROPERTY()
	TWeakObjectPtr<AClimbFallingRock> ActiveRock;

	static constexpr int32 SectionCount = 20;
	static constexpr int32 StepsPerSection = 4;
	static constexpr float StepHeight = 180.f;
	static constexpr float SectionHeight = StepHeight * StepsPerSection;
	TArray<int32> SectionIndices;
	FRandomStream HazardRandom;
	float HazardCountdown = 4.5f;
	float WarningDuration = 1.75f;
	float WarningRemaining = 0.f;
	float WarningAge = 0.f;
	int32 WarningLane = -1;
	bool bArenaBuilt = false;
	bool bWaitingForRock = false;
};
