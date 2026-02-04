# CURRENT_TASK

Goal: Convert Blueprints to C++ in order from easiest to hardest.

---

## CURRENT STATUS (2026-02-03)

**All 110 blueprints compile with 0 errors. Play in Editor functional but Sprint feature not working.**

### Known Issue: Sprint Not Working
- Sprint input action is bound but doesn't affect character movement
- Needs investigation

---

## BLOCKER: Blueprint vs C++ Interface/Struct Type Mismatch

**This is the main blocker for SandboxCharacter_CMC conversion.**

### Problem
The project has **duplicate interface and struct definitions** - one set in Blueprints and one in C++:

| Blueprint Asset | C++ Definition |
|----------------|----------------|
| `S_PlayerInputState` | `FS_PlayerInputState` |
| `S_CharacterPropertiesForAnimation` | `FS_CharacterPropertiesForAnimation` |
| `S_CharacterPropertiesForCamera` | `FS_CharacterPropertiesForCamera` |
| `S_CharacterPropertiesForTraversal` | `FS_CharacterPropertiesForTraversal` |
| `BPI_SandboxCharacter_Pawn` (BP) | `IBPI_SandboxCharacter_Pawn` (C++) |

When `SandboxCharacter_CMC` is reparented to C++:
- C++ class implements `IBPI_SandboxCharacter_Pawn` returning `FS_` structs
- But blueprints like `AC_TraversalLogic` call the BP interface expecting `S_` structs
- Unreal treats these as **completely different types**
- Result: "Accessed None" runtime errors

### Attempted Migration (Reverted)
- Deleted 9 conflicting functions from SandboxCharacter_CMC
- Removed interface implementation from blueprint
- Reparented blueprint to C++ class
- **Failed**: Runtime errors in AC_TraversalLogic due to struct type mismatch
- **Reverted**: SandboxCharacter_CMC.uasset restored from git

### Proper Migration Path (Future Work)
1. Delete blueprint structs (`S_CharacterPropertiesForAnimation`, `S_CharacterPropertiesForCamera`, `S_CharacterPropertiesForTraversal`, `S_PlayerInputState`)
2. Delete blueprint interface (`BPI_SandboxCharacter_Pawn`)
3. Update ALL blueprints that use these structs to use C++ structs (`FS_` prefix)
4. Then reparent SandboxCharacter_CMC to C++ class

---

## CONVERSION SUMMARY

### Successfully Converted (~53 blueprints)
- Level actors: LevelBlock, LevelVisuals, LevelButton, SpinningArrow, TargetDummy, StillCam
- Teleporter system: Teleporter_Level, Teleporter_Sender, Teleporter_Destination
- Characters: BP_Manny, BP_Quinn, BP_Twinblast, BP_UE4_Mannequin, BP_Echo, BP_Walker
- Controllers: PC_Sandbox, GM_Sandbox, PC_Locomotor, GM_Locomotor
- Smart Objects: BP_SmartObject_Base, BP_SmartBench
- Movement: BP_MovementMode_Walking, BP_MovementMode_Slide, BP_MovementMode_Falling
- StateTree: All STT_*, STE_*, STC_* classes
- AnimNotifies: All BP_AnimNotify_FoleyEvent variants, BP_NotifyState_*
- Components: AC_FoleyEvents, AC_PreCMCTick, AC_VisualOverrideManager
- Interfaces: BPI_SandboxCharacter_ABP, BPI_InteractionTransform, I_FoleyAudioBankInterface
- Data: BFL_HelpfulFunctions, DABP_FoleyAudioBank

### Cannot Convert - Too Complex (~5 blueprints)
- AC_TraversalLogic (125K chars event graph)
- STT_FindSmartObject (137K chars)
- AC_SmartObjectAnimation (111K chars)
- STT_PlayAnimFromBestCost (127K chars)

### Blocked - Editor Module Issues (~15 blueprints)
- All AM_* AnimModifiers (need proper editor module/plugin setup)

### High Priority - Blocked by Type Mismatch
- **SandboxCharacter_CMC** - Main character class
- **SandboxCharacter_Mover** - Mover-based character

### Low Priority
- BP_Kellan (MetaHuman character)
- Various animation blueprints (SandboxCharacter_CMC_ABP, SandboxCharacter_Mover_ABP)

---

## REFERENCE

**Original Sample Project**: `E:\repo\unreal_engine\GameAnimationSample\`
- Use for checking original blueprint configurations and default values

**Backup Folder**: `E:\repo\unreal_engine\backup\`
- Contains Content folder snapshots from various development points

**Reference Blueprints**: `Content/Levels/LevelPrototyping/*_ORIGINAL.uasset`
- 11 original blueprints preserved for property reference

---

## KEY LEARNINGS

### Blueprint CDO vs Level Instance Properties
- `read_class_defaults` reads Blueprint CDO values, NOT level instance overrides
- Use `read_actor_properties` for actual level instance values stored in .umap files
- CDO values are often placeholder values that don't match actual gameplay behavior

### Blueprint Sub-Object Property Updates
- Changes to blueprint CDOs don't automatically propagate to existing sub-object instances
- Use `clear_component_map_value_array` to modify properties within map value objects

### Actor Construction Dependencies
- OnConstruction execution order is undefined between actors
- Use `FTSTicker::GetCoreTicker()` for deferred updates when `GetWorld()` is null
- Only create `DynamicMaterialInstance` once, reuse on subsequent constructions

### Full C++ Conversion Principle
When encountering dependencies (e.g., blueprint struct inside another), convert **both** to C++ rather than attempting workarounds. Partial conversion blocks progress.
