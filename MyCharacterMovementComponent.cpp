#include "Character/Components/MyCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Enums/EImpulseMovementMode.h"
#include "Enums/EGrappleHookState.h"
#include "Enums/EWallRunSide.h"
#include "Character/ImpulseDefaultCharacter.h"
#include "Character/Abilities/Movement/GrappleHook.h"
#include "Character/Abilities/Movement/GrappleHookCable.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

#pragma region class MyCharacterMovementComponent

UMyCharacterMovementComponent::UMyCharacterMovementComponent()
{
	NavAgentProps.bCanCrouch = true;
	NavAgentProps.bCanSwim = false;
	bCanWalkOffLedgesWhenCrouching = true;
	
	MaxWalkSpeed = DefaultMaxRunSpeed;
	MaxAcceleration = DefaultMaxRunAcceleration;
	GroundFriction = DefaultGroundFriction;
	BrakingDecelerationWalking = DefaultBrakingDecelerationWalking;
	
	GravityScale = DefaultGravityScale;
	JumpZVelocity = DefaultJumpZVelocity;
	AirControl = DefaultAirControl;
	AirControlBoostMultiplier = DefaultAirControlBoostMultiplier;
	AirControlBoostVelocityThreshold = DefaultAirControlBoostVelocityThreshold;
	FallingLateralFriction = DefaultFallingLateralFriction;

	CurrentMaxSprintSpeed = DefaultMaxSprintSpeed;
	CurrentMaxSprintAcceleration = DefaultMaxSprintAcceleration;
	MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched;

	CurrentMaxRunSpeed = DefaultMaxRunSpeed;
	CurrentMaxRunAcceleration = DefaultMaxRunAcceleration;
	CurrentGrappleHookState = GRAPPLE_Ready;
}

#pragma region Jumping Functions

void UMyCharacterMovementComponent::SetJumping(bool bJumped)
{
	if (bJumped != WantsToJump)
	{		
		WantsToJump = bJumped;
		WallRunKeysDown = WantsToJump;

		if (bJumped)
		{
			Jumped = true;
			ServerSetJumping(true);
			FTimerHandle FJumped;
			GetWorld()->GetTimerManager().SetTimer(FJumped, this, &UMyCharacterMovementComponent::JumpedFalse, 0.1f, false);
		}
	}
}

void UMyCharacterMovementComponent::ServerSetJumping_Implementation(bool bJumped)
{
	MultiSetJumping(bJumped);
}

bool UMyCharacterMovementComponent::ServerSetJumping_Validate(bool bJumped)
{
	return true;
}

void UMyCharacterMovementComponent::MultiSetJumping_Implementation(bool bJumped)
{
	Jumped = bJumped;
}

bool UMyCharacterMovementComponent::MultiSetJumping_Validate(bool bJumped)
{
	return true;
}

void UMyCharacterMovementComponent::JumpedFalse()
{
	Jumped = false;
	ServerSetJumping(false);
}

#pragma endregion

#pragma region Sprinting Functions

void UMyCharacterMovementComponent::SetSprinting(const bool Sprinting)
{
	WantsToSprint = Sprinting;
	//WallRunKeysDown = WantsToSprint;
	
	if (!Sprinting)
		EndSlide();
}

bool UMyCharacterMovementComponent::IsMovingForward() const
{
	if (!PawnOwner)
		return false;
	
	FVector Forward = PawnOwner->GetActorForwardVector();
	FVector MoveDir = Velocity.GetSafeNormal();

	//Ignore vertical movement
	Forward.Z = 0.0f;
	MoveDir.Z = 0.0f;

	Forward.Normalize();
	MoveDir.Normalize();
	
	return FVector::DotProduct(Forward, MoveDir) > 0.5f;
}

#pragma endregion

#pragma region Crouching Functions

void UMyCharacterMovementComponent::BeginCrouch_Implementation()
{
	WantsToCrouch = true;
	bWantsToCrouch = true; //built in crouch bool
	SlideKeysDown = AreRequiredSlideKeysDown();
	CheckSlide();	
}

void UMyCharacterMovementComponent::EndCrouch_Implementation()
{
	WantsToCrouch = false;
	bWantsToCrouch = false; //built in crouch bool
	SlideKeysDown = AreRequiredSlideKeysDown();
	EndSlide();
}

#pragma endregion

#pragma region Sliding Functions

void UMyCharacterMovementComponent::BeginSlide()
{
	if (SlideKeysDown && CanSlide && IsMovingForward())
	{
		GroundFriction = 0.f;

		if (IsStimmy)
			MaxWalkSpeedCrouched = MaxSlideSpeed * StimmySpeedMultiplier;
		else
			MaxWalkSpeedCrouched = MaxSlideSpeed;
		
		CanSlide = true;
		
		if (PawnOwner->GetLocalRole() < ROLE_Authority)
			ServerBeginSlide();
		
		GetWorld()->GetTimerManager().SetTimer(SlideTimerHandle, this, &UMyCharacterMovementComponent::DoSlide, 0.5f, true);
	}
}

