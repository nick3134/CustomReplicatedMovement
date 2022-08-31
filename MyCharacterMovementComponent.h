#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.generated.h"

class AGrappleHook;
class AGrappleHookCable;
class AImpulseDefaultCharacter;
enum EGrappleHookState;
enum EWallRunSide;
enum EImpulseMovementMode;

UCLASS(BlueprintType)
class IMPULSE_API UMyCharacterMovementComponent : public UCharacterMovementComponent
{

#pragma region Setup
	
	GENERATED_BODY()

	friend class FSavedMove_MyMovement;

	/** Constructor */
	UMyCharacterMovementComponent();

#pragma endregion

#pragma region Defaults

private:
	
	/** The maximum ground speed while running. Also determines maximum lateral speed when falling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: Grounded", Meta = (AllowPrivateAccess = "true"))
	float DefaultMaxRunSpeed = 550.0f;

	/** The default maximum acceleration while running. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: Grounded", Meta = (AllowPrivateAccess = "true"))
	float DefaultMaxRunAcceleration = 10000.0f;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: Grounded", Meta = (AllowPrivateAccess = "true"))
	float DefaultGroundFriction = 5.f;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: Grounded", Meta = (AllowPrivateAccess = "true"))
	float DefaultBrakingDecelerationWalking = 10000.f;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: InAir", Meta = (AllowPrivateAccess = "true"))
	float DefaultGravityScale = 1.75f;

	/** Initial velocity (instantaneous vertical acceleration) when jumping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: InAir", Meta = (AllowPrivateAccess = "true"))
	float DefaultJumpZVelocity = 550.f;

	/**
	 * When falling, amount of lateral movement control available to the character.
	 * 0 = no control, 1 = full control at max speed of MaxWalkSpeed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: InAir", Meta = (AllowPrivateAccess = "true"))
	float DefaultAirControl = 0.2f;

	/**
	 * When falling, multiplier applied to AirControl when lateral velocity is less than AirControlBoostVelocityThreshold.
	 * Setting this to zero will disable air control boosting. Final result is clamped at 1.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: InAir", Meta = (AllowPrivateAccess = "true"))
	float DefaultAirControlBoostMultiplier = 0.f;

	/**
	 * When falling, if lateral velocity magnitude is less than this value, AirControl is multiplied by AirControlBoostMultiplier.
	 * Setting this to zero will disable air control boosting.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: InAir", Meta = (AllowPrivateAccess = "true"))
	float DefaultAirControlBoostVelocityThreshold = 0.f;

	/**
	 * Friction to apply to lateral air movement when falling.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero).
	 * @see BrakingFriction, bUseSeparateBrakingFriction
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Defaults: InAir", Meta = (AllowPrivateAccess = "true"))
	float DefaultFallingLateralFriction = 0.1f;

	/** The current maximum ground speed while running. Also determines maximum lateral speed when falling. */
	float CurrentMaxRunSpeed;

	/** The current maximum acceleration while running. */
	float CurrentMaxRunAcceleration;

public:

	/** Called on tick to determine if the camera should be tilted during movements. */
	void CameraTick() const;

#pragma endregion

#pragma region Jumping

private:

	/** True if the jump key is currently being pressed. */
	bool WantsToJump = false;

public:

	/**
	 *	Sets the value of WantsToJump when the jump key is pressed.
	 *	@param bIsJumping the new value of WantsToJump.
	 */
	void SetJumping(bool bJumped);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetJumping(bool bJumped);

	UFUNCTION(NetMulticast, Reliable, WithValidation)
	void MultiSetJumping(bool bJumped);

	bool Jumped;
	
	void JumpedFalse();

#pragma endregion

#pragma region Sprinting

private:

	/** The default maximum ground speed when sprinting. Also determines the maximum lateral speed while falling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "My Character Movement|Sprinting", Meta = (AllowPrivateAccess = "true"))
	float DefaultMaxSprintSpeed = 825.0f;

	/** The default maximum acceleration when sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "My Character Movement|Sprinting", Meta = (AllowPrivateAccess = "true"))
	float DefaultMaxSprintAcceleration = 10000.0f;

	/** The current maximum ground speed while sprinting. Also determines the maximum lateral speed when falling. */
	float CurrentMaxSprintSpeed;

