# CURRENT_TASK

Goal: convert Blueprints to C++ in order from easiest to hardest.

Progress (Batch 1):
- Converted to C++ base + variants: BP_AnimNotify_FoleyEvent_Handplant_L, BP_AnimNotify_FoleyEvent_Handplant_R, BP_AnimNotify_FoleyEvent_Jump, BP_AnimNotify_FoleyEvent_Land, BP_AnimNotify_FoleyEvent_Run_L, BP_AnimNotify_FoleyEvent_Run_R, BP_AnimNotify_FoleyEvent_Scuff_L, BP_AnimNotify_FoleyEvent_Scuff_R, BP_AnimNotify_FoleyEvent_Walk_L, BP_AnimNotify_FoleyEvent_Walk_R.
- BFL_HelpfulFunctions: extracted + implemented in C++: DrawDebugArrowWithCircle, DrawDebugAngleThresholds. DebugDraw_MultiLineGraph extracted (requires DrawDebugLibrary API + S_DebugGraphLineProperties C++ struct before implementation).
- BFL_HelpfulFunctions: implemented in C++: AddToStringHistoryArray, GetObjectNames, GetPawnClassWithCVAR, GetVisualOverrideWithCVAR.
- BFL_HelpfulFunctions: implemented in C++: DebugDraw_BoolStates, DebugDraw_StringArray, DebugDraw_ObjectNameArray (uses DrawDebugLibrary).
- BFL_HelpfulFunctions: implemented in C++: DebugDraw_MultiLineGraph (requires S_DebugGraphLineProperties C++ struct).
- BFL_HelpfulFunctions: remaining graphs extracted. Summaries:
  - DebugDraw_BoolStates: TextLoc = Location + (Offset rotated by Rotation). TextRot = Rotator with Roll/Pitch swapped and Yaw+90. For each BoolNames index: State = BoolValues[index]. LineLoc = TextLoc - RotateVector((0,0,Index*5), Rotation). DrawDebugString(Name) at LineLoc (black). DrawDebugStringDimensions(Name) -> X; LineLoc2 = LineLoc + RotateVector((0, X+1, 0), Rotation). DrawDebugString(State ? "True" : "False") at LineLoc2 with LineStyle_Color = green/red. Drawer from MakeVisualLoggerDebugDrawer.
  - DebugDraw_StringArray: TextLoc/Rot same as above. If Label not empty, String=Label. For each Strings: if String empty -> String=Element else if Element equals Highlighted String (case-insensitive) -> String += "\r\n" + Prefix + Element + Highlight else String += "\r\n" + Prefix + Element. DrawDebugString(String) at TextLoc with Drawer.
  - DebugDraw_ObjectNameArray: TextLoc/Rot same as above. String=ArrayLabel. For each Objects: String += "\r\n" + GetDisplayName(Object). DrawDebugString(String) at TextLoc with Drawer.
  - AddToStringHistoryArray: Insert NewValue at index 0. If IsValidIndex(InOutValues, MaxHistoryCount) then RemoveIndex(MaxHistoryCount).
  - GetObjectNames: For each Objects -> GetDisplayName(Object) -> add to Names array -> return Names.
  - GetPawnClassWithCVAR: Index = GetConsoleVariableIntValue("DDCvar.PawnClass"). If index valid: Class = PawnClasses[Index]; return Class if IsValidClass(Class) else DefaultPawnClass. If index invalid -> DefaultPawnClass.
  - GetVisualOverrideWithCVAR: Index = GetConsoleVariableIntValue("DDCvar.VisualOverride"). If index valid -> return VisualOverrides[Index], else return null.
