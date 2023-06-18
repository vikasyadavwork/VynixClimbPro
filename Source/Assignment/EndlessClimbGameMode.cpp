#include "EndlessClimbGameMode.h"
#include "ClimbCapture.h"
#include "EndlessClimber.h"
#include "EndlessClimbHUD.h"
#include "EndlessClimbWorld.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"

AEndlessClimbGameMode::AEndlessClimbGameMode()
{
    DefaultPawnClass = AEndlessClimber::StaticClass();
    HUDClass = AEndlessClimbHUD::StaticClass();
}

void AEndlessClimbGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->SpawnActor<AEndlessClimbWorld>();
    ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 22000), FRotator(-35, 35, 0));
    if (Sun)
    {
        Sun->SetMobility(EComponentMobility::Movable);
        UDirectionalLightComponent* Light = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
        Light->SetIntensity(3.5f);
        Light->SetLightColor(FLinearColor(1.f, 0.86f, 0.70f));
        Light->SetAtmosphereSunLight(true);
    }
    ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>();
    if (Sky)
    {
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(1.2f);
        Sky->GetLightComponent()->SetRealTimeCapture(true);
    }
    AActor* Atmosphere = GetWorld()->SpawnActor<AActor>();
    if (Atmosphere)
    {
        USkyAtmosphereComponent* Component = NewObject<USkyAtmosphereComponent>(Atmosphere);
        Atmosphere->SetRootComponent(Component);
        Atmosphere->AddInstanceComponent(Component);
        Component->RegisterComponent();
    }
    AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 19000), FRotator::ZeroRotator);
    if (Fog)
    {
        Fog->GetComponent()->SetFogDensity(0.012f);
        Fog->GetComponent()->SetFogHeightFalloff(0.12f);
        Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.16f, 0.24f, 0.29f));
    }
    StartClimbCapture(GetWorld());
}
