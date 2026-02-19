// Copyright Epic Games, Inc. All Rights Reserved.

#include "TargetDummy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"

ATargetDummy::ATargetDummy()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATargetDummy::BeginPlay()
{
	Super::BeginPlay();

	// Cache blueprint components by name
	CachedDefaultSceneRoot = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("DefaultSceneRoot")));
	CachedCylinder = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Cylinder")));
	CachedSphere = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Sphere")));
	CachedTrigger = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Trigger")));
	CachedText = Cast<UTextRenderComponent>(GetDefaultSubobjectByName(TEXT("Text")));
	CachedZone = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Zone")));

	// Bind overlap events to Trigger component
	if (CachedTrigger)
	{
		CachedTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATargetDummy::OnTriggerBeginOverlap);
		CachedTrigger->OnComponentEndOverlap.AddDynamic(this, &ATargetDummy::OnTriggerEndOverlap);
	}
}

void ATargetDummy::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	// Check if OtherActor is a SandboxCharacter_Mover (blueprint class)
	if (OtherActor->GetClass()->GetName().Contains(TEXT("SandboxCharacter_Mover")))
	{
		// Access the TargetableActors array property
		if (FArrayProperty* ArrayProp = FindFieldChecked<FArrayProperty>(OtherActor->GetClass(), TEXT("TargetableActors")))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(OtherActor));

			// Add this actor to the array
			int32 NewIndex = ArrayHelper.AddValue();
			FObjectProperty* InnerProp = CastField<FObjectProperty>(ArrayProp->Inner);
			if (InnerProp)
			{
				InnerProp->SetObjectPropertyValue(ArrayHelper.GetRawPtr(NewIndex), this);
			}
		}
	}
}

void ATargetDummy::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	// Check if OtherActor is a SandboxCharacter_Mover (blueprint class)
	if (OtherActor->GetClass()->GetName().Contains(TEXT("SandboxCharacter_Mover")))
	{
		// Access the TargetableActors array property
		if (FArrayProperty* ArrayProp = FindFieldChecked<FArrayProperty>(OtherActor->GetClass(), TEXT("TargetableActors")))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(OtherActor));

			// Remove this actor from the array
			for (int32 i = ArrayHelper.Num() - 1; i >= 0; --i)
			{
				FObjectProperty* InnerProp = CastField<FObjectProperty>(ArrayProp->Inner);
				if (InnerProp)
				{
					AActor* Element = Cast<AActor>(InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(i)));
					if (Element == this)
					{
						ArrayHelper.RemoveValues(i, 1);
						break;
					}
				}
			}
		}
	}
}
