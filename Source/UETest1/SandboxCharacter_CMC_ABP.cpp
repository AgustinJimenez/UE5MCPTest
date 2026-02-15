#include "SandboxCharacter_CMC_ABP.h"
#include "BPI_SandboxCharacter_Pawn.h"

void USandboxCharacter_CMC_ABP::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	AActor* Owner = GetOwningActor();
	if (!Owner) return;

	bool bImplements = Owner->GetClass()->ImplementsInterface(UBPI_SandboxCharacter_Pawn::StaticClass());
	if (!bImplements) return;

	// Get interface data every frame
	FS_CharacterPropertiesForAnimation Props = IBPI_SandboxCharacter_Pawn::Execute_Get_PropertiesForAnimation(Owner);
	double Speed2D = FVector(Props.Velocity.X, Props.Velocity.Y, 0.0).Size();

	// Call BP update functions for animation pipeline
	{
		UFunction* UpdatePropsFunc = GetClass()->FindFunctionByName(TEXT("Update_PropertiesFromCharacter"));
		UFunction* UpdateLogicFunc = GetClass()->FindFunctionByName(TEXT("Update_Logic"));
		UFunction* UpdateTrajFunc = GetClass()->FindFunctionByName(TEXT("Update_Trajectory"));
		UFunction* UpdateEssFunc = GetClass()->FindFunctionByName(TEXT("Update_EssentialValues"));
		UFunction* UpdateStatesFunc = GetClass()->FindFunctionByName(TEXT("Update_States"));

		if (UpdatePropsFunc)
		{
			ProcessEvent(UpdatePropsFunc, nullptr);
		}
		if (UpdateLogicFunc)
		{
			ProcessEvent(UpdateLogicFunc, nullptr);
		}
	}

	// Directly set BP variables via reflection every frame
	auto SetDoubleProperty = [this](const TCHAR* Name, double Value) {
		if (FProperty* Prop = GetClass()->FindPropertyByName(Name))
		{
			if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
				DoubleProp->SetPropertyValue_InContainer(this, Value);
			else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
				FloatProp->SetPropertyValue_InContainer(this, (float)Value);
		}
	};

	auto SetByteEnumProperty = [this](const TCHAR* Name, uint8 Value) {
		if (FProperty* Prop = GetClass()->FindPropertyByName(Name))
		{
			if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
				ByteProp->SetPropertyValue_InContainer(this, Value);
			else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				FNumericProperty* UnderProp = EnumProp->GetUnderlyingProperty();
				UnderProp->SetIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(this), (int64)Value);
			}
		}
	};

	auto SetBoolProperty = [this](const TCHAR* Name, bool Value) {
		if (FBoolProperty* Prop = CastField<FBoolProperty>(GetClass()->FindPropertyByName(Name)))
		{
			Prop->SetPropertyValue_InContainer(this, Value);
		}
	};

	// Essential values
	SetDoubleProperty(TEXT("Speed2D"), Speed2D);
	SetByteEnumProperty(TEXT("Gait"), (uint8)Props.Gait);
	SetByteEnumProperty(TEXT("MovementMode"), (uint8)Props.MovementMode);
	SetByteEnumProperty(TEXT("Stance"), (uint8)Props.Stance);
	SetByteEnumProperty(TEXT("RotationMode"), (uint8)Props.RotationMode);

	// Movement state: Idle vs Moving based on speed
	uint8 MovState = Speed2D > 1.0 ? 1 : 0; // 0=Idle, 1=Moving
	SetByteEnumProperty(TEXT("MovementState"), MovState);

	// HasOwningActor
	SetBoolProperty(TEXT("HasOwningActor"), true);

	// Velocity-related
	SetDoubleProperty(TEXT("Speed"), Props.Velocity.Size());

	// Additional commonly needed values
	if (FStructProperty* CharPropsProp = CastField<FStructProperty>(GetClass()->FindPropertyByName(TEXT("CharacterProperties"))))
	{
		void* StructPtr = CharPropsProp->ContainerPtrToValuePtr<void>(this);
		CharPropsProp->CopyCompleteValue(StructPtr, &Props);
	}

}
