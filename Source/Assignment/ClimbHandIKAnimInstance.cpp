#include "ClimbHandIKAnimInstance.h"
#include "EndlessClimber.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

namespace
{
    struct FClimbHandProxy : FAnimSingleNodeInstanceProxy
    {
        explicit FClimbHandProxy(UAnimInstance* Instance) : FAnimSingleNodeInstanceProxy(Instance)
        {
            Hands[0].BoneName = TEXT("hand_l");
            Hands[1].BoneName = TEXT("hand_r");
            Fingers[0].BoneName = TEXT("middle_03_l");
            Fingers[1].BoneName = TEXT("middle_03_r");
        }

        virtual void CacheBones() override
        {
            FAnimSingleNodeInstanceProxy::CacheBones();
            for (int32 Side = 0; Side < 2; ++Side)
            {
                Hands[Side].Initialize(GetRequiredBones());
                Fingers[Side].Initialize(GetRequiredBones());
            }
        }

        virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
        {
            FAnimSingleNodeInstanceProxy::PreUpdate(Instance, DeltaSeconds);
            const AEndlessClimber* Player = Cast<AEndlessClimber>(Instance->GetOwningActor());
            bPlantHands = Player && Player->GetHandLedgeBounds(LedgeBounds);
            // Copy world state on the game thread; evaluation only reads these values.
            MeshToWorld = Instance->GetSkelMeshComponent()->GetComponentTransform();
        }

        virtual bool Evaluate(FPoseContext& Output) override
        {
            const bool bEvaluated = FAnimSingleNodeInstanceProxy::Evaluate(Output);
            if (!bEvaluated || !bPlantHands) return bEvaluated;
            FCSPose<FCompactPose> Pose;
            Pose.InitPose(MoveTemp(Output.Pose));
            for (int32 Side = 0; Side < 2; ++Side)
            {
                if (!Hands[Side].IsValidToEvaluate(GetRequiredBones())
                    || !Fingers[Side].IsValidToEvaluate(GetRequiredBones())) continue;
                const FCompactPoseBoneIndex Hand = Hands[Side].GetCompactPoseIndex(GetRequiredBones());
                const FCompactPoseBoneIndex Lower = Pose.GetPose().GetParentBoneIndex(Hand);
                const FCompactPoseBoneIndex Upper = Pose.GetPose().GetParentBoneIndex(Lower);
                const FCompactPoseBoneIndex Finger = Fingers[Side].GetCompactPoseIndex(GetRequiredBones());
                const FCompactPoseBoneIndex FingerParent = Pose.GetPose().GetParentBoneIndex(Finger);
                FTransform UpperCS = Pose.GetComponentSpaceTransform(Upper);
                FTransform LowerCS = Pose.GetComponentSpaceTransform(Lower);
                FTransform HandCS = Pose.GetComponentSpaceTransform(Hand);
                const FVector FingerCS = Pose.GetComponentSpaceTransform(Finger).GetLocation();
                const FVector FingerDirection = (FingerCS - Pose.GetComponentSpaceTransform(FingerParent).GetLocation()).GetSafeNormal();
                // The distal joint stops short of the fingertip. Include the final phalanx
                // and a small skin margin, instead of aligning the wrist to the ledge top.
                const FVector ContactWorld = MeshToWorld.TransformPosition(FingerCS + FingerDirection * 2.0);
                const FVector ContactTarget(LedgeBounds.Min.X - 2.5,
                    FMath::Clamp(LedgeBounds.GetCenter().Y + (Side == 0 ? -32.0 : 32.0),
                        LedgeBounds.Min.Y + 10.0, LedgeBounds.Max.Y - 10.0), LedgeBounds.Max.Z + 2.5);
                const FVector WristWorld = MeshToWorld.TransformPosition(HandCS.GetLocation());
                const FVector ContactOffset = ContactWorld - WristWorld;
                // Approach the lip from outside and below. Keeping the clip's wrist
                // rotation can bury the palm even when the fingertip itself is aligned.
                const FVector GripDirection = FVector(0.45, Side == 0 ? -0.05 : 0.05, 0.89).GetSafeNormal();
                const FVector GripOffset = GripDirection * ContactOffset.Length();
                const FQuat ContactRotation = FQuat::FindBetweenNormals(ContactOffset.GetSafeNormal(), GripDirection);
                const FQuat HandRotation = MeshToWorld.GetRotation().Inverse() * ContactRotation
                    * MeshToWorld.GetRotation() * HandCS.GetRotation();
                const FVector Effector = MeshToWorld.InverseTransformPosition(ContactTarget - GripOffset);
                FVector Bend = LowerCS.GetLocation() - (UpperCS.GetLocation() + HandCS.GetLocation()) * 0.5;
                if (!Bend.Normalize()) Bend = FVector(0, 1, 0);
                const FVector Pole = LowerCS.GetLocation() + Bend * 100.0;
                AnimationCore::SolveTwoBoneIK(UpperCS, LowerCS, HandCS, Pole, Effector, false, 1.0, 1.0);
                HandCS.SetRotation(HandRotation);
                const TArray<FBoneTransform> Adjusted = {
                    FBoneTransform(Upper, UpperCS), FBoneTransform(Lower, LowerCS), FBoneTransform(Hand, HandCS) };
                Pose.LocalBlendCSBoneTransforms(Adjusted, 1.f);
            }
            // Use the copy overload: UE 5.5's move overload writes the root before
            // restoring the destination array, which is empty after InitPose above.
            FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(Pose, Output.Pose);
            return true;
        }

        FBoneReference Hands[2];
        FBoneReference Fingers[2];
        FBox LedgeBounds = FBox(ForceInit);
        FTransform MeshToWorld;
        bool bPlantHands = false;
    };
}

FAnimInstanceProxy* UClimbHandIKAnimInstance::CreateAnimInstanceProxy() { return new FClimbHandProxy(this); }
void UClimbHandIKAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