void UMyCharacterMovementComponent::EndSlide()
{	
	CanSlide = false;

	GroundFriction = DefaultGroundFriction;
	
	if (IsStimmy)
		MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched * StimmySpeedMultiplier;
	else
		MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched;
	
	if (PawnOwner->GetLocalRole() < ROLE_Authority)
		ServerEndSlide();
	
	GetWorld()->GetTimerManager().ClearTimer(SlideTimerHandle);
	FTimerHandle SlideCooldown;
	GetWorld()->GetTimerManager().SetTimer(SlideCooldown, this, &UMyCharacterMovementComponent::AllowSlide, 0.5f, false);
}

void UMyCharacterMovementComponent::AllowSlide()
{
	if (!SlideKeysDown)
		CanSlide = true;
}

void UMyCharacterMovementComponent::CheckSlide()
{
	if (SlideKeysDown)
		BeginSlide();
}

bool UMyCharacterMovementComponent::AreRequiredSlideKeysDown() const
{
	if (GetPawnOwner()->IsLocallyControlled() == false)
		return false;

	if (WantsToSprint && WantsToCrouch)
		return true;
	
	
	return false;
}

void UMyCharacterMovementComponent::DoSlide()
{
	if (SlideKeysDown && CanSlide && IsMovingForward())
	{
		const FVector FloorNormal = CurrentFloor.HitResult.Normal;
		
		const FVector Force = CalcFloorInfluence(FloorNormal);
		AddForce(Force);
		
		const FVector NormalSpeed = UKismetMathLibrary::Normal(GetLastUpdateVelocity());
		
		if (!(UKismetMathLibrary::Dot_VectorVector(FloorNormal, FVector(NormalSpeed.X, NormalSpeed.Y, 0.f)) > 0.1f))
		{
			EndSlide();
		}
	}
	else
	{
		EndSlide();
	}
}

FVector UMyCharacterMovementComponent::CalcFloorInfluence(const FVector FloorNormal)
{
	const FVector VectorUp = FVector(0.f, 0.f, 1.f);
	
	if (UKismetMathLibrary::EqualEqual_VectorVector(FloorNormal, VectorUp, 0.0001f))
	{
		return FVector(0.f, 0.f, 0.f);
	}
	
	const FVector FloorDirection = UKismetMathLibrary::Cross_VectorVector(FloorNormal, UKismetMathLibrary::Cross_VectorVector(FloorNormal, VectorUp));
	const FVector FloorDirectionNormal = UKismetMathLibrary::Normal(FloorDirection, 0.001f);
	
	const float SlopeScale = UKismetMathLibrary::FClamp(1 - UKismetMathLibrary::Dot_VectorVector(FloorNormal, VectorUp), 0.f, 1.f);
	
	return (SlopeScale * FloorDirectionNormal);
}

void UMyCharacterMovementComponent::SlideCameraRotate() const
{
	if (GetPawnOwner()->IsLocallyControlled())
	{
		if (GetController())
		{
			const AActor* Actor = Cast<AActor>(GetOwner());
			const FVector RightVector = Actor->GetActorRightVector();
			const FVector VelocityNormal = UKismetMathLibrary::Normal(Velocity, 0.001f);
			const float Dot = UKismetMathLibrary::Dot_VectorVector(RightVector, VelocityNormal);
			
			const FVector2d Velocity2D = UKismetMathLibrary::Conv_VectorToVector2D(Velocity);
			const float ClampedRange = UKismetMathLibrary::MapRangeClamped(Velocity2D.Length(), 0.f, 1200.f, 0.f, 1.f);

			const float XRotation = ClampedRange * -10.f * Dot;
			const FRotator Target = FRotator(GetController()->GetControlRotation().Pitch, GetController()->GetControlRotation().Yaw, XRotation);
			const FRotator NewRotation = UKismetMathLibrary::RInterpTo(GetController()->GetControlRotation(), Target, GetWorld()->DeltaTimeSeconds, 10.f);
			
			GetController()->SetControlRotation(NewRotation);
		}
	}
}

void UMyCharacterMovementComponent::ServerBeginSlide_Implementation()
{
	IsSliding = true;
	GroundFriction = 0.f;

	if (IsStimmy)
		MaxWalkSpeedCrouched = MaxSlideSpeed * StimmySpeedMultiplier;
	else
		MaxWalkSpeedCrouched = MaxSlideSpeed;
	
	MultiSetIsSliding(IsSliding);
}

bool UMyCharacterMovementComponent::ServerBeginSlide_Validate()
{
	return true;
}

void UMyCharacterMovementComponent::ServerEndSlide_Implementation()
{
	IsSliding = false;
	GroundFriction = DefaultGroundFriction;
	
	if (IsStimmy)
		MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched * StimmySpeedMultiplier;
	else
		MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched;	
	
	MultiSetIsSliding(IsSliding);
}

