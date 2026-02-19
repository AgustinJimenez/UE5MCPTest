#include "CameraDirector_SandboxCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraDirector_SandboxCharacter)

UWorld* UCameraDirector_SandboxCharacter::GetWorld() const
{
	// Get world from outer object chain
	if (UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}
	return nullptr;
}
