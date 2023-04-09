#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ClimbSaveGame.generated.h"

UCLASS()
class ASSIGNMENT_API UClimbSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 BestScore = 0;
    UPROPERTY(SaveGame) float BestHeight = 0.f;
};