	/** The current maximum acceleration while sprinting. */
	float CurrentMaxSprintAcceleration;
	
public:

	/**
	 *	Sets whether the player is sprinting or not.
	 *	@param Sprinting the new value of the player sprinting state
	 */
	void SetSprinting(const bool Sprinting);

	/**
	 *	Determines if the player is moving forward to be able to sprint.
	 *	@return true if moving forward.
	 */
	bool IsMovingForward() const;
	
#pragma endregion
	
#pragma region Crouching

private:

	/** The default maximum ground speed while walking and when crouched. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Crouching", Meta = (AllowPrivateAccess = "true"))
	float DefaultMaxWalkSpeedCrouched = 300.f;

	/** True while the crouch key is held down, false otherwise. */
	bool WantsToCrouch = false;
	
public:

	/** Called when the player presses the crouch key to set WantsToCrouch to true. */
	UFUNCTION(Client, Unreliable)
	void BeginCrouch();

	/** Called when the player releases the crouch key to set WantsToCrouch to false. */
	UFUNCTION(Client, Unreliable)
	void EndCrouch();
	
#pragma endregion

#pragma region Sliding

private:

	/** The maximum ground speed when sliding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "My Character Movement|Sliding", Meta = (AllowPrivateAccess = "true"))
	float MaxSlideSpeed = 1300.f;
	
	/** True if the required keys are being pressed for sliding. */
	bool SlideKeysDown;

	/** True while sliding to allow the force to continue to be applied. False when the slide is ended. */
	bool CanSlide = true;

	/** Handles the looping call of DoSlide() to add a force during the duration fo the slide. */
	FTimerHandle SlideTimerHandle;

	
	
public:

	/**
	 *	True if currently sliding. False if not sliding.
	 *	Replicated for third person animations.
	 */
	UPROPERTY(Replicated)
	bool IsSliding = false;
	
	/**
	 *	Sets the physics values to be able to slide and calls the server to do the same.
	 *	@see ServerBeginSlide().
	 */
	void BeginSlide();

	/**
	 *	Resets the physics values to be able to slide and calls the server to do the same.
	 *	@see ServerEndSlide().
	 */
	void EndSlide();

	/** Lets the player slide after SlideCooldown only if not holding the slide keys still*/
	void AllowSlide();

	/** Checks if should be able to slide based on the keys that are pressed. */
	void CheckSlide();

	/**
	 *	Determines if the required keys to slide are being pressed.
	 *	@return true if the required slide keys are being pressed.
	 */
	bool AreRequiredSlideKeysDown() const;

	/** Applies the force to player to be able to slide. */
	void DoSlide();

	/**
	 *	Calculates the force that should be applied to the player based on the slope of the floor.
	 *	@param FloorNormal the normal vector of the floor the player is standing on.
	 *	@return a vector that represents the force that should be applied to the player while sliding.
	 */
	static FVector CalcFloorInfluence(FVector FloorNormal);

	/** Sets the rotation of the camera while sliding. */
	void SlideCameraRotate() const;

	/**
	 *	Sets the value of IsSliding to other clients.
	 *	@param Sliding the new value of IsSliding.
	 */
	UFUNCTION(NetMulticast, Reliable, WithValidation)
	void MultiSetIsSliding(const bool Sliding);

	/** Sets the physics values to be able to slide on the server. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerBeginSlide();

	/** Resets the physics values to be able to slide on the server. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerEndSlide();

#pragma endregion
	
#pragma region Wall Running

private:

	/** The amount of vertical room between the two line traces when checking if the character is still on the wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float LineTraceVerticalTolerance = 10.0f;

	/** The maximum speed while wall running. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float WallRunSpeed = 1200.0f;

	/** The force applied horizontally when jumping off of a wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float HorizontalWallJumpOffForce = 400.f;

	/** The force applied vertically when jumping off of a wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Wall Running", Meta = (AllowPrivateAccess = "true"))
	float VerticalWallJumpOffForce = 600.f;

	/** Called when the owning actor hits an actor to determine if wall run should begin. */
	UFUNCTION()
	void OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/** The direction the character is currently wall running in. */
	FVector WallRunDirection;

	/** The normal vector of the wall the character is running on. */
	FVector WallRunNormal;
	
public:

