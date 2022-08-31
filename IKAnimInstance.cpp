
#include "Character/Anims/AnimInstances/IKAnimInstance.h"

#include "Character/ImpulseDefaultCharacter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Character/Weapons/WeaponBase.h"
#include "Enums/EImpulseMovementMode.h"

UIKAnimInstance::UIKAnimInstance()
{
	RHandRotation = FRotator::ZeroRotator;

	FP_LeftHandTransform.SetTranslation(FVector(-33.698f, 13.899f, -5.494f));
	
	Speed = 0.f;
	Direction = 0.f;
	Pitch = 0.f;
	IsInAir = false;
	IsCrouching = false;
	IsSliding = false;
}

void UIKAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();

	if (TryGetPawnOwner())
		Character = Cast<AImpulseDefaultCharacter>(TryGetPawnOwner());
}

void UIKAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	if (Character)
	{
		UpdateCharacterInfo(DeltaSeconds);
		UpdateMovementStates(DeltaSeconds);
		UpdateMovementInfo(DeltaSeconds);
		UpdateWeaponInfo(DeltaSeconds);
		UpdateRotationInfo(DeltaSeconds);
		UpdateRootYawOffset(DeltaSeconds);
		UpdateLayerValues(DeltaSeconds);
		UpdateFootIK(DeltaSeconds);
	}
}

#pragma region Update Functions

void UIKAnimInstance::UpdateCharacterInfo(const float DeltaSeconds)
{
	if (Character)
	{
		Speed = Character->GetMovementComponent()->Velocity.Size();
		Velocity = Character->GetMovementComponent()->Velocity;
		Direction = CalculateDirection(Character->GetMovementComponent()->Velocity, Character->GetActorRotation());
		Pitch = Character->GetCharacterPitch();
		
		if ((Character->GetMyMovementComponent()->GetCurrentAcceleration().Size() / Character->GetMyMovementComponent()->GetMaxAcceleration()) > 0.f)
			HasMovementInput = true;
		else
			HasMovementInput = false;
		
		if (((Speed > 1.f) && HasMovementInput) || Speed > 150.f)
			ShouldMove = true;
		else
			ShouldMove = false;
	}
}

void UIKAnimInstance::UpdateMovementStates(const float DeltaSeconds)
{
	if (Character)
	{
		ImpulseMovementMode = Character->GetMyMovementComponent()->ImpulseMovementMode;
		IsInAir = Character->GetMovementComponent()->IsFalling();
		Jumped = Character->GetMyMovementComponent()->Jumped;
		IsCrouching = Character->GetMovementComponent()->IsCrouching();
		IsSliding = Character->GetMyMovementComponent()->IsSliding;
	}
}

void UIKAnimInstance::UpdateMovementInfo(const float DeltaSeconds)
{
	if (Character)
	{
		WallRunSide = Character->GetMyMovementComponent()->WallRunSide;
	}
}

void UIKAnimInstance::UpdateWeaponInfo(const float DeltaSeconds)
{
	if (Character)
	{
		if (Character->GetCurrentWeapon())
		{
			CurrentWeaponID = Character->GetCurrentWeapon()->GetWeaponID();

			if (Character->IsLocallyControlled()) //dont need these calculations for third person
			{
				const float HorizontalMovement = UKismetMathLibrary::Dot_VectorVector(Character->GetMovementComponent()->Velocity.GetSafeNormal(), Character->GetActorRightVector());
				const float VerticalInput = Character->GetInputAxisValue(FName("LookUp"));
				const float HorizontalInput = Character->GetInputAxisValue(FName("LookRight"));

				const float TargetRotation = UKismetMathLibrary::MapRangeClamped(HorizontalMovement, -1.f, 1.f, -6.f, 6.f);
				const float TargetPitch = UKismetMathLibrary::MapRangeClamped(VerticalInput, 1.f, -1.f, -3.f, 3.f);
				const float TargetYaw = UKismetMathLibrary::MapRangeClamped(HorizontalInput, -1.f, 1.f, -3.f, 3.f);

				RHandRotation.Roll = UKismetMathLibrary::FInterpTo(RHandRotation.Roll, TargetRotation, DeltaSeconds, 5.f);
				RHandRotation.Pitch = UKismetMathLibrary::FInterpTo(RHandRotation.Pitch, TargetPitch, DeltaSeconds, 5.f);
				RHandRotation.Yaw = UKismetMathLibrary::FInterpTo(RHandRotation.Yaw, TargetYaw, DeltaSeconds, 5.f);
			}
			
			SetLeftHandIK();
		}
	}
}

