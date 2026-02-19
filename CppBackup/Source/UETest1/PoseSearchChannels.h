#pragma once

#include "CoreMinimal.h"
#include "PoseSearch/PoseSearchFeatureChannel_Distance.h"
#include "PoseSearch/PoseSearchFeatureChannel_Heading.h"
#include "PoseSearch/PoseSearchFeatureChannel_Position.h"
#include "PoseSearchChannels.generated.h"

UCLASS()
class UETEST1_API UPSC_DistanceToTraversalObject : public UPoseSearchFeatureChannel_Distance
{
	GENERATED_BODY()
};

UCLASS()
class UETEST1_API UDistanceToSmartObjectChannel : public UPoseSearchFeatureChannel_Distance
{
	GENERATED_BODY()
};

UCLASS()
class UETEST1_API UPSC_Traversal_Head : public UPoseSearchFeatureChannel_Heading
{
	GENERATED_BODY()
};

UCLASS()
class UETEST1_API UPSC_Traversal_Pos : public UPoseSearchFeatureChannel_Position
{
	GENERATED_BODY()
};
