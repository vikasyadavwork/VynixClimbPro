// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

AMyCharacter::AMyCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
 
	PrimaryActorTick.bCanEverTick = true;
	MyCharMovem = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);


	bUseControllerRotationPitch = bUseControllerRotationYaw = bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;


	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false;

	LeftTracerArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Left Tracer"));
	LeftTracerArrow->SetupAttachment(RootComponent);
	

	RightTracerArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Right Tracer"));
	RightTracerArrow->SetupAttachment(RootComponent);

	LeftLedgeArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Left Ledge Tracer"));
	LeftLedgeArrow->SetupAttachment(RootComponent);

	RightLedgeArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Right Ledge Tracer"));
	RightLedgeArrow->SetupAttachment(RootComponent);

	LeftTurnArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Left TUrn Tracer"));
	LeftTurnArrow->SetupAttachment(RootComponent);

	RightTurnArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Right Tunr Tracer"));
	RightTurnArrow->SetupAttachment(RootComponent);

	JumpUpArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Jump up Tracer"));
	JumpUpArrow->SetupAttachment(RootComponent);

	JumpDownArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Jump Down Tracer"));
	JumpDownArrow->SetupAttachment(RootComponent);

	PlayerUpArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("PLAYER Tracer"));
	PlayerUpArrow->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	MyCharMovem = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

// Called every frame
void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	
}

// Called to bind functionality to input
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		//Jumping
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMyCharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMyCharacter::StopJumping);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AMyCharacter::StopJumping);
		}

		//Moving
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyCharacter::Move);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AMyCharacter::StopMoving);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AMyCharacter::StopMoving);
		}

		//Looking
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyCharacter::Look);
		}

	}

}
void AMyCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	IsJumping = false;
	MyStamina = FMath::Clamp(MyStamina - 3.f, 0.f, 10.f);
	PlayerLanded();
}
//ASSIGNING CUSTOM JUMP
void AMyCharacter::Jump()
{
	//bPressedMyChaJump = true;
	if (MyCharMovem && MyCharMovem->IsHanging && !TurnBack) {
		if (MyCharMovem->CanJumpRight && Dpressed) {
			MyCharMovem->JumpRightLedge();
		}
		else if(MyCharMovem->CanJumpLeft && Apressed){
			MyCharMovem->JumpLeftLedge();
		}
		else if (MyCharMovem->CanJumpUp && Wpressed) {
			MyCharMovem->JumpUpLedge();
		}
		else if (MyCharMovem->CanJumpDown && Spressed) {
			MyCharMovem->JumpDownLedge();
		}
		else {
			MyCharMovem->ClimbLedgeCustomEvent();
		}
	}
	else {
		Super::Jump();
	}
	
	

//	bPressedJump = false;

}


//STOPING JUMP
void AMyCharacter::StopJumping()
{
	
	//bPressedMyChaJump = false;
	Super::StopJumping();
}

// Moving via AddMovementInput

void AMyCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();
	PlayerMovementVector = MovementVector;
	Wpressed = MovementVector.Y > KINDA_SMALL_NUMBER;
	Spressed = MovementVector.Y < -KINDA_SMALL_NUMBER;
	Apressed = MovementVector.X < -KINDA_SMALL_NUMBER;
	Dpressed = MovementVector.X > KINDA_SMALL_NUMBER;
	
	if (Controller != nullptr )
	{
		
		if (!MyCharMovem || (!MyCharMovem->IsHanging && !MyCharMovem->IsHangingPoint)) {
			// find out which way is forward
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			// get forward vector
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

			// get right vector 
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	