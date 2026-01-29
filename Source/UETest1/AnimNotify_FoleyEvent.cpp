#include "AnimNotify_FoleyEvent.h"

UAnimNotify_FoleyEvent::UAnimNotify_FoleyEvent()
{
	Side = EFoleyEventSide::None;
	VolumeMultiplier = 1.0f;
	PitchMultiplier = 1.0f;
	VisLogDebugColor = FLinearColor::Green;
}