bool UMyCharacterMovementComponent::ServerEndSlide_Validate()
{
	return true;
}

void UMyCharacterMovementComponent::MultiSetIsSliding_Implementation(const bool Sliding)
{
	IsSliding = Sliding;
}

bool UMyCharacterMovementComponent::MultiSetIsSliding_Validate(const bool Sliding)
{
	return true;
}

#pragma endregion

#pragma region Wall Running Functions

void UMyCharacterMovementComponent::ServerSetWallRunSide_Implementation(EWallRunSide WRSide)
{
	MultiSetWallRunSide(WRSide);
}

bool UMyCharacterMovementComponent::ServerSetWallRunSide_Validate(EWallRunSide WRSide)
{
	return true;
}

void UMyCharacterMovementComponent::MultiSetWallRunSide_Implementation(EWallRunSide WRSide)
{
	WallRunSide = WRSide;
}

bool UMyCharacterMovementComponent::MultiSetWallRunSide_Validate(EWallRunSide WRSide)
{
	return true;
}

bool UMyCharacterMovementComponent::BeginWallRun()
{
	// Only allow wall running to begin if the required keys are down
	if (WallRunKeysDown == true)
	{
		// Set the movement mode to wall running. UE4 will handle replicating this change to all connected clients.
		SetMovementMode(MOVE_Custom, CMOVE_WallRunning);
		
		return true;
	}

	return false;
}

void UMyCharacterMovementComponent::WallRunCameraRotate(const float Roll) const
{
	const float CameraPitch = GetController()->GetControlRotation().Pitch;
	const float CameraYaw = GetController()->GetControlRotation().Yaw;
	const FRotator NewRotation = FRotator(CameraPitch, CameraYaw, Roll);

	GetController()->SetControlRotation(FMath::RInterpTo(GetController()->GetControlRotation(), NewRotation, GetWorld()->DeltaTimeSeconds, 10.f));
}

void UMyCharacterMovementComponent::CameraTick() const
{
	if (IsSliding)
		SlideCameraRotate();
	
	if (!IsCustomMovementMode(CMOVE_WallRunning))
	{
		WallRunCameraRotate(0.f);
		return;
	}
	
	if (WallRunSide == kRight)
	{
		WallRunCameraRotate(15.f);
	}
	else if (WallRunSide == kLeft)
	{
		WallRunCameraRotate(-15.f);
	}
}

void UMyCharacterMovementComponent::WallRunJump()
{
	EndWallRun();
	AImpulseDefaultCharacter* Player = Cast<AImpulseDefaultCharacter>(GetOwner());
	Player->LaunchCharacter(FVector(WallRunNormal.X * HorizontalWallJumpOffForce, WallRunNormal.Y * HorizontalWallJumpOffForce, VerticalWallJumpOffForce), false, true);

	Jumped = true;
	ServerSetJumping(true);
	FTimerHandle FJumped;
	GetWorld()->GetTimerManager().SetTimer(FJumped, this, &UMyCharacterMovementComponent::JumpedFalse, 0.1f, false);
}

void UMyCharacterMovementComponent::EndWallRun()
{
	// Set the movement mode back to falling
	SetMovementMode(MOVE_Falling);
	WallRunSide = kStraight;
	ServerSetWallRunSide(kStraight);
}

bool UMyCharacterMovementComponent::AreRequiredWallRunKeysDown() const
{
	// Since this function is checking for input, it should only be called for locally controlled character
	if (GetPawnOwner()->IsLocallyControlled() == false)
		return false;
	
	/*if (WantsToSprint)
		return true;*/

	if(WantsToJump)
		return true;
	
	return false;
}

bool UMyCharacterMovementComponent::IsNextToWall(float VerticalTolerance) const
{
	// Do a line trace from the player into the wall to make sure we're still along the side of a wall
	FVector CrossVector = WallRunSide == kLeft ? FVector(0.0f, 0.0f, -1.0f) : FVector(0.0f, 0.0f, 1.0f);
	FVector TraceStart = GetPawnOwner()->GetActorLocation() + (WallRunDirection * 20.0f);
	FVector TraceEnd = TraceStart + (FVector::CrossProduct(WallRunDirection, CrossVector) * 100);
	FHitResult HitResult;

	// Create a helper lambda for performing the line trace
	auto LineTrace = [&](const FVector& Start, const FVector& End)
	{
		return (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility));
	};

	// If a vertical tolerance was provided we want to do two line traces - one above and one below the calculated line
	if (VerticalTolerance > FLT_EPSILON)
	{
		// If both line traces miss the wall then return false, we're not next to a wall
		if (LineTrace(FVector(TraceStart.X, TraceStart.Y, TraceStart.Z + VerticalTolerance / 2.0f), FVector(TraceEnd.X, TraceEnd.Y, TraceEnd.Z + VerticalTolerance / 2.0f)) == false &&
			LineTrace(FVector(TraceStart.X, TraceStart.Y, TraceStart.Z - VerticalTolerance / 2.0f), FVector(TraceEnd.X, TraceEnd.Y, TraceEnd.Z - VerticalTolerance / 2.0f)) == false)
		{
			return false;
		}
	}
	// If no vertical tolerance was provided we just want to do one line trace using the calculated line
	else
	{
		// return false if the line trace misses the wall
		if (LineTrace(TraceStart, TraceEnd) == false)
			return false;
	}

	// Make sure we're still on the side of the wall we expect to be on
	//EWallRunSide newWallRunSide;
	//FindWallRunDirectionAndSide(hitResult.ImpactNormal, WallRunDirection, newWallRunSide);
	//if (newWallRunSide != WallRunSide)
	//{
		//return false;
	//}
	return true;
}

