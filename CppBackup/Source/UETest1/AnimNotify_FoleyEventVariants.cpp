#include "AnimNotify_FoleyEventVariants.h"
#include "GameplayTagContainer.h"

UAnimNotify_FoleyEvent_Handplant_L::UAnimNotify_FoleyEvent_Handplant_L()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Handplant"));
	Side = EFoleyEventSide::Left;
}

UAnimNotify_FoleyEvent_Handplant_R::UAnimNotify_FoleyEvent_Handplant_R()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Handplant"));
	Side = EFoleyEventSide::Right;
}

UAnimNotify_FoleyEvent_Jump::UAnimNotify_FoleyEvent_Jump()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Jump"));
	Side = EFoleyEventSide::None;
}

UAnimNotify_FoleyEvent_Land::UAnimNotify_FoleyEvent_Land()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Land"));
	Side = EFoleyEventSide::None;
}

UAnimNotify_FoleyEvent_Run_L::UAnimNotify_FoleyEvent_Run_L()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Run"));
	Side = EFoleyEventSide::Left;
}

UAnimNotify_FoleyEvent_Run_R::UAnimNotify_FoleyEvent_Run_R()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Run"));
	Side = EFoleyEventSide::Right;
}

UAnimNotify_FoleyEvent_Scuff_L::UAnimNotify_FoleyEvent_Scuff_L()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Scuff"));
	Side = EFoleyEventSide::Left;
}

UAnimNotify_FoleyEvent_Scuff_R::UAnimNotify_FoleyEvent_Scuff_R()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Scuff"));
	Side = EFoleyEventSide::Right;
}

UAnimNotify_FoleyEvent_Walk_L::UAnimNotify_FoleyEvent_Walk_L()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Walk"));
	Side = EFoleyEventSide::Left;
}

UAnimNotify_FoleyEvent_Walk_R::UAnimNotify_FoleyEvent_Walk_R()
{
	Event = FGameplayTag::RequestGameplayTag(TEXT("Foley.Event.Walk"));
	Side = EFoleyEventSide::Right;
}