void UIKAnimInstance::UpdateRotationInfo(const float DeltaSeconds)
{
	if (Character)
	{
		if (Speed > 50)
		{
			FirstInstance = true;
			Rotate_L = false;
			Rotate_R = false;
			return;
		}

		if (FirstInstance)
		{
			FirstInstance = false;
			InitialAimAngle = UKismetMathLibrary::NormalizedDeltaRotator(Character->Control_Rotation, Character->GetActorRotation()).Yaw;
		}
		
		PrevAimAngle = AimAngle;
		AimAngle = UKismetMathLibrary::NormalizedDeltaRotator(Character->Control_Rotation, Character->GetActorRotation()).Yaw;
		
		if (UKismetMathLibrary::Abs(AimAngle - InitialAimAngle) > RotateDegreeThreshold)
		{
			Rotate_L = InitialAimAngle - AimAngle < -RotateDegreeThreshold;
			Rotate_R = InitialAimAngle - AimAngle > RotateDegreeThreshold;
			InitialAimAngle = AimAngle;
		}
		else
		{
			Rotate_L = false;
			Rotate_R = false;
		}

		if (Rotate_L || Rotate_R)
		{
			RotateRate = UKismetMathLibrary::MapRangeClamped(UKismetMathLibrary::Abs(InitialAimAngle - AimAngle), 0.f, 20.f, MinPlayRate, MaxPlayRate);
		}
		
		SpineRotation = FRotator(0.f, (AimAngle - InitialAimAngle) / 4.f, 0.f);
	}
	if (Character)
	{
		RootRotation = GetOwningActor()->GetRootComponent()->GetOwner()->GetActorRotation();
		YawDeltaSinceLastUpdate = RootRotation.Yaw - WorldRotation.Yaw;
		WorldRotation = RootRotation;

		if (IsFirstUpdate)
			YawDeltaSinceLastUpdate = 0.f;
	}
	
}

void UIKAnimInstance::UpdateRootYawOffset(const float DeltaSeconds)
{
	if (Character)
	{
		if (Speed > 50.f)
		{
			Rotate_L = false;
			Rotate_R = false;
		
			FFloatSpringState RootYawOffsetSpringState;
			SetRootOffset(UKismetMathLibrary::FloatSpringInterp(RootYawOffset, 0.f, RootYawOffsetSpringState, 80.f, 1.f, DeltaSeconds, 1.f, 0.5f));
			return;
		}
		SetRootOffset(RootYawOffset - YawDeltaSinceLastUpdate);
	}
}

void UIKAnimInstance::UpdateLayerValues(const float DeltaSeconds)
{
	if (Character)
	{
		BasePoseN = GetAnimCurve_Compact(FName("BasePose_N"));
		BasePoseCLF = GetAnimCurve_Compact(FName("BasePose_CLF"));

		SpineAdd = GetAnimCurve_Compact(FName("Layering_Spine_Add"));
		
		HeadAdd = GetAnimCurve_Compact(FName("Layering_Head_Add"));
		
		ArmAdd_L = GetAnimCurve_Compact(FName("Layering_Arm_L_Add"));
		ArmAdd_R = GetAnimCurve_Compact(FName("Layering_Arm_R_Add"));

		ArmLS_L = GetAnimCurve_Compact(FName("Layering_Arm_L_LS"));
		ArmLS_R = GetAnimCurve_Compact(FName("Layering_Arm_R_LS"));

		ArmMS_L = 1 - UKismetMathLibrary::FFloor(ArmLS_L);
		ArmMS_R = 1 - UKismetMathLibrary::FFloor(ArmLS_R);

		Hand_L = GetAnimCurve_Compact(FName("Layering_Hand_L"));
		Hand_R = GetAnimCurve_Compact(FName("Layering_Hand_R"));

		EnableHandIK_L = UKismetMathLibrary::Lerp(0.f, GetAnimCurve_Compact(FName("Enable_HandIK_L")), GetAnimCurve_Compact(FName("Layering_Arm_L")));
		EnableHandIK_R = UKismetMathLibrary::Lerp(0.f, GetAnimCurve_Compact(FName("Enable_HandIK_R")), GetAnimCurve_Compact(FName("Layering_Arm_R")));
	}
}