void UMyCharacterMovementComponent::FindWallRunDirectionAndSide(const FVector& SurfaceNormal, FVector& Direction, EWallRunSide& Side)
{
	//if (IsCustomMovementMode(ECustomMovementMode::WallRunning)) { return; }
	
	FVector CrossVector;

	if (FVector2D::DotProduct(FVector2D(SurfaceNormal), FVector2D(GetPawnOwner()->GetActorRightVector())) > 0.0)
	{
		Side = kRight;
		ServerSetWallRunSide(Side);
		CrossVector = FVector(0.0f, 0.0f, 1.0f);
	}
	else
	{
		Side = kLeft;
		ServerSetWallRunSide(Side);
		CrossVector = FVector(0.0f, 0.0f, -1.0f);
	}

	// Find the direction parallel to the wall in the direction the player is moving
	Direction = FVector::CrossProduct(SurfaceNormal, CrossVector);
}

bool UMyCharacterMovementComponent::CanSurfaceBeWallRan(const FVector& SurfaceNormal) const
{
	// Return false if the surface normal is facing down
	if (SurfaceNormal.Z < -0.05f)
		return false;

	FVector NormalNoZ = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0.0f);
	NormalNoZ.Normalize();

	// Find the angle of the wall
	const float WallAngle = FMath::Acos(FVector::DotProduct(NormalNoZ, SurfaceNormal));

	// Return true if the wall angle is less than the walkable floor angle
	return WallAngle < GetWalkableFloorAngle();
}

bool UMyCharacterMovementComponent::IsCustomMovementMode(const uint8 CustomMove) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == CustomMove;
}

void UMyCharacterMovementComponent::OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsCustomMovementMode(CMOVE_WallRunning))
		return;

	// Make sure we're falling. Wall running can only begin if we're currently in the air
	if (IsFalling() == false)
		return;

	// Make sure the surface can be wall ran based on the angle of the surface that we hit
	if (CanSurfaceBeWallRan(Hit.ImpactNormal) == false)
		return;

	// Update the wall run direction and side
	FindWallRunDirectionAndSide(Hit.ImpactNormal, WallRunDirection, WallRunSide);

	// Make sure we're next to a wall
	if (IsNextToWall() == false)
		return;
	

	WallRunNormal = Hit.ImpactNormal;
	BeginWallRun();
}

void UMyCharacterMovementComponent::PhysWallRunning(float DeltaTime, int32 Iterations)
{
	// Make sure the required wall run keys are still down
	if (WallRunKeysDown == false)
	{
		WallRunJump();
		return;
	}

	// Make sure we're still next to a wall. Provide a vertical tolerance for the line trace since it's possible the the server has
	// moved our character slightly since we've began the wall run. In the event we're right at the top/bottom of a wall we need this
	// tolerance value so we don't immediately fall of the wall 
	if (IsNextToWall(LineTraceVerticalTolerance) == false)
	{
		EndWallRun();
		return;
	}

	// Set the owning player's new velocity based on the wall run direction
	FVector newVelocity = WallRunDirection;
	if (IsStimmy)
	{
		newVelocity.X *= (WallRunSpeed * StimmySpeedMultiplier);
		newVelocity.Y *= (WallRunSpeed * StimmySpeedMultiplier);
	}
	else
	{
		newVelocity.X *= WallRunSpeed;
		newVelocity.Y *= WallRunSpeed;
	}
	newVelocity.Z *= 0.0f;
	Velocity = newVelocity;

	const FVector Adjusted = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
}

#pragma endregion

#pragma region Blink Functions

void UMyCharacterMovementComponent::DoDodge()
{
	if (CanDodge)
		bWantsToDodge = true;
}

void UMyCharacterMovementComponent::EndDodge()
{
	StopMovementImmediately();
	GroundFriction = DefaultGroundFriction;
	
	FTimerHandle DodgeCooldown;
	GetWorld()->GetTimerManager().SetTimer(DodgeCooldown, this, &UMyCharacterMovementComponent::AllowDodge, BlinkCooldown, false);
}

void UMyCharacterMovementComponent::AllowDodge()
{
	CanDodge = true;
}

