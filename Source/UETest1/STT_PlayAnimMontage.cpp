#include "STT_PlayAnimMontage.h"

#include "Animation/AnimMontage.h"
#include "Components/ActorComponent.h"
#include "SmartObjectSubsystem.h"
#include "UObject/ConstructorHelpers.h"

USTT_PlayAnimMontage::USTT_PlayAnimMontage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UActorComponent> SmartObjectAnimClassFinder(
		TEXT("/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation"));
	if (SmartObjectAnimClassFinder.Succeeded())
	{
		SmartObjectAnimationClass = SmartObjectAnimClassFinder.Class;
	}
}

EStateTreeRunStatus USTT_PlayAnimMontage::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition)
{
	// If no montage, finish immediately
	if (!MontageToPlay)
	{
		OnAnimationFinished();
		return EStateTreeRunStatus::Running;
	}

	// Get AC_SmartObjectAnimation component from Actor
	if (Actor && SmartObjectAnimationClass)
	{
		SmartObjectAnimComponent = Actor->GetComponentByClass(SmartObjectAnimationClass);
	}

	if (!SmartObjectAnimComponent)
	{
		OnAnimationFinished();
		return EStateTreeRunStatus::Running;
	}

	// Bind to OwnerMontageFinished delegate on the component
	{
		static const FName DelegateName(TEXT("OwnerMontageFinished"));

		if (FMulticastDelegateProperty* DelegateProp = CastField<FMulticastDelegateProperty>(
			SmartObjectAnimComponent->GetClass()->FindPropertyByName(DelegateName)))
		{
			FMulticastScriptDelegate* Delegate =
				DelegateProp->ContainerPtrToValuePtr<FMulticastScriptDelegate>(SmartObjectAnimComponent);

			if (Delegate)
			{
				FScriptDelegate ScriptDelegate;
				ScriptDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(USTT_PlayAnimMontage, OnAnimationFinished));
				Delegate->AddUnique(ScriptDelegate);
			}
		}
	}

	// Build the SmartObjectAnimationPayload struct and call PlayMontage_Multi
	{
		// Load the UserDefinedStruct
		static UScriptStruct* PayloadStruct = nullptr;
		if (!PayloadStruct)
		{
			UObject* LoadedObj = StaticLoadObject(UScriptStruct::StaticClass(), nullptr,
				TEXT("/Game/Blueprints/SmartObjects/SmartObjectAnimationPayload.SmartObjectAnimationPayload"));
			PayloadStruct = Cast<UScriptStruct>(LoadedObj);
		}

		if (PayloadStruct)
		{
			// Get slot transform from SmartObjectSubsystem
			FTransform WarpTargetTransform = FTransform::Identity;
			bool bUseWarpTarget = false;

			if (USmartObjectSubsystem* SOSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USmartObjectSubsystem>() : nullptr)
			{
				bUseWarpTarget = SOSubsystem->GetSlotTransform(SlotHandle, WarpTargetTransform);
			}

			// Find PlayMontage_Multi function
			static const FName PlayMontageFuncName(TEXT("PlayMontage_Multi"));
			UFunction* PlayMontageFunc = SmartObjectAnimComponent->GetClass()->FindFunctionByName(PlayMontageFuncName);

			if (PlayMontageFunc)
			{
				// Allocate and initialize the payload struct
				TArray<uint8> PayloadMemory;
				PayloadMemory.SetNumZeroed(PayloadStruct->GetStructureSize());
				PayloadStruct->InitializeStruct(PayloadMemory.GetData());

				// Set payload fields by name
				for (TFieldIterator<FProperty> It(PayloadStruct); It; ++It)
				{
					FProperty* Prop = *It;
					FString PropName = Prop->GetName();

					if (PropName.StartsWith(TEXT("MontageToPlay")))
					{
						*Prop->ContainerPtrToValuePtr<UAnimMontage*>(PayloadMemory.GetData()) = MontageToPlay;
					}
					else if (PropName.StartsWith(TEXT("PlayTime_")))
					{
						*Prop->ContainerPtrToValuePtr<double>(PayloadMemory.GetData()) = static_cast<double>(PlayTime);
					}
					else if (PropName.StartsWith(TEXT("RandomPlaytimeVariance")))
					{
						*Prop->ContainerPtrToValuePtr<double>(PayloadMemory.GetData()) = static_cast<double>(PlayTimeVariance);
					}
					else if (PropName.StartsWith(TEXT("StartTime")))
					{
						*Prop->ContainerPtrToValuePtr<double>(PayloadMemory.GetData()) = static_cast<double>(StartTime);
					}
					else if (PropName.StartsWith(TEXT("Playrate")))
					{
						*Prop->ContainerPtrToValuePtr<double>(PayloadMemory.GetData()) = static_cast<double>(PlayRate);
					}
					else if (PropName.StartsWith(TEXT("NumLoops")))
					{
						*Prop->ContainerPtrToValuePtr<int32>(PayloadMemory.GetData()) = NumberOfLoops;
					}
					else if (PropName.StartsWith(TEXT("WarpTargetTransform")))
					{
						*Prop->ContainerPtrToValuePtr<FTransform>(PayloadMemory.GetData()) = WarpTargetTransform;
					}
					else if (PropName.StartsWith(TEXT("UseWarpTarget")))
					{
						// BoolProperty uses a special accessor
						if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
						{
							BoolProp->SetPropertyValue_InContainer(PayloadMemory.GetData(), bUseWarpTarget);
						}
					}
				}

				// Build function params: the function takes the payload struct as parameter
				// Allocate params buffer for ProcessEvent
				TArray<uint8> ParamsMemory;
				ParamsMemory.SetNumZeroed(PlayMontageFunc->ParmsSize);

				// Find the Animation Payload parameter in the function and copy our struct into it
				for (TFieldIterator<FProperty> It(PlayMontageFunc); It; ++It)
				{
					FProperty* Param = *It;
					if (Param->HasAnyPropertyFlags(CPF_Parm) && !Param->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						if (FStructProperty* StructParam = CastField<FStructProperty>(Param))
						{
							if (StructParam->Struct == PayloadStruct)
							{
								FMemory::Memcpy(
									Param->ContainerPtrToValuePtr<void>(ParamsMemory.GetData()),
									PayloadMemory.GetData(),
									PayloadStruct->GetStructureSize());
								break;
							}
						}
					}
				}

				SmartObjectAnimComponent->ProcessEvent(PlayMontageFunc, ParamsMemory.GetData());

				// Clean up struct
				PayloadStruct->DestroyStruct(PayloadMemory.GetData());
			}
		}
	}

	return EStateTreeRunStatus::Running;
}

void USTT_PlayAnimMontage::OnAnimationFinished()
{
	FinishTask(true);
}
