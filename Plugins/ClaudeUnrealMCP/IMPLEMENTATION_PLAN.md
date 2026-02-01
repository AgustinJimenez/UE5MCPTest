# MCP Feature Implementation Plan

**Created:** 2026-02-01
**Status:** Planning Phase
**Goal:** Add blueprint node manipulation, function creation, and graph analysis capabilities

---

## Overview

This plan outlines the implementation of 3 high-priority feature sets identified in FEATURE_RESEARCH.md:

1. **Blueprint Function Creation** - Create/modify blueprint functions with inputs/outputs
2. **Blueprint Node Manipulation** - Add/delete/connect nodes in blueprint graphs
3. **Blueprint Graph Analysis** - Analyze graph complexity and patterns

---

## Phase 1: Blueprint Function Creation

### Features to Implement

#### 1.1 Create Function
**MCP Command:** `create_blueprint_function`
**Parameters:**
- `blueprint_path` (string) - Path to blueprint asset
- `function_name` (string) - Name of new function
- `category` (string, optional) - Function category
- `is_pure` (bool, optional) - Whether function is pure (no exec pins)
- `is_const` (bool, optional) - Whether function is const
- `access_specifier` (string, optional) - Public/Protected/Private

**Implementation:**
```cpp
FString HandleCreateBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Load blueprint
    UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

    // 2. Create new graph for function
    UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        FName(*FunctionName),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass()
    );

    // 3. Add function entry and result nodes
    UK2Node_FunctionEntry* EntryNode = CompilerContext.SpawnIntermediateNode<UK2Node_FunctionEntry>();
    UK2Node_FunctionResult* ResultNode = CompilerContext.SpawnIntermediateNode<UK2Node_FunctionResult>();

    // 4. Set function flags
    FunctionGraph->GetSchema()->SetPureFlagOnFunction(FunctionGraph, bIsPure);

    // 5. Add to blueprint
    FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FunctionGraph, /*bIsUserCreated=*/true, nullptr);

    return MakeResponse(true, ...);
}
```

**UE API Classes:**
- `FBlueprintEditorUtils::CreateNewGraph()`
- `FBlueprintEditorUtils::AddFunctionGraph()`
- `UK2Node_FunctionEntry`
- `UK2Node_FunctionResult`

#### 1.2 Add Function Input
**MCP Command:** `add_function_input`
**Parameters:**
- `blueprint_path` (string)
- `function_name` (string)
- `param_name` (string)
- `param_type` (string) - int, float, bool, string, object, vector, rotator, etc.
- `is_reference` (bool, optional)
- `default_value` (string, optional)

**Implementation:**
```cpp
FString HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Find function graph
    UEdGraph* FunctionGraph = FindFunctionGraph(Blueprint, FunctionName);

    // 2. Find function entry node
    UK2Node_FunctionEntry* EntryNode = FindFunctionEntryNode(FunctionGraph);

    // 3. Create pin on entry node
    FEdGraphPinType PinType = ParsePinType(ParamType);
    UEdGraphPin* NewPin = EntryNode->CreatePin(
        EGPD_Output,
        PinType,
        FName(*ParamName)
    );

    // 4. Set default value if provided
    if (!DefaultValue.IsEmpty())
    {
        NewPin->DefaultValue = DefaultValue;
    }

    // 5. Mark blueprint dirty
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    return MakeResponse(true, ...);
}
```

**Pin Type Parsing:**
```cpp
FEdGraphPinType ParsePinType(const FString& TypeString)
{
    FEdGraphPinType PinType;

    if (TypeString == "int") {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    } else if (TypeString == "float" || TypeString == "double") {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    } else if (TypeString == "bool") {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    } else if (TypeString == "string") {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    } else if (TypeString == "vector") {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    } else if (TypeString == "rotator") {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    } else if (TypeString == "transform") {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    } else if (TypeString.StartsWith("object:")) {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        // Parse object class name
        FString ClassName = TypeString.RightChop(7);
        PinType.PinSubCategoryObject = FindObject<UClass>(nullptr, *ClassName);
    }

    return PinType;
}
```

#### 1.3 Add Function Output
**MCP Command:** `add_function_output`
**Parameters:** Same as `add_function_input`

