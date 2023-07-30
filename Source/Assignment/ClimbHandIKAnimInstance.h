#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "ClimbHandIKAnimInstance.generated.h"

/** Keeps the existing clips, then plants each hand on the measured ledge lip. */
UCLASS(Transient)
class ASSIGNMENT_API UClimbHandIKAnimInstance : public UAnimSingleNodeInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