#pragma endregion

#pragma region Stimmy Functions

void UMyCharacterMovementComponent::StartStimmy()
{
	if (CanStimmy)
	{
		IsStimmy = true;
		ServerSetStimmy(IsStimmy);
		CanStimmy = false;
		CurrentMaxRunSpeed *= StimmySpeedMultiplier;
		DefaultMaxSprintSpeed *= StimmySpeedMultiplier;
		MaxWalkSpeedCrouched *= StimmySpeedMultiplier;
		ServerSetStimmySpeed();
		FTimerHandle FStimmyDuration;
		GetWorld()->GetTimerManager().SetTimer(FStimmyDuration, this, &UMyCharacterMovementComponent::EndStimmy, StimmyDuration, false);
	}
}

void UMyCharacterMovementComponent::ServerSetStimmySpeed_Implementation()
{
	CurrentMaxRunSpeed *= StimmySpeedMultiplier;
	CurrentMaxSprintSpeed *= StimmySpeedMultiplier;
	MaxWalkSpeedCrouched *= StimmySpeedMultiplier;
}

void UMyCharacterMovementComponent::ServerEndStimmySpeed_Implementation()
{
	CurrentMaxRunSpeed = DefaultMaxRunSpeed;
	CurrentMaxSprintSpeed = DefaultMaxSprintSpeed;
	MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched;
}

void UMyCharacterMovementComponent::EndStimmy()
{
	IsStimmy = false;
	ServerSetStimmy(IsStimmy);
	CurrentMaxRunSpeed = DefaultMaxRunSpeed;
	CurrentMaxSprintSpeed = DefaultMaxSprintSpeed;
	MaxWalkSpeedCrouched = DefaultMaxWalkSpeedCrouched;
	ServerEndStimmySpeed();
	FTimerHandle FStimmyCooldown;
	GetWorld()->GetTimerManager().SetTimer(FStimmyCooldown, this, &UMyCharacterMovementComponent::ResetStimmy, StimmyCooldown, false);
}

void UMyCharacterMovementComponent::ResetStimmy()
{
	CanStimmy = true;
}

void UMyCharacterMovementComponent::ServerSetStimmy_Implementation(bool bStimmy)
{
	IsStimmy = bStimmy;
}

#pragma endregion

#pragma region Slide Jump Functions

void UMyCharacterMovementComponent::SlideJump()
{
	if (CanSlideJump && IsSliding && MovementMode != MOVE_Falling)
	{
		CanSlideJump = false;
		ServerSlideJump();

		FTimerHandle FSlideJumpCooldown;
		GetWorld()->GetTimerManager().SetTimer(FSlideJumpCooldown, this, &UMyCharacterMovementComponent::AllowSlideJump, SlideJumpCooldown, false);
	}
}

void UMyCharacterMovementComponent::AllowSlideJump()
{
	CanSlideJump = true;
}

void UMyCharacterMovementComponent::ServerSlideJump_Implementation()
{
	if (IsSliding && MovementMode != MOVE_Falling)
	{
		MoveDirection.Normalize();
		FVector SlideJumpVel = MoveDirection * HorizontalSlideJumpForce;
		SlideJumpVel.Z = VerticalSlideJumpForce;
		Launch(SlideJumpVel);
		GravityScale = SlideJumpGravityScale;
	}
}

#pragma endregion

#pragma region Grapple Hook Functions

void UMyCharacterMovementComponent::FireGrapple(FVector TargetLocation, FVector LocalOffset)
{
	if (CanGrapple)
	{
		if (!IsGrappleInUse())
		{
			const FVector CableStart = GetOwner()->GetActorLocation() + UKismetMathLibrary::TransformDirection(GetOwner()->GetActorTransform(), LocalOffset);
			const FVector FiringDirection = (TargetLocation - CableStart).GetSafeNormal();

			CableStartLocation = CableStart;
			ServerFireGrapple(FiringDirection, CableStart);
		
			SetGrappleHookState(GRAPPLE_Firing);
		}
		else
		{
			// End Grapple? Launch?
			ServerCancelGrapple();
		}
	}
}

void UMyCharacterMovementComponent::ServerFireGrapple_Implementation(const FVector FiringDirection, const FVector CableStart)
{
	if (GetOwner()->HasAuthority())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetPawnOwner();
		
		GrappleHook = GetWorld()->SpawnActor<AGrappleHook>(GrappleHookClass, CableStart, FRotator(0.f, 0.f, 0.f), SpawnParams);
		GrappleHook->FireGrappleHook(FiringDirection);
		
		//GrappleCable = GetWorld()->SpawnActor<AGrappleHookCable>(GrappleCableClass, GrappleHook->GetActorLocation(), FRotator(UKismetMathLibrary::MakeRotFromX(FiringDirection)), SpawnParams);
		//GrappleCable->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
		GrappleCable = GetWorld()->SpawnActor<AGrappleHookCable>(GrappleCableClass, SpawnParams);
		GrappleCable->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
		GrappleCable->SetActorLocation(GrappleHook->GetActorLocation());
	}
}