- Prepared C++ replacements: BP_NotifyState_EarlyTransition, BP_NotifyState_MontageBlendOut (requires reparent after compile).
- Reparented to C++: BP_NotifyState_EarlyTransition, BP_NotifyState_MontageBlendOut, PSC_DistanceToTraversalObject, PSC_Traversal_Head, PSC_Traversal_Pos, DistanceToSmartObject.
- Reparented to C++: GM_Locomotor.
- Reparented to C++: STC_CheckCooldown.
- Reparented to C++: BP_SmartObject_Base, BP_SmartBench.
- Reparented to C++: AC_FoleyEvents.
- Reparented to C++: DABP_FoleyAudioBank.
- Reparented to C++: BPI_SandboxCharacter_ABP, BPI_InteractionTransform, I_FoleyAudioBankInterface.
- Reparented to C++: BPI_SandboxCharacter_Pawn.
- Reparented to C++: PC_Locomotor.
- Reparented to C++: AC_VisualOverrideManager, STT_ClearFocus.
- Reparented to C++: AC_PreCMCTick.
- Reparented to C++: AIC_NPC_SmartObject.
- Reparented to C++: STT_FocusToTarget.
- Reparented to C++: STT_SetCharacterInputState.
- Reparented to C++: STT_UseSmartObject.
- Reparented to C++: STT_FindRandomLocation.
- Reparented to C++: STT_CharacterIgnoreCollisionsWithOtherActor.
- Reparented to C++: STT_AddCooldown.
- Reparented to C++: STT_ClaimSlot.
- BP_AnimNotify_FoleyEvent: already on AnimNotify_FoleyEvent C++ parent; compile failed in editor (needs check in UE output log).
- Blocked: AnimModifier BPs (AM_*). Editor-only modules cause linker errors when subclassed from game module. Needs an editor module or plugin to host those classes.
- Editor module created for AnimModifiers (UETest1Editor). AnimModifier subclasses removed after linker errors; AM_* remain blocked.
- Reparented to C++: BP_MovementMode_Falling, BP_MovementTransition_FromSlide, BP_MovementTransition_ToSlide.
- MCP: added read_timelines tool to expose timeline templates/tracks/keys for BP timeline conversion.
- SpinningArrow: C++ class created (timeline curves + spinner transform). Reparented to C++ and saved successfully after fixes.
- SpinningArrow fixes:
  - Avoided property name collisions by removing C++ subobject components for Spinner/Arrow; cache BP components by name at runtime.
  - Timeline curves moved to runtime with `Transient` properties to avoid illegal private object save errors.
- BP_Manny: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparented to C++.
- BP_Quinn: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparented to C++.
- BP_Twinblast: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparented to C++.
- BP_UE4_Mannequin: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparent pending until next build/load.
- BP_Echo: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparent pending until next build/load.
- BP_Manny: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparented to C++.
- BP_Quinn: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparented to C++.
- BP_Twinblast: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparented to C++.
- BP_UE4_Mannequin: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparent pending until next build/load.
- BP_Echo: C++ class created to match component setup and BeginPlay deferred AddTickPrerequisiteComponent logic. Reparent pending until next build/load.
- MCP Server Improvements (2026-01-29):
  - Fixed connection management: Implemented one-request-per-connection pattern to prevent stale CLOSE_WAIT connections
  - Added detailed compilation error reporting: compile_blueprint now returns arrays of errors/warnings with messages and severity
  - Added save_all command: Saves all modified assets in project, returns saved/failed counts
  - Added advanced MCP tools: delete_function_graph, refresh_nodes (with orphaned pin cleanup), set_blueprint_compile_settings, modify_function_metadata
  - **Successfully fixed 49 out of 56 errors via MCP (87.5% success rate):**
    - BPI_SandboxCharacter_ABP: Deleted 6 duplicate functions + made Get_InteractionTransform thread-safe
    - BPI_InteractionTransform: Deleted 3 "_Old" duplicate functions
    - I_FoleyAudioBankInterface: Deleted 1 duplicate function
    - AC_PreCMCTick: Fixed via C++ edit - added BlueprintCallable to Tick delegate
    - BFL_HelpfulFunctions: Deleted 10 conflicting function graphs (44 errors → 0)
    - GM_Sandbox: Refreshed nodes after BFL_HelpfulFunctions cleanup (1 error → 0)
    - AC_VisualOverrideManager: Refreshed nodes after BFL_HelpfulFunctions cleanup (1 error → 0)
    - PSC_Traversal_Head: Made Get_InteractionTransform thread-safe in C++ (1 error → 0)
    - PSC_Traversal_Pos: Made Get_InteractionTransform thread-safe in C++ (1 error → 0)
    - GameAnimationWidget: Refreshed nodes after BFL_HelpfulFunctions cleanup (2 errors → 0)
  - **Remaining 7 errors require manual Blueprint editor fixes (3 blueprints):**
    - SandboxCharacter_CMC_ABP (3 errors): Orphaned pins on DebugDraw_StringArray nodes + Return Node
    - SandboxCharacter_Mover_ABP (2 errors): Orphaned pins on DebugDraw_StringArray nodes
    - AC_FoleyEvents (2 errors): Variable Get nodes referencing deleted blueprint variable "Foley Event Bank"
  - **CRITICAL LIMITATION DISCOVERED - Orphaned Pins:**
    - **What**: When C++ function parameters are renamed (e.g., "Highlighted String" → "HighlightedString"), blueprint nodes retain connections to old pin names
    - **Why MCP can't fix**: Orphaned pin connection data is serialized in .uasset file, not accessible via Blueprint API
    - **Evidence**: Extensive research (see refs below) confirms no programmatic API exists to remove orphaned pin metadata
    - **MCP attempts made**: ReconstructNode(), BreakAllPinLinks(), FBlueprintEditorUtils::RefreshAllNodes(), post-reconstruction cleanup - all failed
    - **Only solution**: Manual deletion of stale wire connections in Blueprint editor (10-30 seconds per blueprint)
    - **References**:
      - RigVMNode::GetOrphanedPins API (Control Rig only, not K2 nodes): https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/RigVMNode/GetOrphanedPins
      - UE Bug Reports on orphaned pins: UE-50159, UE-45846, UE-46224
      - Forum consensus - manual fix required: https://forums.unrealengine.com/t/how-to-fix-broken-blueprint-pins-that-no-longer-exist/485666
      - Blueprint Compiler Internals explanation: https://ikrima.dev/ue4guide/engine-programming/blueprints/bp-compiler-internals-2/
  - **Lessons for future BP→C++ conversions:**
    - Avoid renaming function parameters when converting (keep spaces if blueprint had them, or use UPARAM DisplayName metadata)
    - Document which blueprints use debug functions so manual cleanup can be planned
    - Budget 30 seconds per blueprint for manual orphaned pin cleanup if parameters are renamed

