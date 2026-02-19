#include "STT_FindSmartObject.h"

#include "SmartObjectSubsystem.h"
#include "SmartObjectComponent.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "GameplayTagContainer.h"

EStateTreeRunStatus USTT_FindSmartObject::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	if (!Actor)
	{
		FinishTask(false);
		return EStateTreeRunStatus::Running;
	}

	UWorld* World = Actor->GetWorld();
	if (!World)
	{
		FinishTask(false);
		return EStateTreeRunStatus::Running;
	}

	USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		FinishTask(false);
		return EStateTreeRunStatus::Running;
	}

	// Build request
	const FVector ActorLocation = Actor->GetActorLocation();
	const FBox QueryBox(ActorLocation - SearchBoxExtents, ActorLocation + SearchBoxExtents);

	FSmartObjectRequestFilter Filter;
	Filter.UserTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("SmartObject.ObjectType.NPC"))));
	Filter.BehaviorDefinitionClasses.Add(UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass());
	Filter.bShouldEvaluateConditions = true;
	Filter.bShouldIncludeClaimedSlots = false;
	Filter.bShouldIncludeDisabledSlots = false;

	FSmartObjectRequest Request(QueryBox, Filter);

	// Execute search
	TArray<FSmartObjectRequestResult> SearchResults;
	const bool bFound = SOSubsystem->FindSmartObjects(Request, SearchResults, FConstStructView::Make(FSmartObjectActorUserData(Actor)));

	if (!bFound || SearchResults.IsEmpty())
	{
		FinishTask(false);
		return EStateTreeRunStatus::Running;
	}

	// Select result based on SearchType
	FSmartObjectRequestResult SelectedResult;

	switch (SearchType)
	{
	case ESmartObjectSearchType::ClosestDistance:
		SelectedResult = FindSlotByDistance(SearchResults, true);
		break;

	case ESmartObjectSearchType::FarthestDistance:
		SelectedResult = FindSlotByDistance(SearchResults, false);
		break;

	case ESmartObjectSearchType::FirstFound:
		SelectedResult = SearchResults[0];
		break;

	case ESmartObjectSearchType::Random:
	{
		const int32 RandomIndex = FMath::RandRange(0, SearchResults.Num() - 1);
		SelectedResult = SearchResults[RandomIndex];
		break;
	}
	}

	// Validate result and set outputs
	CandidateSlot = SelectedResult.SlotHandle;

	if (USmartObjectComponent* SOComponent = SOSubsystem->GetSmartObjectComponentByRequestResult(SelectedResult))
	{
		SmartObject = SOComponent->GetOwner();
	}

	if (IsValid(SmartObject) && CandidateSlot.IsValid())
	{
		FinishTask(true);
	}
	else
	{
		FinishTask(false);
	}

	return EStateTreeRunStatus::Running;
}

FSmartObjectRequestResult USTT_FindSmartObject::FindSlotByDistance(const TArray<FSmartObjectRequestResult>& Results, bool bClosest) const
{
	if (!Actor)
	{
		return FSmartObjectRequestResult();
	}

	UWorld* World = Actor->GetWorld();
	if (!World)
	{
		return FSmartObjectRequestResult();
	}

	USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		return FSmartObjectRequestResult();
	}

	const FVector ActorLocation = Actor->GetActorLocation();
	double BestDistance = 0.0;
	FSmartObjectRequestResult BestResult;

	for (const FSmartObjectRequestResult& Result : Results)
	{
		FTransform SlotTransform;
		if (SOSubsystem->GetSlotTransformFromRequestResult(Result, SlotTransform))
		{
			const double Distance = FVector::Dist(SlotTransform.GetLocation(), ActorLocation);

			if (bClosest)
			{
				if (BestDistance == 0.0 || Distance < BestDistance)
				{
					BestDistance = Distance;
					BestResult = Result;
				}
			}
			else
			{
				if (Distance > BestDistance)
				{
					BestDistance = Distance;
					BestResult = Result;
				}
			}
		}
	}

	return BestResult;
}