void UIKAnimInstance::UpdateFootIK(const float DeltaSeconds)
{
	if (Character)
	{
		SetFootLocking(FName("Enable_FootIK_L"), FName("FootLock_L"), FName("ik_foot_l"), LFootLockAlpha, LFootLockLocation, LFootLockRotation, DeltaSeconds);
		SetFootLocking(FName("Enable_FootIK_R"), FName("FootLock_R"), FName("ik_foot_r"), RFootLockAlpha, RFootLockLocation, RFootLockRotation, DeltaSeconds);

		if (Character->GetMyMovementComponent()->ImpulseMovementMode != CMOVE_InAir)
		{
			SetFootOffsets(FName("Enable_FootIK_L"), FName("ik_foot_l"), FName("root"), LFootOffsetTarget, LFootOffsetLocation, LFootOffsetRotation, DeltaSeconds);
			SetFootOffsets(FName("Enable_FootIK_R"), FName("ik_foot_r"), FName("root"), RFootOffsetTarget, RFootOffsetLocation, RFootOffsetRotation, DeltaSeconds);
			SetPelvisIKOffset(LFootOffsetTarget, RFootOffsetTarget, DeltaSeconds);
		}
		else
		{
			SetPelvisIKOffset(FVector::ZeroVector, FVector::ZeroVector, DeltaSeconds);
			ResetIKOffsets(DeltaSeconds);
		}
	}
}

#pragma endregion 

#pragma region Helper Functions

float UIKAnimInstance::GetAnimCurve_Compact(const FName CurveName) const
{
	if (Character)
	{
		float OutValue;
		if (GetCurveValue(FName(CurveName), OutValue))
		{
			return OutValue;
		}
	}
	return 0.f;
}

void UIKAnimInstance::SetLeftHandIK()
{
	if (Character)
	{
		if (Character->GetCurrentWeapon())
		{
			if (Character->GetCurrentWeapon()->GetWeaponMesh())
			{
				const FTransform FPGunSocketTransform = Character->GetCurrentWeapon()->GetWeaponMesh()->GetSocketTransform(FName("S_FP_LeftHand"));
				const FTransform FPMeshSocketTransform = Character->GetMesh1P()->GetSocketTransform(FName("hand_r"));
			
				FP_LeftHandTransform = UKismetMathLibrary::MakeRelativeTransform(FPGunSocketTransform, FPMeshSocketTransform);
			
				const FTransform TPPGunSocketTransform = Character->GetCurrentWeapon()->GetWeaponMesh()->GetSocketTransform(FName("S_TPP_LeftHand"));
				const FTransform TPMeshSocketTransform = Character->GetMesh()->GetSocketTransform(FName("hand_r"));
			
				TPP_LeftHandTransform = UKismetMathLibrary::MakeRelativeTransform(TPPGunSocketTransform, TPMeshSocketTransform);
			}
		}
	}
}