Blockers / Notes:
- AM_* AnimModifiers: still blocked by linker errors even in editor module. Requires different module/plugin setup that properly links AnimationModifierLibrary.
- Graph-heavy BPs: MCP only provides node titles and counts, not pin wiring/logic. Need manual description or screenshots of graph logic to convert safely.
- New C++ classes not visible to editor after external build. Reparenting AC_VisualOverrideManager and STT_ClearFocus required editor reload (or in-editor compile).
- AC_VisualOverrideManager C++ logic inferred: toggles visibility of VisualOverride class actors based on DDCvar.VisualOverride (>=0 enables). Verify against BP if behavior differs.
- AIC_NPC_SmartObject Delay values pulled from graph pins (DedicatedServer=8.0, Client=2.0).

Remaining (auto, from table + progress list):
- BFL_HelpfulFunctions, AM_Copy_IKFootRoot, AM_DistanceFromLedge, AM_FootSpeed_L, AM_FootSpeed_R, AM_FootSteps_Run, AM_FootSteps_Walk, AM_MoveData_Speed, AM_RateWarpingAlpha, AM_Reset_Attach, CameraDirector_SandboxCharacter, AM_RemoveCurves, BP_Manny, BP_Quinn, BP_Twinblast, BP_UE4_Mannequin, STE_GetAIData, AM_TriggerWeightThreshold, BP_Echo, GM_Sandbox, SpinningArrow, PC_Sandbox, STT_AddCooldown, STT_ClaimSlot, AM_RenameCurve, StillCam, Teleporter_Level, AC_TraversalLogic, TargetDummy, Teleporter_Sender, LevelBlock_Traversable, Teleporter_Destination, STT_FindSmartObject, AM_ReorderCurves, LevelVisuals, AC_SmartObjectAnimation, STT_PlayAnimFromBestCost, LevelButton, BP_MovementMode_Slide, STT_PlayAnimMontage, AM_BakePhaseCurveFromFootstepNotifies, AM_FootSteps_Modulation, LevelBlock, BP_MovementMode_Walking, AM_OrientationWarpingAlpha, AM_WarpingAlpha, BP_Kellan.

Ordering metric (lowest first): score = variable_count + component_count + (graph_count * 2).

Columns: Name | Path | Parent | Vars | Components | Graphs | Score