**Implementation:** Similar to input, but adds pin to `UK2Node_FunctionResult` instead

#### 1.4 Rename Function
**MCP Command:** `rename_blueprint_function`
**Parameters:**
- `blueprint_path` (string)
- `old_name` (string)
- `new_name` (string)

**Implementation:**
```cpp
FString HandleRenameFunction(const TSharedPtr<FJsonObject>& Params)
{
    UEdGraph* FunctionGraph = FindFunctionGraph(Blueprint, OldName);
    FBlueprintEditorUtils::RenameGraph(FunctionGraph, NewName);
    return MakeResponse(true, ...);
}
```

### Testing Plan - Phase 1

**Test 1: Create Simple Function**
```json
{
  "tool": "create_blueprint_function",
  "blueprint_path": "/Game/Test/BP_Test",
  "function_name": "CalculateDistance",
  "is_pure": true
}
```

**Test 2: Add Inputs**
```json
{
  "tool": "add_function_input",
  "blueprint_path": "/Game/Test/BP_Test",
  "function_name": "CalculateDistance",
  "param_name": "PointA",
  "param_type": "vector"
}
```

**Test 3: Add Output**
```json
{
  "tool": "add_function_output",
  "blueprint_path": "/Game/Test/BP_Test",
  "function_name": "CalculateDistance",
  "param_name": "Distance",
  "param_type": "float"
}
```

**Validation:**
- Function appears in blueprint function list
- Pins are visible in blueprint editor
- Types are correct
- Blueprint compiles successfully

---

## Phase 2: Blueprint Node Manipulation

### Features to Implement

#### 2.1 Add Node to Graph
**MCP Command:** `add_node_to_graph`
**Parameters:**
- `blueprint_path` (string)
- `graph_name` (string) - "EventGraph" or function name
- `node_type` (string) - "Event_BeginPlay", "CallFunction", "VariableGet", "Branch", etc.
- `position_x` (int, optional)
- `position_y` (int, optional)
- `function_name` (string, optional) - For CallFunction nodes
- `variable_name` (string, optional) - For VariableGet/Set nodes

**Node Types:**
- `Event_BeginPlay` - Begin Play event
- `Event_Tick` - Tick event
- `Event_Custom` - Custom event (requires event_name parameter)
- `CallFunction` - Function call (requires function_name)
- `VariableGet` - Get variable (requires variable_name)
- `VariableSet` - Set variable (requires variable_name)
- `Branch` - If/Then/Else branch
- `Sequence` - Sequence node
- `ForEach` - For each loop
- `PrintString` - Print string debug

**Implementation:**
```cpp
FString HandleAddNode(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Get graph
    UEdGraph* Graph = GetGraphByName(Blueprint, GraphName);

    // 2. Create node based on type
    UEdGraphNode* NewNode = nullptr;

    if (NodeType == "Event_BeginPlay")
    {
        UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(
            TEXT("ReceiveBeginPlay"),
            AActor::StaticClass()
        );
        NewNode = EventNode;
    }
    else if (NodeType == "CallFunction")
    {
        UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(Graph);
        UFunction* Function = FindUFunction(FunctionName);
        FuncNode->SetFromFunction(Function);
        NewNode = FuncNode;
    }
    else if (NodeType == "VariableGet")
    {
        UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(Graph);
        VarNode->VariableReference.SetSelfMember(FName(*VariableName));
        NewNode = VarNode;
    }
    else if (NodeType == "Branch")
    {
        UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
        NewNode = BranchNode;
    }

    // 3. Set position
    if (NewNode)
    {
        NewNode->NodePosX = PositionX;
        NewNode->NodePosY = PositionY;

        // 4. Add to graph
        Graph->AddNode(NewNode, /*bFromUI=*/false, /*bSelectNewNode=*/false);

        // 5. Allocate default pins
        NewNode->AllocateDefaultPins();

        // 6. Return node ID for connection
        TSharedPtr<FJsonObject> ResponseData = MakeShareable(new FJsonObject());
        ResponseData->SetStringField("node_id", NewNode->NodeGuid.ToString());
        return MakeResponse(true, ResponseData);
    }

    return MakeError("Failed to create node");
}
```