float UIKAnimInstance::CalculateDirection(const FVector& Velocity, const FRotator& BaseRotation)
{
	if (!Velocity.IsNearlyZero())
	{
		FMatrix RotMatrix = FRotationMatrix(BaseRotation);
		FVector ForwardVector = RotMatrix.GetScaledAxis(EAxis::X);
		FVector RightVector = RotMatrix.GetScaledAxis(EAxis::Y);
		FVector NormalizedVel = Velocity.GetSafeNormal2D();

		// get a cos(alpha) of forward vector vs velocity
		float ForwardCosAngle = FVector::DotProduct(ForwardVector, NormalizedVel);
		// now get the alpha and convert to degree
		float ForwardDeltaDegree = FMath::RadiansToDegrees(FMath::Acos(ForwardCosAngle));

		// depending on where right vector is, flip it
		float RightCosAngle = FVector::DotProduct(RightVector, NormalizedVel);
		if (RightCosAngle < 0)
		{
			ForwardDeltaDegree *= -1;
		}

		return ForwardDeltaDegree;
	}

	return 0.f;
}

void UIKAnimInstance::SetRootOffset(float InRootYawOffset)
{
	if (Character)
	{
		const float NormalizedRootYawOffset = UKismetMathLibrary::NormalizeAxis(InRootYawOffset);

		const float ClampedRootYawOffset = UKismetMathLibrary::ClampAngle(NormalizedRootYawOffset, RootYawOffsetAngleClamp_Left, RootYawOffsetAngleClamp_Right);

		if (RootYawOffsetAngleClamp_Left == RootYawOffsetAngleClamp_Right)
			RootYawOffset = ClampedRootYawOffset;
		else
			RootYawOffset = NormalizedRootYawOffset;

		AimYaw = RootYawOffset * -1.f;

		SpineRotation = FRotator(0.f, AimYaw / 4.f, 0.f);
	}
}

void UIKAnimInstance::SetFootLocking(const FName EnableFootIKCurve, const FName FootLockCurve, const FName IKFootBone,
	float &CurrentFootLockAlpha, FVector &CurrentFootLockLocation, FRotator &CurrentFootLockRotation, const float DeltaSeconds) const
{
	if (GetAnimCurve_Compact(EnableFootIKCurve) <= 0.f)
		return;

	float FootLockCurveValue = GetAnimCurve_Compact(FootLockCurve);

	if (FootLockCurveValue >= 0.99 || FootLockCurveValue < CurrentFootLockAlpha)
		CurrentFootLockAlpha = FootLockCurveValue;

	if (CurrentFootLockAlpha >= 0.99)
	{
		FTransform FootTransform = GetOwningComponent()->GetSocketTransform(IKFootBone, RTS_Component);
		CurrentFootLockLocation = FootTransform.GetLocation();
		CurrentFootLockRotation = FootTransform.Rotator();
	}
	
	if (CurrentFootLockAlpha > 0.f)
		SetFootLockOffsets(CurrentFootLockLocation, CurrentFootLockRotation, DeltaSeconds);
}

void UIKAnimInstance::SetFootLockOffsets(FVector &LocalLocation, FRotator &LocalRotation, const float DeltaSeconds) const 
{
	FRotator RotationDifference = FRotator::ZeroRotator;
	if (Character->GetMyMovementComponent()->IsMovingOnGround())
		RotationDifference = UKismetMathLibrary::NormalizedDeltaRotator(Character->GetActorRotation(), Character->GetMyMovementComponent()->GetLastUpdateRotation());

	const FVector ScaledVelocity = Velocity * DeltaSeconds;
	const FVector LocationDifference = UKismetMathLibrary::LessLess_VectorRotator(ScaledVelocity, Character->Control_Rotation);
	
	LocalLocation = UKismetMathLibrary::RotateAngleAxis(LocalLocation - LocationDifference, RotationDifference.Yaw, FVector(0.f, 0.f, -1.f)); 

	LocalRotation = UKismetMathLibrary::NormalizedDeltaRotator(LocalRotation, RotationDifference);
}