bool UMyCharacterMovementComponent::ServerFireGrapple_Validate(FVector FiringDirection, FVector CableStart)
{
	return true;
}

void UMyCharacterMovementComponent::ServerCancelGrapple_Implementation()
{
	if (GrappleHook)
		GrappleHook->Destroy();
}

bool UMyCharacterMovementComponent::ServerCancelGrapple_Validate()
{
	return true;
}

void UMyCharacterMovementComponent::OnGrappleHookHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                                     UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (GetOwner()->GetLocalRole() > ROLE_SimulatedProxy)
	{
		if (GrappleHook)
		{
			SetGrappleHookState(GRAPPLE_Attached);

			FVector Direction = (GrappleHook->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
			InitialHookDirection2D.X = Direction.X;
			InitialHookDirection2D.Y = Direction.Y;
			InitialHookDirection2D.Z = 0.f;

			GroundFriction = 0.f;
			GravityScale = 0.f;

			Velocity = Direction * InstantaneousVelocityFromGrapple;
		}
	}
}

void UMyCharacterMovementComponent::OnGrappleHookDestroyed(AActor* DestroyedActor)
{	
	if (GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		if (GrappleCable)
			GrappleCable->Destroy();
	}

	if (GetOwner()->GetLocalRole() >ROLE_SimulatedProxy)
	{
		SetGrappleHookState(GRAPPLE_Ready);

		GroundFriction = DefaultGroundFriction;
		GravityScale = DefaultGravityScale;
	}

	CanGrapple = false;
	FTimerHandle FGrappleCooldown;
	GetWorld()->GetTimerManager().SetTimer(FGrappleCooldown, this, &UMyCharacterMovementComponent::AllowGrapple, GrappleCooldown, false);
}

void UMyCharacterMovementComponent::SetGrappleHookState(EGrappleHookState NewGrappleHookState)
{
	if (CurrentGrappleHookState != NewGrappleHookState)
	{
		CurrentGrappleHookState = NewGrappleHookState;
		ServerSetGrappleHookState(CurrentGrappleHookState);
	}
}

void UMyCharacterMovementComponent::ServerSetGrappleHookState_Implementation(EGrappleHookState NewGrappleHookState)
{
	CurrentGrappleHookState = NewGrappleHookState;
}

bool UMyCharacterMovementComponent::ServerSetGrappleHookState_Validate(EGrappleHookState NewGrappleHookState)
{
	return true;
}

bool UMyCharacterMovementComponent::IsGrappleHookState(EGrappleHookState GrappleHookState)
{
	return CurrentGrappleHookState == GrappleHookState;
}

bool UMyCharacterMovementComponent::IsGrappleInUse()
{
	return IsGrappleHookState(GRAPPLE_Attached) || IsGrappleHookState(GRAPPLE_Firing);
}

void UMyCharacterMovementComponent::AllowGrapple()
{
	CanGrapple = true;
}

void UMyCharacterMovementComponent::GrappleCableTick()
{
	if (GetPawnOwner()->GetLocalRole() == ROLE_Authority)
	{
		GrappleCable->FollowGrappleHook(GrappleHook, CableStartLocation);

		if (GrappleHook)
			if (UKismetMathLibrary::Vector_Distance(GetOwner()->GetActorLocation(), GrappleHook->GetActorLocation()) > GrappleDistance)
				GrappleHook->Destroy();
	}
}

#pragma endregion 

#pragma region Movement Overrides

void UMyCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	WallRunSide = kStraight;
	// We don't want simulated proxies detecting their own collision
	if (GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
	{
		// Bind to the OnActorHot component so we're notified when the owning actor hits something (like a wall)
		GetPawnOwner()->OnActorHit.AddDynamic(this, &UMyCharacterMovementComponent::OnActorHit);
	}
}

void UMyCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UMyCharacterMovementComponent, IsSliding);
	
	//DOREPLIFETIME(UMyCharacterMovementComponent, IsStimmy); ONLY NEED FOR ANIMATION REPLICATION TO OTHER CLIENTS
}

FNetworkPredictionData_Client* UMyCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		// Return our custom client prediction class instead
		UMyCharacterMovementComponent* MutableThis = const_cast<UMyCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_My(*this);
	}

	return ClientPredictionData;
}

void UMyCharacterMovementComponent::SetImpulseMovementMode(const EImpulseMovementMode NewMovementMode)
{
	if (NewMovementMode == ImpulseMovementMode)
		return;
	
	ImpulseMovementMode = NewMovementMode;

	if (PawnOwner->GetLocalRole() > ROLE_SimulatedProxy)
		ServerSetImpulseMovementMode(NewMovementMode);
}