#### 2.2 Connect Nodes
**MCP Command:** `connect_nodes`
**Parameters:**
- `blueprint_path` (string)
- `graph_name` (string)
- `source_node_id` (string) - GUID from add_node response
- `source_pin_name` (string) - Pin name (e.g., "then", "ReturnValue")
- `target_node_id` (string)
- `target_pin_name` (string)

**Implementation:**
```cpp
FString HandleConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Find nodes by GUID
    UEdGraphNode* SourceNode = FindNodeByGuid(Graph, SourceGuid);
    UEdGraphNode* TargetNode = FindNodeByGuid(Graph, TargetGuid);

    // 2. Find pins
    UEdGraphPin* SourcePin = SourceNode->FindPin(FName(*SourcePinName));
    UEdGraphPin* TargetPin = TargetNode->FindPin(FName(*TargetPinName));

    // 3. Validate compatibility
    const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
    FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);

    if (Response.Response != CONNECT_RESPONSE_DISALLOW)
    {
        // 4. Make connection
        SourcePin->MakeLinkTo(TargetPin);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        return MakeResponse(true, ...);
    }

    return MakeError("Pins are not compatible: " + Response.Message.ToString());
}
```

#### 2.3 Delete Node
**MCP Command:** `delete_node_from_graph`
**Parameters:**
- `blueprint_path` (string)
- `graph_name` (string)
- `node_id` (string) - Node GUID

**Implementation:**
```cpp
FString HandleDeleteNode(const TSharedPtr<FJsonObject>& Params)
{
    UEdGraphNode* Node = FindNodeByGuid(Graph, NodeGuid);
    if (Node)
    {
        // Break all pin links first
        Node->BreakAllNodeLinks();

        // Remove from graph
        Graph->RemoveNode(Node);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        return MakeResponse(true, ...);
    }
    return MakeError("Node not found");
}
```

#### 2.4 Set Node Property
**MCP Command:** `set_node_property`
**Parameters:**
- `blueprint_path` (string)
- `graph_name` (string)
- `node_id` (string)
- `property_name` (string)
- `property_value` (string)

**Example:** Set default value on PrintString node
```json
{
  "tool": "set_node_property",
  "blueprint_path": "/Game/Test/BP_Test",
  "graph_name": "EventGraph",
  "node_id": "...",
  "property_name": "InString",
  "property_value": "Hello World"
}
```

### Testing Plan - Phase 2

**Test 1: Create Simple Event Graph**
```
1. Add BeginPlay event node
2. Add PrintString function call node
3. Connect BeginPlay "then" pin to PrintString "execute" pin
4. Set PrintString "InString" value to "Test Message"
5. Compile blueprint
6. Verify prints "Test Message" when game starts
```

**Test 2: Create Variable Get/Set**
```
1. Create variable "Counter" (int)
2. Add BeginPlay event
3. Add VariableGet for "Counter"
4. Add "Add" math node (int + int)
5. Add VariableSet for "Counter"
6. Connect: BeginPlay -> Get Counter -> Add (A=Counter, B=1) -> Set Counter
7. Compile and verify counter increments
```

---

## Phase 3: Blueprint Graph Analysis

### Features to Implement

#### 3.1 Analyze Graph Complexity
**MCP Command:** `analyze_graph_complexity`
**Parameters:**
- `blueprint_path` (string)
- `graph_name` (string, optional) - If omitted, analyzes all graphs

**Returns:**
```json
{
  "graphs": [
    {
      "name": "EventGraph",
      "metrics": {
        "node_count": 45,
        "execution_path_count": 3,
        "max_depth": 8,
        "branch_count": 12,
        "loop_count": 2,
        "function_call_count": 15,
        "complexity_score": 67
      }
    }
  ],
  "overall_complexity": "high",
  "conversion_difficulty": "moderate"
}
```

