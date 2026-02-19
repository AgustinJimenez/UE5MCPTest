#include "LevelBlock_Traversable.h"

ALevelBlock_Traversable::ALevelBlock_Traversable()
{
	// Create spline components
	Ledge_1 = CreateDefaultSubobject<USplineComponent>(TEXT("Ledge_1"));
	Ledge_1->SetupAttachment(RootComponent);

	Ledge_2 = CreateDefaultSubobject<USplineComponent>(TEXT("Ledge_2"));
	Ledge_2->SetupAttachment(RootComponent);

	Ledge_3 = CreateDefaultSubobject<USplineComponent>(TEXT("Ledge_3"));
	Ledge_3->SetupAttachment(RootComponent);

	Ledge_4 = CreateDefaultSubobject<USplineComponent>(TEXT("Ledge_4"));
	Ledge_4->SetupAttachment(RootComponent);

	// Initialize MinLedgeWidth
	MinLedgeWidth = 0.0;
}

void ALevelBlock_Traversable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Populate Ledges array
	Ledges.Empty();
	Ledges.Add(Ledge_1);
	Ledges.Add(Ledge_2);
	Ledges.Add(Ledge_3);
	Ledges.Add(Ledge_4);

	// Populate OppositeLedges map
	// Ledge_1 <-> Ledge_2, Ledge_3 <-> Ledge_4
	OppositeLedges.Empty();
	OppositeLedges.Add(Ledge_1, Ledge_2);
	OppositeLedges.Add(Ledge_2, Ledge_1);
	OppositeLedges.Add(Ledge_3, Ledge_4);
	OppositeLedges.Add(Ledge_4, Ledge_3);
}
