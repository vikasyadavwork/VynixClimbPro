#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EndlessClimbGameMode.generated.h"

class AEndlessClimber;

/** Boots the complete arcade mode without any Blueprint setup. */
UCLASS()
class ASSIGNMENT_API AEndlessClimbGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AEndlessClimbGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
protected:
    virtual void BeginPlay() override;
    UPROPERTY(EditDefaultsOnly, Category = "Climb")
    TSoftClassPtr<AEndlessClimber> ClimberClass;
private:
    UPROPERTY(Transient) TObjectPtr<UClass> ResidentClimberClass;
};