| Name | Path | Parent | Vars | Components | Graphs | Score |
| --- | --- | --- | --- | --- | --- | --- |
| BFL_HelpfulFunctions | /Game/Blueprints/Data/BFL_HelpfulFunctions.BFL_HelpfulFunctions | BlueprintFunctionLibrary | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Handplant_L | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Handplant_L.BP_AnimNotify_FoleyEvent_Handplant_L | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Handplant_R | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Handplant_R.BP_AnimNotify_FoleyEvent_Handplant_R | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Jump | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Jump.BP_AnimNotify_FoleyEvent_Jump | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Land | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Land.BP_AnimNotify_FoleyEvent_Land | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Run_L | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Run_L.BP_AnimNotify_FoleyEvent_Run_L | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Run_R | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Run_R.BP_AnimNotify_FoleyEvent_Run_R | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Scuff_L | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Scuff_L.BP_AnimNotify_FoleyEvent_Scuff_L | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Scuff_R | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Scuff_R.BP_AnimNotify_FoleyEvent_Scuff_R | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Walk_L | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Walk_L.BP_AnimNotify_FoleyEvent_Walk_L | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BP_AnimNotify_FoleyEvent_Walk_R | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Walk_R.BP_AnimNotify_FoleyEvent_Walk_R | BP_AnimNotify_FoleyEvent_C | 0 | 0 | 0 | 0 |
| BPI_InteractionTransform | /Game/Blueprints/Data/BPI_InteractionTransform.BPI_InteractionTransform | BPI_InteractionTransform | 0 | 0 | 0 | 0 |
| BPI_SandboxCharacter_ABP | /Game/Blueprints/BPI_SandboxCharacter_ABP.BPI_SandboxCharacter_ABP | BPI_SandboxCharacter_ABP | 0 | 0 | 0 | 0 |
| BPI_SandboxCharacter_Pawn | /Game/Blueprints/BPI_SandboxCharacter_Pawn.BPI_SandboxCharacter_Pawn | Interface | 0 | 0 | 0 | 0 |
| I_FoleyAudioBankInterface | /Game/Audio/Foley/I_FoleyAudioBankInterface.I_FoleyAudioBankInterface | I_FoleyAudioBankInterface | 0 | 0 | 0 | 0 |
| AM_Copy_IKFootRoot | /Game/Blueprints/AnimModifiers/AM_Copy_IKFootRoot.AM_Copy_IKFootRoot | CopyBonesModifier | 0 | 0 | 1 | 2 |
| AM_DistanceFromLedge | /Game/Blueprints/AnimModifiers/AM_DistanceFromLedge.AM_DistanceFromLedge | MotionExtractorModifier | 0 | 0 | 1 | 2 |
| AM_FootSpeed_L | /Game/Blueprints/AnimModifiers/AM_FootSpeed_L.AM_FootSpeed_L | MotionExtractorModifier | 0 | 0 | 1 | 2 |
| AM_FootSpeed_R | /Game/Blueprints/AnimModifiers/AM_FootSpeed_R.AM_FootSpeed_R | MotionExtractorModifier | 0 | 0 | 1 | 2 |
| AM_FootSteps_Run | /Game/Blueprints/AnimModifiers/AM_FootSteps_Run.AM_FootSteps_Run | FootstepAnimEventsModifier | 0 | 0 | 1 | 2 |
| AM_FootSteps_Walk | /Game/Blueprints/AnimModifiers/AM_FootSteps_Walk.AM_FootSteps_Walk | FootstepAnimEventsModifier | 0 | 0 | 1 | 2 |
| AM_MoveData_Speed | /Game/Blueprints/AnimModifiers/AM_MoveData_Speed.AM_MoveData_Speed | MotionExtractorModifier | 0 | 0 | 1 | 2 |
| AM_RateWarpingAlpha | /Game/Blueprints/AnimModifiers/AM_RateWarpingAlpha.AM_RateWarpingAlpha | AM_OrientationWarpingAlpha_C | 0 | 0 | 1 | 2 |
| AM_Reset_Attach | /Game/Blueprints/AnimModifiers/AM_Reset_Attach.AM_Reset_Attach | CopyBonesModifier | 0 | 0 | 1 | 2 |
| BP_MovementMode_Falling | /Game/Blueprints/MovementModes/BP_MovementMode_Falling.BP_MovementMode_Falling | FallingMode | 0 | 0 | 1 | 2 |
| BP_MovementTransition_FromSlide | /Game/Blueprints/MovementModes/BP_MovementTransition_FromSlide.BP_MovementTransition_FromSlide | BaseMovementModeTransition | 0 | 0 | 1 | 2 |
| BP_MovementTransition_ToSlide | /Game/Blueprints/MovementModes/BP_MovementTransition_ToSlide.BP_MovementTransition_ToSlide | BaseMovementModeTransition | 0 | 0 | 1 | 2 |
| DistanceToSmartObject | /Game/Blueprints/SmartObjects/DistanceToSmartObject.DistanceToSmartObject | PoseSearchFeatureChannel_Distance | 0 | 0 | 1 | 2 |
| PC_Locomotor | /Game/Locomotor/PC_Locomotor.PC_Locomotor | PC_Locomotor | 0 | 0 | 1 | 2 |
| PSC_DistanceToTraversalObject | /Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Channels/PSC_DistanceToTraversalObject.PSC_DistanceToTraversalObject | PoseSearchFeatureChannel_Distance | 0 | 0 | 1 | 2 |
| PSC_Traversal_Head | /Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Channels/PSC_Traversal_Head.PSC_Traversal_Head | PoseSearchFeatureChannel_Heading | 0 | 0 | 1 | 2 |
| PSC_Traversal_Pos | /Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Channels/PSC_Traversal_Pos.PSC_Traversal_Pos | PoseSearchFeatureChannel_Position | 0 | 0 | 1 | 2 |
| AC_VisualOverrideManager | /Game/Blueprints/AC_VisualOverrideManager.AC_VisualOverrideManager | AC_VisualOverrideManager | 1 | 0 | 1 | 3 |
| BP_NotifyState_EarlyTransition | /Game/Blueprints/AnimNotifies/BP_NotifyState_EarlyTransition.BP_NotifyState_EarlyTransition | AnimNotifyState | 3 | 0 | 0 | 3 |
| BP_NotifyState_MontageBlendOut | /Game/Blueprints/AnimNotifies/BP_NotifyState_MontageBlendOut.BP_NotifyState_MontageBlendOut | AnimNotifyState | 3 | 0 | 0 | 3 |
| CameraDirector_SandboxCharacter | /Game/Blueprints/Cameras/CameraDirector_SandboxCharacter.CameraDirector_SandboxCharacter | BlueprintCameraDirectorEvaluator | 1 | 0 | 1 | 3 |
| DABP_FoleyAudioBank | /Game/Audio/Foley/DABP_FoleyAudioBank.DABP_FoleyAudioBank | PrimaryDataAsset | 1 | 0 | 1 | 3 |
| GM_Locomotor | /Game/Locomotor/GM_Locomotor.GM_Locomotor | GameModeBase | 0 | 1 | 1 | 3 |
| STT_ClearFocus | /Game/Blueprints/AI/StateTree/TasksAndConditions/STT_ClearFocus.STT_ClearFocus | STT_ClearFocus | 1 | 0 | 1 | 3 |
| AC_PreCMCTick | /Game/Blueprints/AC_PreCMCTick.AC_PreCMCTick | ActorComponent | 2 | 0 | 1 | 4 |
| AIC_NPC_SmartObject | /Game/Blueprints/AI/AIC_NPC_SmartObject.AIC_NPC_SmartObject | AIC_NPC_SmartObject | 1 | 1 | 1 | 4 |
| AM_RemoveCurves | /Game/Blueprints/AnimModifiers/AM_RemoveCurves.AM_RemoveCurves | AnimationModifier | 2 | 0 | 1 | 4 |
| BP_Manny | /Game/Blueprints/RetargetedCharacters/BP_Manny.BP_Manny | Actor | 0 | 2 | 1 | 4 |
| BP_Quinn | /Game/Blueprints/RetargetedCharacters/BP_Quinn.BP_Quinn | Actor | 0 | 2 | 1 | 4 |
| BP_SmartBench | /Game/Blueprints/SmartObjects/Bench/BP_SmartBench.BP_SmartBench | BP_SmartObject_Base_C | 1 | 1 | 1 | 4 |
| BP_SmartObject_Base | /Game/Blueprints/SmartObjects/BP_SmartObject_Base.BP_SmartObject_Base | Actor | 0 | 2 | 1 | 4 |
| BP_Twinblast | /Game/Blueprints/RetargetedCharacters/BP_Twinblast.BP_Twinblast | Actor | 0 | 2 | 1 | 4 |
| BP_UE4_Mannequin | /Game/Blueprints/RetargetedCharacters/BP_UE4_Mannequin.BP_UE4_Mannequin | Actor | 0 | 2 | 1 | 4 |
| STC_CheckCooldown | /Game/Blueprints/SmartObjects/TasksAndConditions/STC_CheckCooldown.STC_CheckCooldown | StateTreeConditionBlueprintBase | 2 | 0 | 1 | 4 |
| STE_GetAIData | /Game/Blueprints/SmartObjects/TasksAndConditions/STE_GetAIData.STE_GetAIData | StateTreeEvaluatorBlueprintBase | 2 | 0 | 1 | 4 |
| STT_FocusToTarget | /Game/Blueprints/AI/StateTree/TasksAndConditions/STT_FocusToTarget.STT_FocusToTarget | STT_FocusToTarget | 2 | 0 | 1 | 4 |
| STT_SetCharacterInputState | /Game/Blueprints/AI/StateTree/TasksAndConditions/STT_SetCharacterInputState.STT_SetCharacterInputState | STT_SetCharacterInputState | 2 | 0 | 1 | 4 |
| STT_UseSmartObject | /Game/Blueprints/SmartObjects/TasksAndConditions/STT_UseSmartObject.STT_UseSmartObject | StateTreeTaskBlueprintBase | 2 | 0 | 1 | 4 |
| AC_FoleyEvents | /Game/Audio/Foley/AC_FoleyEvents.AC_FoleyEvents | ActorComponent | 3 | 0 | 1 | 5 |
| AM_TriggerWeightThreshold | /Game/Blueprints/AnimModifiers/AM_TriggerWeightThreshold.AM_TriggerWeightThreshold | AnimationModifier | 3 | 0 | 1 | 5 |
| BP_Echo | /Game/Blueprints/RetargetedCharacters/BP_Echo.BP_Echo | Actor | 0 | 3 | 1 | 5 |
| BP_Walker | /Game/Locomotor/BP_Walker.BP_Walker | Character | 0 | 3 | 1 | 5 |
| GM_Sandbox | /Game/Blueprints/GM_Sandbox.GM_Sandbox | GameModeBase | 2 | 1 | 1 | 5 |
| SpinningArrow | /Game/Levels/LevelPrototyping/SpinningArrow.SpinningArrow | Actor | 0 | 3 | 1 | 5 |
| STT_CharacterIgnoreCollisionsWithOtherActor | /Game/Blueprints/AI/StateTree/TasksAndConditions/STT_CharacterIgnoreCollisionsWithOtherActor.STT_CharacterIgnoreCollisionsWithOtherActor | STT_CharacterIgnoreCollisionsWithOtherActor | 3 | 0 | 1 | 5 |
| STT_FindRandomLocation | /Game/Blueprints/AI/StateTree/TasksAndConditions/STT_FindRandomLocation.STT_FindRandomLocation | STT_FindRandomLocation | 3 | 0 | 1 | 5 |
| PC_Sandbox | /Game/Blueprints/PC_Sandbox.PC_Sandbox | PlayerController | 4 | 0 | 1 | 6 |
| STT_AddCooldown | /Game/Blueprints/SmartObjects/TasksAndConditions/STT_AddCooldown.STT_AddCooldown | STT_AddCooldown | 4 | 0 | 1 | 6 |
| STT_ClaimSlot | /Game/Blueprints/SmartObjects/TasksAndConditions/STT_ClaimSlot.STT_ClaimSlot | STT_ClaimSlot | 4 | 0 | 1 | 6 |
| AM_RenameCurve | /Game/Blueprints/AnimModifiers/AM_RenameCurve.AM_RenameCurve | AnimationModifier | 5 | 0 | 1 | 7 |
| BP_AnimNotify_FoleyEvent | /Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent.BP_AnimNotify_FoleyEvent | AnimNotify | 7 | 0 | 0 | 7 |
| StillCam | /Game/Widgets/WidgetData/StillCam.StillCam | Actor | 4 | 1 | 1 | 7 |
| Teleporter_Level | /Game/Levels/LevelPrototyping/Teleporter_Level.Teleporter_Level | Actor | 1 | 4 | 1 | 7 |
| AC_TraversalLogic | /Game/Blueprints/AC_TraversalLogic.AC_TraversalLogic | ActorComponent | 6 | 0 | 1 | 8 |
| TargetDummy | /Game/Levels/LevelPrototyping/TargetDummy.TargetDummy | Actor | 0 | 6 | 1 | 8 |
| Teleporter_Sender | /Game/Levels/LevelPrototyping/Teleporter_Sender.Teleporter_Sender | Actor | 1 | 5 | 1 | 8 |
| LevelBlock_Traversable | /Game/Levels/LevelPrototyping/LevelBlock_Traversable.LevelBlock_Traversable | LevelBlock_C | 3 | 4 | 1 | 9 |
| Teleporter_Destination | /Game/Levels/LevelPrototyping/Teleporter_Destination.Teleporter_Destination | Actor | 3 | 4 | 1 | 9 |
| STT_FindSmartObject | /Game/Blueprints/SmartObjects/TasksAndConditions/STT_FindSmartObject.STT_FindSmartObject | StateTreeTaskBlueprintBase | 8 | 0 | 1 | 10 |
| AM_ReorderCurves | /Game/Blueprints/AnimModifiers/AM_ReorderCurves.AM_ReorderCurves | AnimationModifier | 9 | 0 | 1 | 11 |
| LevelVisuals | /Game/Levels/LevelPrototyping/LevelVisuals.LevelVisuals | Actor | 3 | 6 | 1 | 11 |
| AC_SmartObjectAnimation | /Game/Blueprints/SmartObjects/AC_SmartObjectAnimation.AC_SmartObjectAnimation | ActorComponent | 10 | 0 | 1 | 12 |
| STT_PlayAnimFromBestCost | /Game/Blueprints/SmartObjects/TasksAndConditions/STT_PlayAnimFromBestCost.STT_PlayAnimFromBestCost | StateTreeTaskBlueprintBase | 10 | 0 | 1 | 12 |
| LevelButton | /Game/Levels/LevelPrototyping/LevelButton.LevelButton | Actor | 7 | 4 | 1 | 13 |
| BP_MovementMode_Slide | /Game/Blueprints/MovementModes/BP_MovementMode_Slide.BP_MovementMode_Slide | SmoothWalkingMode | 12 | 0 | 1 | 14 |
| STT_PlayAnimMontage | /Game/Blueprints/SmartObjects/TasksAndConditions/STT_PlayAnimMontage.STT_PlayAnimMontage | StateTreeTaskBlueprintBase | 12 | 0 | 1 | 14 |
| AM_BakePhaseCurveFromFootstepNotifies | /Game/Blueprints/AnimModifiers/AM_BakePhaseCurveFromFootstepNotifies.AM_BakePhaseCurveFromFootstepNotifies | AnimationModifier | 13 | 0 | 1 | 15 |
| AM_FootSteps_Modulation | /Game/Blueprints/AnimModifiers/AM_FootSteps_Modulation.AM_FootSteps_Modulation | AnimationModifier | 13 | 0 | 1 | 15 |
| LevelBlock | /Game/Levels/LevelPrototyping/LevelBlock.LevelBlock | Actor | 11 | 3 | 1 | 16 |
| BP_MovementMode_Walking | /Game/Blueprints/MovementModes/BP_MovementMode_Walking.BP_MovementMode_Walking | SmoothWalkingMode | 15 | 0 | 1 | 17 |
| AM_OrientationWarpingAlpha | /Game/Blueprints/AnimModifiers/AM_OrientationWarpingAlpha.AM_OrientationWarpingAlpha | AnimationModifier | 20 | 0 | 1 | 22 |
| AM_WarpingAlpha | /Game/Blueprints/AnimModifiers/AM_WarpingAlpha.AM_WarpingAlpha | AnimationModifier | 20 | 0 | 1 | 22 |
| BP_Kellan | /Game/MetaHumans/Kellan/BP_Kellan.BP_Kellan | Actor | 8 | 13 | 1 | 23 |
| SandboxCharacter_CMC | /Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC | Character | 20 | 10 | 1 | 32 |
| SandboxCharacter_Mover | /Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover | Pawn | 19 | 15 | 1 | 36 |
