#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EndlessClimbGameMode.generated.h"

/** Boots the complete arcade mode without any Blueprint setup. */
UCLASS()
class ASSIGNMENT_API AEndlessClimbGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AEndlessClimbGameMode();
protected:
    virtual void BeginPlay() override;
};
