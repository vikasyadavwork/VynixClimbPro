#pragma once

#include "CoreMinimal.h"

// Shared by the game and the automation tests. Distances are Unreal centimetres.
namespace ClimbRunRules
{
    constexpr float LaneOffset = 190.f;
    constexpr float StepHeight = 180.f;
    constexpr float StartingHeight = 300.f;
    constexpr float JumpDuration = 0.58f;
    constexpr float MaxHealth = 100.f;
    constexpr float HitGraceSeconds = 1.35f;

    inline float LaneY(int32 Lane) { return Lane == 0 ? -LaneOffset : LaneOffset; }

    // The cliff faces -X. Bounds must already include the mesh pivot and instance scale.
    inline bool TryHangLocation(const FBox& Bounds, float DesiredY, float CapsuleRadius,
        float Clearance, float HandHeight, FVector& OutLocation)
    {
        if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN()
            || !FMath::IsFinite(DesiredY) || !FMath::IsFinite(CapsuleRadius)
            || !FMath::IsFinite(Clearance) || !FMath::IsFinite(HandHeight)
            || CapsuleRadius <= 0.f || Clearance < 0.f) return false;
        const double Margin = CapsuleRadius + Clearance;
        const FVector Size = Bounds.GetSize();
        if (Size.X <= 0.0 || Size.Z <= 0.0 || Size.Y < 2.0 * Margin) return false;
        OutLocation = FVector(Bounds.Min.X - Margin,
            FMath::Clamp(static_cast<double>(DesiredY), Bounds.Min.Y + Margin, Bounds.Max.Y - Margin),
            Bounds.Max.Z - HandHeight);
        return true;
    }

    inline FVector JumpPosition(const FVector& Start, const FVector& Target, float Alpha)
    {
        if (Alpha <= 0.f) return Start;
        if (Alpha >= 1.f) return Target;
        const float T = FMath::Clamp(Alpha, 0.f, 1.f);
        const float Ease = T * T * (3.f - 2.f * T);
        const float Arc = FMath::Sin(T * PI);
        // Travel across/up the cliff while retaining the grip's wall clearance.
        return FMath::Lerp(Start, Target, Ease) + FVector(0.f, 0.f, 75.f * Arc);
    }

    inline int32 Score(double HeightMetres, double Seconds, int32 JumpBonus)
    {
        // Saturate the displayed score, rather than overflowing during a very long run.
        const double Value = FMath::Max(0.0, HeightMetres) * 100.0
            + FMath::Max(0.0, Seconds) * 5.0 + FMath::Max(0, JumpBonus);
        return static_cast<int32>(FMath::Clamp(Value, 0.0, static_cast<double>(MAX_int32)));
    }

    inline float HealthAfterHit(float Health, float Damage)
    {
        if (!FMath::IsFinite(Damage) || Damage <= 0.f) return Health;
        return FMath::Clamp(Health - Damage, 0.f, MaxHealth);
    }
}