**Implementation:**
```cpp
FString HandleAnalyzeGraphComplexity(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> GraphsArray;

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        TSharedPtr<FJsonObject> GraphMetrics = AnalyzeGraph(Graph);
        GraphsArray.Add(MakeShareable(new FJsonValueObject(GraphMetrics)));
    }

    Result->SetArrayField("graphs", GraphsArray);
    Result->SetStringField("overall_complexity", CalculateOverallComplexity(GraphsArray));

    return MakeResponse(true, Result);
}

TSharedPtr<FJsonObject> AnalyzeGraph(UEdGraph* Graph)
{
    TSharedPtr<FJsonObject> Metrics = MakeShareable(new FJsonObject());

    // Count nodes
    int32 NodeCount = Graph->Nodes.Num();
    int32 BranchCount = 0;
    int32 LoopCount = 0;
    int32 FunctionCallCount = 0;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->IsA<UK2Node_IfThenElse>()) BranchCount++;
        else if (Node->IsA<UK2Node_ForEachLoop>()) LoopCount++;
        else if (Node->IsA<UK2Node_CallFunction>()) FunctionCallCount++;
    }

    // Calculate max depth via BFS
    int32 MaxDepth = CalculateGraphDepth(Graph);

    // Calculate complexity score
    int32 ComplexityScore = NodeCount +
                           (BranchCount * 2) +
                           (LoopCount * 3) +
                           (MaxDepth * 5);

    Metrics->SetNumberField("node_count", NodeCount);
    Metrics->SetNumberField("branch_count", BranchCount);
    Metrics->SetNumberField("loop_count", LoopCount);
    Metrics->SetNumberField("function_call_count", FunctionCallCount);
    Metrics->SetNumberField("max_depth", MaxDepth);
    Metrics->SetNumberField("complexity_score", ComplexityScore);

    return Metrics;
}
```

#### 3.2 Find Conversion Candidates
**MCP Command:** `find_conversion_candidates`
**Parameters:**
- `blueprint_path` (string)
- `max_complexity` (int, optional) - Only return functions below this complexity

**Returns:**
```json
{
  "simple_functions": [
    {
      "name": "CalculateDistance",
      "complexity": 5,
      "nodes": 3,
      "recommendation": "Easy - Pure math function"
    }
  ],
  "moderate_functions": [...],
  "complex_functions": [...]
}
```

#### 3.3 Detect Graph Patterns
**MCP Command:** `detect_graph_patterns`
**Parameters:**
- `blueprint_path` (string)
- `graph_name` (string)

**Returns:**
```json
{
  "patterns": [
    {
      "type": "state_machine",
      "description": "Manual state machine using Switch node",
      "location": "EventGraph nodes 15-32",
      "suggestion": "Consider using enum-based state machine pattern"
    },
    {
      "type": "timer_loop",
      "description": "Timer-based loop for periodic updates",
      "location": "EventGraph nodes 45-50"
    }
  ]
}
```

### Testing Plan - Phase 3

**Test 1: Analyze SandboxCharacter_CMC_ABP**
```
1. Run analyze_graph_complexity on main ABP
2. Verify complexity scores match our manual assessment
3. Check that Update_States is marked as "simple"
4. Check that Update_Trajectory is marked as "complex"
```

**Test 2: Find Conversion Candidates**
```
1. Run find_conversion_candidates with max_complexity=30
2. Should return Update_States, Update_TargetRotation
3. Should NOT return Update_Trajectory
```

---

## Implementation Order

### Sprint 1: Function Creation (Days 1-2)
- [ ] Implement `create_blueprint_function`
- [ ] Implement `add_function_input`
- [ ] Implement `add_function_output`
- [ ] Implement `rename_blueprint_function`
- [ ] Test function creation workflow
- [ ] Update MCP server index.js with new commands

### Sprint 2: Node Manipulation (Days 3-4)
- [ ] Implement `add_node_to_graph`
- [ ] Support for Event nodes (BeginPlay, Tick, Custom)
- [ ] Support for Function call nodes
- [ ] Support for Variable Get/Set nodes
- [ ] Support for Branch/Sequence/Loop nodes
- [ ] Implement `connect_nodes`
- [ ] Implement `delete_node_from_graph`
- [ ] Implement `set_node_property`
- [ ] Test node creation workflow