	/**
	 *	Enum that determines which side of the player is on the wall.
	 *	kLeft: Running along the left side of a wall.
	 *	kRight: Running along the right side of a wall.
	 *	kStraight: Not currently wall running.
	 */
	EWallRunSide WallRunSide;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetWallRunSide(EWallRunSide WRSide);

	UFUNCTION(NetMulticast, Reliable, WithValidation)
	void MultiSetWallRunSide(EWallRunSide WRSide);
	
	/** Requests that the character begins wall running. Will return false if the required keys are not being pressed. */
	bool BeginWallRun();

	/** Called while wall running to rotate the camera based on the current wall run direction. */
	void WallRunCameraRotate(float Roll) const;

	/** Called when jumping while wall running to launch the player off of the wall. */
	void WallRunJump();
	
	/** Called to end the wall run. */
	void EndWallRun();
	
	/**
	 *	Determines if the player is able to wall run based on the keys that are being pressed.
	 *	@return true if the keys required to wall run are being pressed.
	 */
	bool AreRequiredWallRunKeysDown() const;
	
	/**
	 *	Determines if the player if the wall next to the player is a valid wall.
	 *	@param VerticalTolerance allowable height of a valid wall that can be wall ran on.
	 *	@return true if the player is next to a wall that can be wall ran.
	 */
	bool IsNextToWall(float VerticalTolerance = 0.0f) const;
	
	/**
	 *	Finds the wall run direction and side based on the specified surface normal.
	 *	@param SurfaceNormal normal vector of the wall attempting to run on.
	 *	@param Direction the direction the character is currently wall running.
	 *  @param Side the side of the player that is on the wall. 
	 */
	void FindWallRunDirectionAndSide(const FVector& SurfaceNormal, FVector& Direction, EWallRunSide& Side);

	/**
	 *	Helper function that determines if a wall can be wall ran on based on the surface normal.
	 *	@param SurfaceNormal normal vector of the wall attempting to run on.
	 *	@return true if the specified surface normal can be wall ran on.
	 */
	bool CanSurfaceBeWallRan(const FVector& SurfaceNormal) const;

	/**
	 *	Function to set the physics of the player when in the wall custom movement mode.
	 *	@param DeltaTime frame time to advance, in seconds
	 *	@param Iterations physics iteration count
	 *	@note This function (and all other Phys* functions) will be called on characters with ROLE_Authority and ROLE_AutonomousProxy
	 *  but not ROLE_SimulatedProxy. All movement should be performed in this function so that is runs locally and on the server. UE4 will handle
	 *  replicating the final position, velocity, etc.. to the other simulated proxies.
	 */
	void PhysWallRunning(float DeltaTime, int32 Iterations);
	
	/**
	 *	Determines if the inputted movement mode is the current custom movement mode.
	 *	@param CustomMove movement mode that is getting checked as current custom movement mode.
	 *	@return true if the movement mode is custom and matches the provided custom movement mode
	 */
	bool IsCustomMovementMode(uint8 CustomMove) const;

#pragma endregion

#pragma region Blink

private:

	/** Amount of force applied in the direction of the blink. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Blink", Meta = (AllowPrivateAccess = "true"))
	float BlinkStrength = 10000.f;

	/** The duration of the blink. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Blink", Meta = (AllowPrivateAccess = "true"))
	float BlinkDuration = 0.15f;

	/** The time it takes to be able to reuse the blink. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Blink", Meta = (AllowPrivateAccess = "true"))
	float BlinkCooldown = 1.f;

	/** True if not currently blinking and not on cooldown. */
	bool CanDodge = true;
	
public:

	/**
	 *	Sets bWantsToBlink to true to allow the force to be applied during the blink.
	 *	@see OnMovementUpdate()
	 */
	void DoDodge();

	/** Stops blink movement and resets the changed variables during the dodge. */
	void EndDodge();

	/** Sets CanBlink to true once the cooldown for the blink is over */
	void AllowDodge();

#pragma endregion

#pragma region Stimmy

private:

	/** Multiplier applied to all movements while the stimmy is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Stimmy", Meta = (AllowPrivateAccess = "true"))
	float StimmySpeedMultiplier = 1.5f;

	/** The duration of the stimmy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Stimmy", Meta = (AllowPrivateAccess = "true"))
	float StimmyDuration = 10.f;

	/** The time it takes to be able to reuse the stimmy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|Stimmy", Meta = (AllowPrivateAccess = "true"))
	float StimmyCooldown = 8.f;

	/** True if the stimmy is currently active. */
	bool IsStimmy = false;