void UMyCharacterMovementComponent::ServerSetImpulseMovementMode_Implementation(EImpulseMovementMode NewMoveMode)
{
	MultiSetImpulseMovementMode(NewMoveMode);
}

bool UMyCharacterMovementComponent::ServerSetImpulseMovementMode_Validate(EImpulseMovementMode NewMoveMode)
{
	return true;
}

void UMyCharacterMovementComponent::MultiSetImpulseMovementMode_Implementation(EImpulseMovementMode NewMoveMode)
{
	ImpulseMovementMode = NewMoveMode;
}

bool UMyCharacterMovementComponent::MultiSetImpulseMovementMode_Validate(EImpulseMovementMode NewMoveMode)
{
	return true;
}

void UMyCharacterMovementComponent::OnComponentDestroyed(bool DestroyingHierarchy)
{
	if (GetPawnOwner() != nullptr && GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
	{
		// Unbind from all events
		GetPawnOwner()->OnActorHit.RemoveDynamic(this, &UMyCharacterMovementComponent::OnActorHit);
	}

	Super::OnComponentDestroyed(DestroyingHierarchy);
}

void UMyCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Perform local only checks
	if (GetPawnOwner()->IsLocallyControlled())
		CameraTick();
	
	if (IsGrappleInUse())
		GrappleCableTick();
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMyCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	/*  There are 4 custom move flags for us to use. Below is what each is currently being used for:
		FLAG_Custom_0		= 0x10, // Sprinting
		FLAG_Custom_1		= 0x20, // WallRunning
		FLAG_Custom_2		= 0x40, // Unused
		FLAG_Custom_3		= 0x80, // Unused
	*/

	// Read the values from the compressed flags
	WantsToSprint = (Flags & FSavedMove_MyMovement::FLAG_Sprint) != 0;
	WallRunKeysDown = (Flags & FSavedMove_MyMovement::FLAG_WallRun) != 0;
	bWantsToDodge = (Flags & FSavedMove_MyMovement::FLAG_3) != 0;
}

void UMyCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (PreviousMovementMode == MovementMode && PreviousCustomMode == CustomMovementMode)
	{
		Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
		return;
	}
	
#pragma region Leaving State Handlers
	
	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_WallRunning)
	{
		GravityScale = DefaultGravityScale; //in case slide jump to a wall
		bConstrainToPlane = false;
	}
	
#pragma endregion
	
#pragma region Entering State Handlers

	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		SetImpulseMovementMode(CMOVE_Grounded);
	}
	if (MovementMode == MOVE_Falling)
	{
		SetImpulseMovementMode(CMOVE_InAir);
	}
	
	if (IsCustomMovementMode(CMOVE_WallRunning))
	{
		SetImpulseMovementMode(CMOVE_WallRunning);
		StopMovementImmediately();
		bConstrainToPlane = true;
		SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 1.0f));
	}

#pragma endregion 

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	
}

void UMyCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation,
	const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (!CharacterOwner)
		return;
	
	if (PawnOwner->IsLocallyControlled())
		MoveDirection = PawnOwner->GetLastMovementInputVector();
	
	if (GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
		ServerSetMoveDirection(MoveDirection);

	//Update dodge movement
	if (bWantsToDodge && CanDodge)
	{
		CanDodge = false;
		MoveDirection.Normalize();
		FVector DodgeVel = MoveDirection * BlinkStrength;
		DodgeVel.Z = 0.0f;
		
		bWantsToDodge = false;
		GroundFriction = 0.f;
		Launch(DodgeVel);
		FTimerHandle StoppingMovement;
		GetWorld()->GetTimerManager().SetTimer(StoppingMovement, this, &UMyCharacterMovementComponent::EndDodge, BlinkDuration, false);
	}

	if (CurrentGrappleHookState == GRAPPLE_Attached)
	{
		if (GrappleHook)
		{			
			FVector Direction = (GrappleHook->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();

			AddForce(Direction * GrapplePullForce);
			
			float DistanceFromHook = UKismetMathLibrary::Vector_Distance(GetOwner()->GetActorLocation(), GrappleHook->GetActorLocation());
			if (DistanceFromHook < 250.f)
				GrappleHook->Destroy();
			else if (UKismetMathLibrary::Dot_VectorVector(InitialHookDirection2D, Direction) < 0.f)
				GrappleHook->Destroy();
		}
	}
}

void UMyCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	// Phys* functions should only run for characters with ROLE_Authority or ROLE_AutonomousProxy. However, Unreal calls PhysCustom in
	// two separate locations, one of which doesn't check the role, so we must check it here to prevent this code from running on simulated proxies.
	if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
		return;

	switch (CustomMovementMode)
	{
		case CMOVE_WallRunning:
			{
				PhysWallRunning(deltaTime, Iterations);
				break;
			}
		default:
			{
				break;
			}
	}

	// Not sure if this is needed
	Super::PhysCustom(deltaTime, Iterations);
}