### Sprint 3: Graph Analysis (Day 5)
- [ ] Implement `analyze_graph_complexity`
- [ ] Implement graph traversal (BFS for depth calculation)
- [ ] Implement complexity scoring
- [ ] Implement `find_conversion_candidates`
- [ ] Implement `detect_graph_patterns`
- [ ] Test analysis on existing blueprints

### Sprint 4: Integration & Polish (Day 6)
- [ ] Update AGENTS.md with new capabilities
- [ ] Update CURRENT_TASK.md with MCP enhancements
- [ ] Create example workflows using new tools
- [ ] Performance testing with large blueprints
- [ ] Error handling improvements
- [ ] Documentation

---

## Code References

### From chongdashu/unreal-mcp

**Blueprint Node Creation Pattern:**
```cpp
// They use a generic node spawning pattern
UK2Node* SpawnNode(UEdGraph* Graph, TSubclassOf<UK2Node> NodeClass)
{
    UK2Node* NewNode = NewObject<UK2Node>(Graph, NodeClass);
    Graph->AddNode(NewNode);
    NewNode->AllocateDefaultPins();
    return NewNode;
}
```

### From flopperam/unreal-engine-mcp

**Pin Connection Validation:**
```typescript
// TypeScript example showing validation approach
function validatePinConnection(sourcePin, targetPin) {
  // Check direction (output -> input)
  if (sourcePin.direction !== 'output' || targetPin.direction !== 'input') {
    return false;
  }

  // Check type compatibility
  return arePinTypesCompatible(sourcePin.type, targetPin.type);
}
```

**Node Position Management:**
```typescript
// Auto-layout nodes in a grid
function positionNodes(nodes, startX = 0, startY = 0, spacingX = 300, spacingY = 100) {
  nodes.forEach((node, index) => {
    node.posX = startX + (index * spacingX);
    node.posY = startY;
  });
}
```

---

## Dependencies & Module Setup

### Required Engine Modules

Add to `ClaudeUnrealMCP.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "UnrealEd",           // Editor utilities
    "BlueprintGraph",     // Blueprint graph classes
    "Kismet",            // Blueprint compilation
    "KismetCompiler",    // Blueprint compiler
    "GraphEditor",       // Graph editing
    "EditorSubsystem"    // Editor subsystems
});
```

### Header Includes

Add to `MCPServer.cpp`:

```cpp
#include "BlueprintEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_ForEachLoop.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
```

---

## Risk Mitigation

### Risk 1: Blueprint Corruption
**Mitigation:**
- Always validate graph structure before saving
- Test on copy of blueprint first
- Implement rollback mechanism
- Auto-save backup before modifications

### Risk 2: Pin Connection Failures
**Mitigation:**
- Strict pin type validation
- Clear error messages
- Provide pin compatibility checking tool
- Document common pin types

### Risk 3: Performance Issues with Large Graphs
**Mitigation:**
- Implement pagination for node listing
- Cache graph analysis results
- Optimize traversal algorithms
- Add timeout limits

### Risk 4: UE Version Compatibility
**Mitigation:**
- Test on UE 5.5, 5.6, 5.7
- Use version-agnostic APIs where possible
- Document version-specific features
- Graceful fallbacks for missing APIs

---

## Success Criteria

### Phase 1 Success:
- ✅ Can create blueprint function with input/output parameters
- ✅ Functions appear correctly in blueprint editor
- ✅ Functions compile without errors
- ✅ Can rename functions programmatically

### Phase 2 Success:
- ✅ Can add Event nodes (BeginPlay, Tick)
- ✅ Can add Function call nodes
- ✅ Can connect nodes via pins
- ✅ Connections are valid and compile
- ✅ Can delete nodes cleanly

### Phase 3 Success:
- ✅ Complexity analysis matches manual assessment
- ✅ Can identify simple vs complex functions
- ✅ Pattern detection finds known patterns
- ✅ Analysis completes in <5 seconds for typical blueprints

---

## Next Steps

1. Review this plan with project stakeholders
2. Set up development branch for MCP enhancements
3. Begin Sprint 1: Function Creation
4. Create test blueprints for validation
5. Document new MCP commands in README

---

**Last Updated:** 2026-02-01
**Next Review:** After Sprint 1 completion