	/** True if the stimmy is not currently in use and not cooldown. */
	bool CanStimmy = true;
	
public:

	/** Applies the StimmySpeedMultiplier to all the movements. */
	void StartStimmy();

	/** Resets all the movement speeds to their default values. */
	void EndStimmy();
	
	/** Applies the StimmySpeedMultiplier to all the movements on the server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetStimmySpeed();

	/** Resets all the movement speeds to their default values on the server. */
	UFUNCTION(Server, Unreliable)
	void ServerEndStimmySpeed();

	/**
	 *	Sets the value of IsStimmy on the server to be used in other server functions
	 *	@param bStimmy new value of IsStimmy
	 *	@see PhysWallRunning()
	 */
	UFUNCTION(Server, Unreliable)
	void ServerSetStimmy(bool bStimmy);

	/** Called when StimmyCooldown is finished to allow the stimmy again. */
	void ResetStimmy();

#pragma endregion

#pragma region Slide Jump

private:

	/** The value of the gravity scale only while slide jumping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|SlideJump", Meta = (AllowPrivateAccess = "true"))
	float SlideJumpGravityScale = 2.5f;

	/** The horizontal force applied when slide jumping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|SlideJump", Meta = (AllowPrivateAccess = "true"))
	float HorizontalSlideJumpForce = 1500.f;

	/** The vertical force applied when slide jumping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|SlideJump", Meta = (AllowPrivateAccess = "true"))
	float VerticalSlideJumpForce = 2000.f;

	/** The time it takes to be able to slide jump again after starting the ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "My Character Movement|SlideJump", Meta = (AllowPrivateAccess = "true"))
	float SlideJumpCooldown = 6.f;

	/** True once the SlideJumpCooldown is finished to be able to slide jump again. */
	float CanSlideJump = true;
	
public:

	/**
	 *	Calls the start of the slide jump on the server to launch the character.
	 *	@see ServerSlideJump().
	 */
	UFUNCTION()
	void SlideJump();

	/** Sets CanSlideJump to true once the SlideJumpCooldown is finished. */
	void AllowSlideJump();
	
	/** Launches the player for the slide jump. */
	UFUNCTION(Server, Unreliable)
	void ServerSlideJump();

	

#pragma endregion

#pragma region Grapple Hook

private:

	/** The maximum distance the grapple hook travels before breaking. */
	UPROPERTY(EditDefaultsOnly, Category = "My Character Movement|Grapple Hook", Meta = (AllowPrivateAccess = "true"))
	float GrappleDistance = 4000.f;

	/** The force applied to the player by the grapple. */
	UPROPERTY(EditDefaultsOnly, Category = "My Character Movement|Grapple Hook", Meta = (AllowPrivateAccess = "true"))
	float GrapplePullForce = 200000.f;

	/** The instantaneous velocity of the player when the grapple attaches. */
	UPROPERTY(EditDefaultsOnly, Category = "My Character Movement|Grapple Hook", Meta = (AllowPrivateAccess = "true"))
	float InstantaneousVelocityFromGrapple = 1200.f;

	/** The time it takes to be able to use the grapple again after it is destroyed. */
	UPROPERTY(EditDefaultsOnly, Category = "My Character Movement|Grapple Hook", Meta = (AllowPrivateAccess = "true"))
	float GrappleCooldown = 8.f;
	