float UMyCharacterMovementComponent::GetMaxSpeed() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	{
		if (IsCrouching())
		{
			return MaxWalkSpeedCrouched;
		}
			
		if (WantsToSprint && IsMovingForward())
		{
			return CurrentMaxSprintSpeed;
		}
			
		return CurrentMaxRunSpeed;
	}
	case MOVE_Falling:
		return CurrentMaxRunSpeed;
	case MOVE_Swimming:
		return MaxSwimSpeed;
	case MOVE_Flying:
		return MaxFlySpeed;
	case MOVE_Custom:
		return MaxCustomMovementSpeed;
	case MOVE_None:
	default:
		return 0.f;
	}
}

float UMyCharacterMovementComponent::GetMaxAcceleration() const
{
	if (IsMovingOnGround())
	{
		if (WantsToSprint && IsMovingForward())
		{
			return DefaultMaxSprintAcceleration;
		}

		return CurrentMaxRunAcceleration;
	}

	return Super::GetMaxAcceleration();
}

void UMyCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, remainingTime, Iterations);

	// If we landed while wall running, make sure we stop wall running
	if (IsCustomMovementMode(CMOVE_WallRunning))
	{
		EndWallRun();
	}

	GravityScale = DefaultGravityScale;
}

bool UMyCharacterMovementComponent::CanAttemptJump() const
{
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
}

#pragma endregion

#pragma region Compressed Flag Functions

void UMyCharacterMovementComponent::ServerSetMoveDirection_Implementation(const FVector& MoveDir)
{
	MoveDirection = MoveDir;
}

bool UMyCharacterMovementComponent::ServerSetMoveDirection_Validate(const FVector& MoveDir)
{
	return true;
}

#pragma endregion 

#pragma endregion

#pragma region class FSavedMove_MyMovement

FSavedMove_MyMovement::FSavedMove_MyMovement()
{
	SavedWantsToSprint = false;
	SavedWallRunKeysDown = false;
	bSavedWantsToDodge = false;
	SavedMoveDirection = FVector::ZeroVector;
}

#pragma region Saved Move Overrides

void FSavedMove_MyMovement::Clear()
{
	Super::Clear();

	// Clear all values
	SavedWantsToSprint = false;
	SavedWallRunKeysDown = false;
	bSavedWantsToDodge = false;
	SavedMoveDirection = FVector::ZeroVector;
}

uint8 FSavedMove_MyMovement::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	/* There are 4 custom move flags for us to use. Below is what each is currently being used for:
	FLAG_Custom_0		= 0x10, // Sprinting
	FLAG_Custom_1		= 0x20, // WallRunning
	FLAG_Custom_2		= 0x40, // Blinking
	FLAG_Custom_3		= 0x80, // Unused
	*/
	
	// Write to the compressed flags 
	if (SavedWantsToSprint)
		Result |= FLAG_Sprint;
	if (SavedWallRunKeysDown)
		Result |= FLAG_WallRun;
	if (bSavedWantsToDodge)
		Result |= FLAG_3;

	return Result;
}

bool FSavedMove_MyMovement::CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const
{
	const FSavedMove_MyMovement* NewMove = static_cast<const FSavedMove_MyMovement*>(NewMovePtr.Get());

	// As an optimization, check if the engine can combine saved moves.
	if (SavedWantsToSprint != NewMove->SavedWantsToSprint ||
		SavedWallRunKeysDown != NewMove->SavedWallRunKeysDown ||
		bSavedWantsToDodge != NewMove->bSavedWantsToDodge ||
		SavedMoveDirection != NewMove->SavedMoveDirection)
	{
		return false;
	}

	return Super::CanCombineWith(NewMovePtr, Character, MaxDelta);
}

void FSavedMove_MyMovement::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	
	if (const UMyCharacterMovementComponent* CharMov = static_cast<UMyCharacterMovementComponent*>(Character->GetCharacterMovement()))
	{
		// Copy values into the saved move
		SavedWantsToSprint = CharMov->WantsToSprint;
		SavedWallRunKeysDown = CharMov->WallRunKeysDown;
		bSavedWantsToDodge = CharMov->bWantsToDodge;
		SavedMoveDirection = CharMov->MoveDirection;
	}
}

void FSavedMove_MyMovement::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	
	if (UMyCharacterMovementComponent* CharMov = Cast<UMyCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		// Copy values out of the saved move
		CharMov->WantsToSprint = SavedWantsToSprint;
		CharMov->WallRunKeysDown = SavedWallRunKeysDown;
		CharMov->bWantsToDodge = bSavedWantsToDodge;
		CharMov->MoveDirection = SavedMoveDirection;
	}
}

#pragma endregion

#pragma endregion

#pragma region class FNetworkPredictionData_Client_My

#pragma region Network Prediction Data

FNetworkPredictionData_Client_My::FNetworkPredictionData_Client_My(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{

}

FSavedMovePtr FNetworkPredictionData_Client_My::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_MyMovement());
}

#pragma endregion

#pragma endregion 
