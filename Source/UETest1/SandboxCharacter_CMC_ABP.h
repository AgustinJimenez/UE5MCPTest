#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "LocomotionEnums.h"
#include "SandboxCharacter_CMC_ABP.generated.h"

// Forward declarations
class UMoverComponent;
class UPoseSearchDatabase;
class UAnimationAsset;

UCLASS()
class UETEST1_API USandboxCharacter_CMC_ABP : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativePostEvaluateAnimation() override;

	// Main update functions
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Update_CVarDrivenVariables();

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Update_PropertiesFromCharacter();

	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	void Update_Logic();

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void Debug_ExperimentalStateMachine();

	// Sub-update functions called by Update_Logic
	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	void Update_Trajectory();

	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	void Update_EssentialValues();

	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	void Update_States();

	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	void Update_MovementDirection();

	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))
	void Update_TargetRotation();

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void DebugDraws();

	// Movement state variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementMode MovementMode = E_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementMode MovementMode_LastFrame = E_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_RotationMode RotationMode = E_RotationMode::OrientToMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_RotationMode RotationMode_LastFrame = E_RotationMode::OrientToMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementState MovementState = E_MovementState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementState MovementState_LastFrame = E_MovementState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_Gait Gait = E_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_Gait Gait_LastFrame = E_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_Stance Stance = E_Stance::Stand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_Stance Stance_LastFrame = E_Stance::Stand;

	// Character properties
	// TODO: Convert S_CharacterPropertiesForAnimation struct to C++
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	// struct FS_CharacterPropertiesForAnimation CharacterProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FTransform CharacterTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FTransform CharacterTransform_LastFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FTransform RootTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	bool HasOwningActor = false;

	// Acceleration and velocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool HasAcceleration = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector Acceleration_LastFrame = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double AccelerationAmount = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool HasVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector Velocity_LastFrame = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector RelativeAcceleration = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector VelocityAcceleration = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector LastNonZeroVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double Speed2D = 0.0;

	// Trajectory data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FPoseSearchQueryTrajectory TrajectoryGenerationData_Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FPoseSearchQueryTrajectory TrajectoryGenerationData_Moving;

	// TODO: Verify correct PoseSearch types
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	// FTrajectorySampleRange Trajectory;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	// FPoseSearchQueryTrajectory_WorldCollisionResults TrajectoryCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	double PreviousDesiredControllerYaw = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FVector Trj_PastVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FVector Trj_CurrentVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FVector Trj_FutureVelocity = FVector::ZeroVector;

	// Motion matching
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	int32 MMDatabaseLOD = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	TObjectPtr<UObject> CurrentSelectedAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	TObjectPtr<UPoseSearchDatabase> CurrentSelectedDatabase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	TArray<TObjectPtr<UPoseSearchDatabase>> ValidDatabases;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	double MM_Search_Cost = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	TArray<FName> CurrentDatabaseTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	TArray<TObjectPtr<UAnimationAsset>> ValidAnims;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionMatching")
	double Search_Cost = 0.0;

	// Landing and interaction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	double HeavyLandSpeedThreshold = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FTransform InteractionTransform;

	// Root offset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RootOffset")
	bool OffsetRootBoneEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RootOffset")
	double OffsetRootTranslationRadius = 0.0;

	// Blend stack
	// TODO: Convert S_BlendStackInputs struct to C++
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlendStack")
	// struct FS_BlendStackInputs BlendStackInputs;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlendStack")
	// struct FS_BlendStackInputs Previous_BlendStackInputs;

	// State machine
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine")
	E_ExperimentalStateMachineState StateMachineState = E_ExperimentalStateMachineState::IdleLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine")
	bool NoValidAnim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine")
	bool NotifyTransition_ReTransition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine")
	bool NotifyTransition_ToLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine")
	bool DebugExperimentalStateMachine = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateMachine")
	bool UseExperimentalStateMachine = false;

	// Foot placement
	// TODO: Find correct foot placement struct types or convert to uint8 placeholders
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootPlacement")
	// FFootPlacementPlantSettings PlantSettings_Default;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootPlacement")
	// FFootPlacementPlantSettings PlantSettings_Stops;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootPlacement")
	// FFootPlacementInterpolationSettings InterpolationSettings_Default;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootPlacement")
	// FFootPlacementInterpolationSettings InterpolationSettings_Stops;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FootPlacement")
	int32 FootPlacementMode = 0;

	// Movement direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementDirection MovementDirection = E_MovementDirection::F;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementDirection MovementDirectionLastFrame = E_MovementDirection::F;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementDirectionBias MovementDirectionBias = E_MovementDirectionBias::LeftFootForward;

	// TODO: Convert S_MovementDirectionThresholds struct to C++
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	// struct FS_MovementDirectionThresholds MovementDirectionThresholds;

	// Target rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FRotator TargetRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FRotator TargetRotationOnTransitionStart = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	double TargetRotationDelta = 0.0;

	// Thread safety
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool UseThreadSafeUpdateAnimation = false;

	// Locomotion setup
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 LocomotionSetup = 0;

	// Mover component reference
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	TObjectPtr<UMoverComponent> Mover;

	// Debug history arrays
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	TArray<FString> TransitionHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	TArray<double> PawnSpeedHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	TArray<double> MoveData_Speed_History;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	TArray<double> Phase_History;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	TArray<double> Contact_L_History;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	TArray<double> Contact_R_History;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	TArray<double> Enable_Warping_History;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDraws = false;
};