	/** The class used as the hook for the grapple hook ability. */
	UPROPERTY(EditDefaultsOnly, Category = "My Character Movement|Grapple Hook", Meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGrappleHook> GrappleHookClass;

	/** The class used as the cable for the grapple hook ability. */
	UPROPERTY(EditDefaultsOnly, Category = "My Character Movement|Grapple Hook", Meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGrappleHookCable> GrappleCableClass;

	/** A pointer to the AGrappleHook class to use as the current grapple hook */
	UPROPERTY()
	AGrappleHook* GrappleHook;

	/** A pointer to the AGrappleHookCable class to use as the current grapple hook cable. */
	UPROPERTY()
	AGrappleHookCable* GrappleCable;

	/**
	 *	Enum to determine the current state of the grapple hook
	 *	GRAPPLE_Ready - the grapple is ready to be fired
	 *	GRAPPLE_Firing - the grapple is currently being fired
	 *	GRAPPLE_Attached - the grapple is currently pulling the player
	 */
	TEnumAsByte<EGrappleHookState> CurrentGrappleHookState;

	/** The start location of the cable for the grapple hook. */
	FVector CableStartLocation;

	/** The x,y direction the grapple hook is initially fired from. */
	FVector InitialHookDirection2D;

	/** True when the grapple cooldown has finished. */
	float CanGrapple = true;
	
public:

	/**
	 *	Calls the grapple hook to be fired on the server for spawning the actors.
	 *	@param TargetLocation the location the player is looking.
	 *	@param LocalOffset the offset of the spawn location of the grapple hook.
	 *	@see ServerFireGrapple()
	 */
	void FireGrapple(FVector TargetLocation, FVector LocalOffset);

	/** Spawns the grapple hook, grapple cable, and attaches the cable to the player. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFireGrapple(const FVector FiringDirection, const FVector CableStart);

	/** Destroys the grapple hook on the server. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerCancelGrapple();

	/**
	 *	Called when the grapple hook hits something solid.
	 *	This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 *	For events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *	@param HitComponent the hit component from the grapple hook
	 *	@param OtherActor the other actor
	 *	@param OtherComp the other component
	 *	@param NormalImpulse the normal impulse
	 *	@param Hit the hit result
	 *	@note For collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled for this component.
	 *	@note When receiving a hit from another object's movement, the directions of 'Hit.Normal' and 'Hit.ImpactNormal'
	 *	will be adjusted to indicate force from the other object against this object.
	 *	@note NormalImpulse will be filled in for physics-simulating bodies, but will be zero for swept-component blocking collisions.
	 */
	void OnGrappleHookHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/**
	 *	Event triggered when the AGrappleHook has been destroyed.
	 *	Destroys the AGrappleHookCable and resets the changed physics values.
	 *	@param DestroyedActor the actor that got destroyed.
	 */
	void OnGrappleHookDestroyed(AActor* DestroyedActor);

	/**
	 *	Sets the grapple hook state and calls to be set on the server.
	 *	@param NewGrappleHookState the new grapple hook state being set.
	 *	@see ServerSetGrappleHookState()
	 */
	void SetGrappleHookState(EGrappleHookState NewGrappleHookState);

	/**
	 *	Sets the grapple hook state on the server to be used in other server functions.
	 *	@param NewGrappleHookState the new grapple hook state being set.
	 *	@note this function should not be called directly, set the grapple hook state for the client first.
	 *	@see SetGrappleHookState()
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetGrappleHookState(EGrappleHookState NewGrappleHookState);

	/**
	 *	Determines if the inputted grapple hook state is the same as the current grapple hook state.
	 *	@param GrappleHookState grapple hook state that is being checked as the current grapple hook state.
	 *	@return true if the current grapple hook state is equal to the input.
	 */
	bool IsGrappleHookState(EGrappleHookState GrappleHookState);

	/**
	 *	Determines if the grapple is currently in use.
	 *	@returns true if grapple is attached or grapple is being fired (GRAPPLE_Attached or GRAPPLE_Firing).
	 */
	bool IsGrappleInUse();

	/** Sets CanGrapple to true after the GrappleCooldown is complete. */
	void AllowGrapple();

	/** Called on tick to move and rotate the cable based on the player and hook if the grapple hook is in use. */
	void GrappleCableTick();

#pragma endregion

#pragma region Overrides

protected:

	/** Native event for when play begins for this actor. */
	virtual void BeginPlay() override;

	/**
	 *	Event called when component is destroyed.
	 *	@param DestroyingHierarchy destroying hierarchy.
	 */
	virtual void OnComponentDestroyed(bool DestroyingHierarchy) override;
	
public:

	/**
	 *	Enum used to set the movement mode for the animations.
	 *	CMOVE_Grounded - player is currently grounded.
	 *	CMOVE_InAir - player is currently in the air.
	 *	CMOVE_WallRunning - player is currently wall running.
	 */
	TEnumAsByte<EImpulseMovementMode> ImpulseMovementMode;

	/**
	 *	Sets the impulse movement mode to the new impulse movement mode.
	 *	@param NewMovementMode the new impulse movement mode.
	 */
	void SetImpulseMovementMode(EImpulseMovementMode NewMovementMode);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetImpulseMovementMode(EImpulseMovementMode NewMoveMode);

	UFUNCTION(NetMulticast, Reliable, WithValidation)
	void MultiSetImpulseMovementMode(EImpulseMovementMode NewMoveMode);

	/**
	 *	Event called every frame.
	 *	@param DeltaTime frame time to advance, in seconds
	 *	@param TickType type of tick to perform on the level
	 *	@param ThisTickFunction this tick function
	 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Unpack compressed flags from a saved move and set state accordingly. See FSavedMove_Character. */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	/** Called after MovementMode has changed. Base implementation does special handling for starting certain modes, then notifies the CharacterOwner. */
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/**
	 * Event triggered at the end of a movement update. If scoped movement updates are enabled (bEnableScopedMovementUpdates), this is within such a scope.
	 * If that is not desired, bind to the CharacterOwner's OnMovementUpdated event instead, as that is triggered after the scoped movement update.
	 */
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	/** @note Movement update functions should only be called through StartNewPhysics()*/
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	/** Returns maximum speed for the current state. */
	virtual float GetMaxSpeed() const override;

	/** Returns maximum acceleration for the current state. */
	virtual float GetMaxAcceleration() const override;

	/** Handle landing against Hit surface over remaingTime and iterations, calling SetPostLandedPhysics() and starting the new movement mode. */
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Returns true if current movement state allows an attempt at jumping. Used by Character::CanJump().
	 */
	virtual bool CanAttemptJump() const override;

	
#pragma endregion

#pragma region Compressed Flags
	
public:

	/**
	 *	Sets the direction of the player on the server.
	 *	@param MoveDir the direction of the player.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetMoveDirection(const FVector& MoveDir);
	
private:

	/** Compressed flag for requesting to sprint. */
	uint8 WantsToSprint : 1;
	
	/** Compressed flag for requesting to wall run .*/
	uint8 WallRunKeysDown : 1;
	
	/** Compressed flag for requesting to blink. */
	uint8 bWantsToDodge : 1;
	
	/** RPC setting movement direction of player. */
	FVector MoveDirection;

	//bool WantsToJump;
	
#pragma endregion
	
};

class FSavedMove_MyMovement : public FSavedMove_Character
{
#pragma region Setup
	
public:

	typedef FSavedMove_Character Super;

	FSavedMove_MyMovement();

#pragma endregion

#pragma region Saved Move Overrides

public:
	
	/** Resets all saved variables. */
	virtual void Clear() override;
	
	/** Store input commands in the compressed flags. */
	virtual uint8 GetCompressedFlags() const override;
	
	/**
	 *	This is used to check whether or not two moves can be combined into one.
	 *	Basically you just check to make sure that the saved variables are the same.
	 */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMovePtr, ACharacter* Character, float MaxDelta) const override;

	/** Sets up the move before sending it to the server. */
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;

	/** Sets variables on character movement component before making a predictive correction. */
	virtual void PrepMoveFor(class ACharacter* Character) override;
	
	enum Flags
	{
		FLAG_Sprint = 0x04,
		FLAG_WallRun = 0x08,
		FLAG_Slide = 0x10,
		FLAG_3 = 0x20,
		FLAG_4 = 0x40,
		FLAG_5 = 0x80,
	};

#pragma endregion 

#pragma region Saved Compressed Flags
	
private:

	/** Saved compressed flag for requesting to sprint. */
	uint8 SavedWantsToSprint : 1;
	
	/** Saved compressed flag for requesting to wall run .*/
	uint8 SavedWallRunKeysDown : 1;
	
	/** Saved compressed flag for requesting to blink. */
	uint8 bSavedWantsToDodge : 1;
	
	/** Saved RPC setting movement direction of player. */
	FVector SavedMoveDirection;

	//bool SavedWantsToJump;
	
#pragma endregion
	
};

class FNetworkPredictionData_Client_My : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	/** Constructor */
	FNetworkPredictionData_Client_My(const UCharacterMovementComponent& ClientMovement);

	/** Brief allocates a new copy of our custom saved move. */
	virtual FSavedMovePtr AllocateNewMove() override;
};