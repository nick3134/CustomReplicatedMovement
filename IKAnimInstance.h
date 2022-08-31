
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/Weapons/WeaponBase.h"
#include "IKAnimInstance.generated.h"

class AImpulseDefaultCharacter;
enum EWallRunSide;
enum EImpulseMovementMode;

UCLASS()
class IMPULSE_API UIKAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	
	UIKAnimInstance();

	virtual void NativeBeginPlay() override;

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	void UpdateCharacterInfo(const float DeltaSeconds);

	void UpdateMovementStates(const float DeltaSeconds);

	void UpdateMovementInfo(const float DeltaSeconds);

	void UpdateWeaponInfo(const float DeltaSeconds);

	void UpdateRotationInfo(const float DeltaSeconds);

	void UpdateRootYawOffset(const float DeltaSeconds);

	void UpdateLayerValues(const float DeltaSeconds);

	void UpdateFootIK(const float DeltaSeconds);

	UPROPERTY(BlueprintReadOnly, Category = "References")
	AImpulseDefaultCharacter* Character;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TEnumAsByte<EWeaponID> CurrentWeaponID = NoWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon Sway")
	FRotator RHandRotation;
	
	UPROPERTY(BlueprintReadOnly, Category = "HandIK")
	FTransform FP_LeftHandTransform;

	UPROPERTY(BlueprintReadOnly, Category = "HandIK")
	FTransform TPP_LeftHandTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Character Info")
	float Speed;

	UPROPERTY(BlueprintReadOnly, Category = "Character Info")
	FVector Velocity;
	
	UPROPERTY(BlueprintReadOnly, Category = "Character Info")
	float Direction;

	UPROPERTY(BlueprintReadOnly, Category = "Character Info")
	float Pitch;

	UPROPERTY(BlueprintReadOnly, Category = "Character Info")
	bool HasMovementInput = false;

	UPROPERTY(BlueprintReadOnly, Category = "Character Info")
	bool ShouldMove = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement States")
	TEnumAsByte<EImpulseMovementMode> ImpulseMovementMode;

	UPROPERTY(BlueprintReadOnly, Category = "Movement States")
	bool IsInAir;

	UPROPERTY(BlueprintReadOnly, Category = "Movement States")
	bool Jumped;

	UPROPERTY(BlueprintReadOnly, Category = "Movement States")
	bool IsCrouching;

	UPROPERTY(BlueprintReadOnly, Category = "Movement States")
	bool IsSliding;

	UPROPERTY(BlueprintReadOnly, Category = "Movement Info")
	TEnumAsByte<EWallRunSide> WallRunSide;
	
	bool FirstInstance = true;
	
	float AimAngle;

	float PrevAimAngle;

	float InitialAimAngle;
	
	float MinPlayRate = 1.5f;
	
	float MaxPlayRate = 4.f;
	
	float RotateDegreeThreshold = 20.f;

	UPROPERTY(BlueprintReadOnly, Category = "Rotation Info")
	float RootYawOffset = 0.f;

	float YawDeltaSinceLastUpdate = 0.f;

	FRotator WorldRotation = FRotator::ZeroRotator;

	FRotator RootRotation = FRotator::ZeroRotator;

	bool IsFirstUpdate = true;

	float RootYawOffsetAngleClamp_Left = -120.f;

	float RootYawOffsetAngleClamp_Right = 100.f;

	float AimYaw = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Rotation Info")
	bool Rotate_L = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rotation Info")
	bool Rotate_R = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rotation Info")
	float RotateRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Rotation Info")
	FRotator SpineRotation;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float BasePoseN = 1.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float BasePoseCLF = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float SpineAdd = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float HeadAdd = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float ArmAdd_L = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float ArmAdd_R = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float ArmLS_L = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float ArmLS_R = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float ArmMS_L = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float ArmMS_R = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float Hand_L = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float Hand_R = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float EnableHandIK_L = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Layered Blending")
	float EnableHandIK_R = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	float LFootLockAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector LFootLockLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FRotator LFootLockRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector LFootOffsetTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector LFootOffsetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FRotator LFootOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	float RFootLockAlpha = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector RFootLockLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FRotator RFootLockRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector RFootOffsetTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector RFootOffsetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FRotator RFootOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	float PelvisAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Foot IK")
	FVector PelvisOffset = FVector::ZeroVector;

	float IKTraceDistanceBelowFoot = 45.f;
	float IKTraceDistanceAboveFoot = 50.f;

	float FootHeight = 13.5f;

protected:
	
	void SetLeftHandIK();
	
	static float CalculateDirection(const FVector &Velocity, const FRotator &BaseRotation);

	float GetAnimCurve_Compact(const FName CurveName) const;

	void SetRootOffset(float InRootYawOffset);

	void SetFootLocking(const FName EnableFootIKCurve, const FName FootLockCurve, const FName IKFootBone, float &CurrentFootLockAlpha, FVector &CurrentFootLockLocation, FRotator &CurrentFootLockRotation, const float DeltaSeconds) const;

	void SetFootLockOffsets(FVector &LocalLocation, FRotator &LocalRotation, const float DeltaSeconds) const;

	void SetFootOffsets(FName Enable_FootIK_Curve, FName IKFootBone, FName RootBone, FVector &CurrentLocationTarget, FVector &CurrentLocationOffset, FRotator &CurrentRotationOffset, const float DeltaSeconds) const;

	void SetPelvisIKOffset(FVector FootOffset_L_Target, FVector FootOffset_R_Target, const float DeltaSeconds);

	void ResetIKOffsets(const float DeltaSeconds);
};
