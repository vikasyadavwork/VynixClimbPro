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
            Pelvis.BoneName = TEXT("pelvis");
            Spine.BoneName = TEXT("spine_01");
            Torso[0].BoneName = TEXT("spine_03");
            Torso[1].BoneName = TEXT("neck_01");
            Torso[2].BoneName = TEXT("head");
            Hands[0].BoneName = TEXT("hand_l");
            Hands[1].BoneName = TEXT("hand_r");
            Fingers[0].BoneName = TEXT("middle_03_l");
            Fingers[1].BoneName = TEXT("middle_03_r");
            Feet[0].BoneName = TEXT("foot_l");
            Feet[1].BoneName = TEXT("foot_r");
            Toes[0].BoneName = TEXT("ball_l");
            Toes[1].BoneName = TEXT("ball_r");
        }

        virtual void CacheBones() override
        {
            FAnimSingleNodeInstanceProxy::CacheBones();
            Pelvis.Initialize(GetRequiredBones());
            Spine.Initialize(GetRequiredBones());
            for (FBoneReference& Bone : Torso) Bone.Initialize(GetRequiredBones());
            for (int32 Side = 0; Side < 2; ++Side)
            {
                Hands[Side].Initialize(GetRequiredBones());
                Fingers[Side].Initialize(GetRequiredBones());
                Feet[Side].Initialize(GetRequiredBones());
                Toes[Side].Initialize(GetRequiredBones());
            }
        }

        virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
        {
            FAnimSingleNodeInstanceProxy::PreUpdate(Instance, DeltaSeconds);
            const AEndlessClimber* Player = Cast<AEndlessClimber>(Instance->GetOwningActor());
            bPlantHands = Player && Player->GetHandLedgeBounds(LedgeBounds);
            bLockJumpDepth = Player && Player->IsGripReady() && Player->IsLeaping();
            if (bLockJumpDepth) PelvisWorldTarget = Player->GetActorLocation() + Player->GetHangPelvisOffset();
            bClearJumpLimbs = bLockJumpDepth && Player->GetJumpLedgeBounds(JumpLedgeBounds);
            // Copy world state on the game thread; evaluation only reads these values.
            MeshToWorld = Instance->GetSkelMeshComponent()->GetComponentTransform();
        }

        void KeepTorsoOutside(FCSPose<FCompactPose>& Pose)
        {
            if (!Spine.IsValidToEvaluate(GetRequiredBones())) return;
            const FCompactPoseBoneIndex SpineIndex = Spine.GetCompactPoseIndex(GetRequiredBones());
            FTransform SpineCS = Pose.GetComponentSpaceTransform(SpineIndex);
            const FVector PivotWorld = MeshToWorld.TransformPosition(SpineCS.GetLocation());
            FVector Offsets[3];
            const double SkinMargins[3] = { 14.0, 14.0, 18.0 };
            for (int32 Index = 0; Index < 3; ++Index)
            {
                if (!Torso[Index].IsValidToEvaluate(GetRequiredBones())) return;
                Offsets[Index] = MeshToWorld.TransformPosition(Pose.GetComponentSpaceTransform(
                    Torso[Index].GetCompactPoseIndex(GetRequiredBones())).GetLocation()) - PivotWorld;
            }
            const auto OverlapAt = [&](double Angle)
            {
                const FQuat Turn(FVector(0, 1, 0), -Angle);
                double Overlap = -TNumericLimits<double>::Max();
                for (int32 Index = 0; Index < 3; ++Index)
                    Overlap = FMath::Max(Overlap, PivotWorld.X + Turn.RotateVector(Offsets[Index]).X
                        + SkinMargins[Index] - JumpLedgeBounds.Min.X);
                return Overlap;
            };
            double BestOverlap = OverlapAt(0.0);
            if (BestOverlap <= 0.0) return;

            // Keep the hips in place and retain the authored spine shape. Find the first
            // small backward bend that clears the chest/head, rather than flattening the pose.
            double Angle = 0.0;
            const double Step = FMath::DegreesToRadians(1.0);
            for (int32 Degree = 1; Degree <= 60; ++Degree)
            {
                const double Candidate = Degree * Step;
                const double Overlap = OverlapAt(Candidate);
                if (Overlap < BestOverlap)
                {
                    BestOverlap = Overlap;
                    Angle = Candidate;
                }
                if (Overlap <= 0.0)
                {
                    double Low = Candidate - Step;
                    double High = Candidate;
                    for (int32 Iteration = 0; Iteration < 10; ++Iteration)
                    {
                        const double Mid = (Low + High) * 0.5;
                        if (OverlapAt(Mid) > 0.0) Low = Mid;
                        else High = Mid;
                    }
                    Angle = High;
                    break;
                }
            }
            if (Angle <= 0.0) return;
            const FQuat WorldTurn(FVector(0, 1, 0), -Angle);
            const FQuat ComponentTurn = MeshToWorld.GetRotation().Inverse() * WorldTurn * MeshToWorld.GetRotation();
            SpineCS.SetRotation((ComponentTurn * SpineCS.GetRotation()).GetNormalized());
            const TArray<FBoneTransform> Adjusted = { FBoneTransform(SpineIndex, SpineCS) };
            Pose.LocalBlendCSBoneTransforms(Adjusted, 1.f);
        }

        void KeepLimbOutside(FCSPose<FCompactPose>& Pose, const FBoneReference& EndBone,
            const FBoneReference& TipBone, double TipLength)
        {
            if (!EndBone.IsValidToEvaluate(GetRequiredBones()) || !TipBone.IsValidToEvaluate(GetRequiredBones())) return;
            const FCompactPoseBoneIndex End = EndBone.GetCompactPoseIndex(GetRequiredBones());
            const FCompactPoseBoneIndex Joint = Pose.GetPose().GetParentBoneIndex(End);
            const FCompactPoseBoneIndex Upper = Pose.GetPose().GetParentBoneIndex(Joint);
            const FCompactPoseBoneIndex Tip = TipBone.GetCompactPoseIndex(GetRequiredBones());
            const FCompactPoseBoneIndex TipParent = Pose.GetPose().GetParentBoneIndex(Tip);
            FTransform UpperCS = Pose.GetComponentSpaceTransform(Upper);
            FTransform JointCS = Pose.GetComponentSpaceTransform(Joint);
            FTransform EndCS = Pose.GetComponentSpaceTransform(End);
            const FVector UpperWorld = MeshToWorld.TransformPosition(UpperCS.GetLocation());
            const FVector JointWorld = MeshToWorld.TransformPosition(JointCS.GetLocation());
            const FVector EndWorld = MeshToWorld.TransformPosition(EndCS.GetLocation());
            const FVector TipCS = Pose.GetComponentSpaceTransform(Tip).GetLocation();
            const FVector TipDirection = (TipCS - Pose.GetComponentSpaceTransform(TipParent).GetLocation()).GetSafeNormal();
            const FVector TipWorld = MeshToWorld.TransformPosition(TipCS);
            const FVector ContactWorld = MeshToWorld.TransformPosition(TipCS + TipDirection * TipLength);
            const double PlaneX = JumpLedgeBounds.Min.X - 8.0;
            const double TipOffsetX = FMath::Max3(0.0, TipWorld.X - EndWorld.X, ContactWorld.X - EndWorld.X);
            if (EndWorld.X + TipOffsetX <= PlaneX && JointWorld.X <= PlaneX) return;

            // Keep the animated wrist/ankle rotation, accounting for the skin beyond the
            // final finger/toe joint. Move only a limb that reaches toward the obstacle.
            const FQuat EndRotation = EndCS.GetRotation();
            FVector TargetWorld = EndWorld;
            TargetWorld.X = FMath::Min(TargetWorld.X, PlaneX - TipOffsetX);
            const double Reach = FVector::Distance(UpperWorld, JointWorld) + FVector::Distance(JointWorld, EndWorld) - 0.01;
            const FVector Pole = MeshToWorld.InverseTransformPosition(UpperWorld + FVector(-100.0, 0, 0));
            for (int32 Attempt = 0; Attempt < 3; ++Attempt)
            {
                // A target moved outward can become unreachable. Shorten its sideways/up
                // reach while retaining the safe depth, rather than stretching the bones.
                const FVector Delta = TargetWorld - UpperWorld;
                if (Delta.SizeSquared() > Reach * Reach && FMath::Abs(Delta.X) < Reach)
                {
                    const double Sideways = FMath::Sqrt(Delta.Y * Delta.Y + Delta.Z * Delta.Z);
                    const double Available = FMath::Sqrt(FMath::Max(0.0, Reach * Reach - Delta.X * Delta.X));
                    if (Sideways > UE_SMALL_NUMBER)
                    {
                        TargetWorld.Y = UpperWorld.Y + Delta.Y * Available / Sideways;
                        TargetWorld.Z = UpperWorld.Z + Delta.Z * Available / Sideways;
                    }
                }
                AnimationCore::SolveTwoBoneIK(UpperCS, JointCS, EndCS, Pole,
                    MeshToWorld.InverseTransformPosition(TargetWorld), false, 1.0, 1.0);
                EndCS.SetRotation(EndRotation);
                const double JointOverrun = MeshToWorld.TransformPosition(JointCS.GetLocation()).X - PlaneX;
                const double EndOverrun = MeshToWorld.TransformPosition(EndCS.GetLocation()).X + TipOffsetX - PlaneX;
                const double Overrun = FMath::Max(JointOverrun, EndOverrun);
                if (Overrun <= 0.01) break;
                TargetWorld.X -= Overrun + 0.5;
            }
            const TArray<FBoneTransform> Adjusted = {
                FBoneTransform(Upper, UpperCS), FBoneTransform(Joint, JointCS), FBoneTransform(End, EndCS) };
            Pose.LocalBlendCSBoneTransforms(Adjusted, 1.f);
        }

        virtual bool Evaluate(FPoseContext& Output) override
        {
            const bool bEvaluated = FAnimSingleNodeInstanceProxy::Evaluate(Output);
            if (!bEvaluated || (!bPlantHands && !bLockJumpDepth)) return bEvaluated;
            if (bLockJumpDepth && !Pelvis.IsValidToEvaluate(GetRequiredBones())) return bEvaluated;
            FCSPose<FCompactPose> Pose;
            Pose.InitPose(MoveTemp(Output.Pose));
            if (bLockJumpDepth && Pelvis.IsValidToEvaluate(GetRequiredBones()))
            {
                const FVector PelvisWorld = MeshToWorld.TransformPosition(Pose.GetComponentSpaceTransform(
                    Pelvis.GetCompactPoseIndex(GetRequiredBones())).GetLocation());
                const FVector Correction = MeshToWorld.InverseTransformVector(PelvisWorldTarget - PelvisWorld);
                FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(Pose, Output.Pose);
                // Root lock alone cannot remove translation authored on the pelvis.
                // Actor movement already supplies all travel. Keep the evaluated body
                // with that trajectory instead of applying the clip's travel twice.
                Output.Pose[FCompactPoseBoneIndex(0)].AddToTranslation(Correction);
                if (bClearJumpLimbs)
                {
                    Pose.InitPose(MoveTemp(Output.Pose));
                    KeepTorsoOutside(Pose);
                    for (int32 Side = 0; Side < 2; ++Side)
                    {
                        KeepLimbOutside(Pose, Hands[Side], Fingers[Side], 2.0);
                        KeepLimbOutside(Pose, Feet[Side], Toes[Side], 7.0);
                    }
                    FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(Pose, Output.Pose);
                }
                return true;
            }
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
        FBoneReference Pelvis;
        FBoneReference Spine;
        FBoneReference Torso[3];
        FBoneReference Fingers[2];
        FBoneReference Feet[2];
        FBoneReference Toes[2];
        FBox LedgeBounds = FBox(ForceInit);
        FBox JumpLedgeBounds = FBox(ForceInit);
        FTransform MeshToWorld;
        bool bPlantHands = false;
        bool bLockJumpDepth = false;
        bool bClearJumpLimbs = false;
        FVector PelvisWorldTarget = FVector::ZeroVector;
    };
}

FAnimInstanceProxy* UClimbHandIKAnimInstance::CreateAnimInstanceProxy() { return new FClimbHandProxy(this); }
void UClimbHandIKAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
