// RemoteMCP — ECABridge Proxy
// Provides a thin C++ bridge between RemoteMCP's Python layer and ECABridge's
// FECACommandRegistry. All methods are conditional on WITH_ECA_BRIDGE; when
// the macro is 0 (e.g. TestMCP project without ECABridge), every method
// compiles to a safe stub that returns "ECA not available".
//
// Design decisions:
//   - UBlueprintFunctionLibrary + static UFUNCTION: matches all existing
//     RemoteMCP tool classes (MCPEditorTools, MCPBlueprintTools, etc.)
//   - FJsonObjectParameter for CallECACommand: lets Python call via the
//     same call_cpp_tools() → SafeCallCPPFunction bridge
//   - FString returns for list/category queries: avoids FJsonObjectParameter
//     overhead for read-only metadata that Python json.loads() directly

#pragma once

#include "CoreMinimal.h"
#include "Structure/JsonParameter.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MCPECAProxy.generated.h"

/**
 * Proxy for ECABridge command execution from RemoteMCP.
 *
 * When WITH_ECA_BRIDGE=1, delegates to FECACommandRegistry singleton.
 * When WITH_ECA_BRIDGE=0, returns structured error responses so that
 * the Python eca_tools.py domain can report "ECA not available" cleanly.
 */
UCLASS()
class REMOTEMCP_API UMCPECAProxy : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── Availability ──────────────────────────────────────────────────

	/** Check whether ECABridge module is compiled in AND has registered commands. */
	UFUNCTION(BlueprintCallable, Category = "MCP|ECA")
	static bool IsECAAvailable();

	// ── Discovery ─────────────────────────────────────────────────────

	/**
	 * List all registered ECA commands, optionally filtered by category.
	 *
	 * @param CategoryFilter  If non-empty, only return commands in this category.
	 *                        Empty string returns all commands.
	 * @return JSON string:
	 *   {
	 *     "commands": [
	 *       {
	 *         "name": "create_actor",
	 *         "description": "Create a new actor...",
	 *         "category": "Actor",
	 *         "parameters": [ { "name": "actor_type", "type": "string", ... }, ... ]
	 *       }, ...
	 *     ],
	 *     "count": 238
	 *   }
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|ECA")
	static FString ListECACommands(const FString& CategoryFilter);

	/**
	 * List all ECA command category names.
	 *
	 * @return JSON string:
	 *   {
	 *     "categories": ["Actor", "Asset", "Blueprint", ...],
	 *     "count": 18
	 *   }
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|ECA")
	static FString ListECACategories();

	// ── Execution ─────────────────────────────────────────────────────

	/**
	 * Execute any ECA command by name.
	 *
	 * This is the primary entry point for Python's eca_call() tool.
	 * Uses FJsonObjectParameter to match the existing RemoteMCP C++ tool
	 * calling convention (call_cpp_tools → SafeCallCPPFunction).
	 *
	 * @param Params  JSON object with required fields:
	 *   - "command" (string): ECA command name, e.g. "get_actors_in_level"
	 *   - "arguments" (object, optional): command-specific parameters
	 * @return JSON object with ECA command result:
	 *   - On success: { "success": true, "result": { ... } }
	 *   - On error:   { "success": false, "error": "..." }
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|ECA")
	static FJsonObjectParameter CallECACommand(const FJsonObjectParameter& Params);

private:
	/** Build a JSON error response for when ECA is not available. */
	static FJsonObjectParameter MakeUnavailableResponse(const FString& Context);

	/** Build a JSON string error for list queries when ECA is not available. */
	static FString MakeUnavailableJsonString(const FString& Context);
};