void UIKAnimInstance::SetFootOffsets(FName Enable_FootIK_Curve, FName IKFootBone, FName RootBone,
	FVector &CurrentLocationTarget, FVector &CurrentLocationOffset, FRotator &CurrentRotationOffset, const float DeltaSeconds) const
{
	if (GetAnimCurve_Compact(Enable_FootIK_Curve) <= 0.f)
	{
		CurrentLocationOffset = FVector::ZeroVector;
		CurrentRotationOffset = FRotator::ZeroRotator;
		return;
	}

	FVector IKFootFloorLocation;
	IKFootFloorLocation.X = GetOwningComponent()->GetSocketLocation(IKFootBone).X;
	IKFootFloorLocation.Y = GetOwningComponent()->GetSocketLocation(IKFootBone).Y;
	IKFootFloorLocation.Z = GetOwningComponent()->GetSocketLocation(RootBone).Z;

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(Character);
	FCollisionResponseParams CollisionResponse;
	
	GetWorld()->LineTraceSingleByChannel(OUT HitResult, IKFootFloorLocation + FVector(0.f, 0.f, IKTraceDistanceAboveFoot), IKFootFloorLocation - FVector(0.f, 0.f, IKTraceDistanceBelowFoot), ECC_Visibility, CollisionParams, CollisionResponse);

	if (!Character->GetMyMovementComponent()->IsWalkable(HitResult))
		return;

	FVector ImpactPoint = HitResult.ImpactPoint;
	FVector ImpactNormal = HitResult.ImpactNormal;

	FVector temp1 = ImpactPoint + (ImpactNormal * FootHeight);
	FVector temp2 = IKFootFloorLocation + (FVector(0.f, 0.f, 1.f) * FootHeight);
	CurrentLocationTarget = temp1 - temp2;

	FRotator TargetRotationOffset;
	TargetRotationOffset.Roll = UKismetMathLibrary::Atan2(ImpactNormal.Y, ImpactNormal.Z);
	TargetRotationOffset.Pitch = UKismetMathLibrary::Atan2(ImpactNormal.X, ImpactNormal.Z) * -1.f;
	TargetRotationOffset.Yaw = 0.f;

	if (CurrentLocationOffset.Z > CurrentLocationTarget.Z)
		CurrentLocationOffset = UKismetMathLibrary::VInterpTo(CurrentLocationOffset, CurrentLocationTarget, DeltaSeconds, 30.f);
	else
		CurrentLocationOffset = UKismetMathLibrary::VInterpTo(CurrentLocationOffset, CurrentLocationTarget, DeltaSeconds, 15.f);

	CurrentRotationOffset = UKismetMathLibrary::RInterpTo(CurrentRotationOffset, TargetRotationOffset, DeltaSeconds, 30.f);
}

void UIKAnimInstance::SetPelvisIKOffset(FVector FootOffset_L_Target, FVector FootOffset_R_Target, const float DeltaSeconds)
{
	PelvisAlpha = (GetAnimCurve_Compact(FName("Enable_FootIK_L")) + GetAnimCurve_Compact(FName("Enable_FootIK_R"))) / 2.f;

	if (PelvisAlpha <= 0.f)
		PelvisOffset = FVector::ZeroVector;

	FVector PelvisTarget;
	if (LFootOffsetTarget.Z < RFootOffsetTarget.Z)
		PelvisTarget = LFootOffsetTarget;
	else
		PelvisTarget = RFootOffsetTarget;

	if (PelvisTarget.Z > PelvisOffset.Z)
		PelvisOffset = UKismetMathLibrary::VInterpTo(PelvisOffset, PelvisTarget, DeltaSeconds, 10.f);
	else
		PelvisOffset = UKismetMathLibrary::VInterpTo(PelvisOffset, PelvisTarget, DeltaSeconds, 15.f);
		
	
}

void UIKAnimInstance::ResetIKOffsets(const float DeltaSeconds)
{
	LFootOffsetLocation = UKismetMathLibrary::VInterpTo(LFootOffsetLocation, FVector::ZeroVector, DeltaSeconds, 15.f);
	RFootOffsetLocation = UKismetMathLibrary::VInterpTo(RFootOffsetLocation, FVector::ZeroVector, DeltaSeconds, 15.f);

	LFootOffsetRotation = UKismetMathLibrary::RInterpTo(LFootOffsetRotation, FRotator::ZeroRotator, DeltaSeconds, 15.f);
	RFootOffsetRotation = UKismetMathLibrary::RInterpTo(RFootOffsetRotation, FRotator::ZeroRotator, DeltaSeconds, 15.f);
}

#pragma endregion
