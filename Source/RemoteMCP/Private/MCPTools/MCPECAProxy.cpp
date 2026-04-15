// RemoteMCP — ECABridge Proxy Implementation
//
// Conditional compilation:
//   WITH_ECA_BRIDGE=1  →  delegates to FECACommandRegistry (linked against ECABridge module)
//   WITH_ECA_BRIDGE=0  →  every method returns a structured "not available" response
//
// Thread safety:
//   FECACommandRegistry uses FCriticalSection internally (ECACommand.cpp L315, L342, etc.)
//   All UFUNCTION calls here happen on the game thread (enforced by RemoteMCP's
//   SafeCallCPPFunction and the domain_tool game_thread=True default).

#include "MCPTools/MCPECAProxy.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#if WITH_ECA_BRIDGE
#include "Commands/ECACommand.h"
#endif

// ============================================================================
// Helpers
// ============================================================================

namespace MCPECAProxyInternal
{
	/** Serialize a TSharedPtr<FJsonObject> to compact JSON string. */
	static FString SerializeJson(const TSharedPtr<FJsonObject>& Obj)
	{
		if (!Obj.IsValid())
		{
			return TEXT("{}");
		}
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
		return Out;
	}

#if WITH_ECA_BRIDGE
	/** Convert FECACommandParam array to JSON array for ListECACommands. */
	static TArray<TSharedPtr<FJsonValue>> ParamsToJsonArray(const TArray<FECACommandParam>& Params)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FECACommandParam& P : Params)
		{
			TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
			ParamObj->SetStringField(TEXT("name"), P.Name);
			ParamObj->SetStringField(TEXT("type"), P.Type);
			ParamObj->SetStringField(TEXT("description"), P.Description);
			ParamObj->SetBoolField(TEXT("required"), P.bRequired);
			if (!P.DefaultValue.IsEmpty())
			{
				ParamObj->SetStringField(TEXT("default"), P.DefaultValue);
			}
			Arr.Add(MakeShared<FJsonValueObject>(ParamObj));
		}
		return Arr;
	}
#endif
}

// ============================================================================
// Stub helpers (used by both WITH_ECA_BRIDGE=0 stubs and error paths)
// ============================================================================

FJsonObjectParameter UMCPECAProxy::MakeUnavailableResponse(const FString& Context)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), false);
	Obj->SetStringField(TEXT("error"),
		FString::Printf(TEXT("ECABridge is not available (%s). "
			"This engine build does not include the ECABridge plugin (WITH_ECA_BRIDGE=0)."), *Context));
	return FJsonObjectParameter(Obj);
}

FString UMCPECAProxy::MakeUnavailableJsonString(const FString& Context)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), false);
	Obj->SetStringField(TEXT("error"),
		FString::Printf(TEXT("ECABridge is not available (%s). "
			"This engine build does not include the ECABridge plugin (WITH_ECA_BRIDGE=0)."), *Context));
	return MCPECAProxyInternal::SerializeJson(Obj);
}

// ============================================================================
// IsECAAvailable
// ============================================================================

bool UMCPECAProxy::IsECAAvailable()
{
#if WITH_ECA_BRIDGE
	// Check that the registry actually has commands registered.
	// This catches the edge case where the module is compiled in but
	// ECABridge plugin is disabled at runtime (EnabledByDefault=false).
	return FECACommandRegistry::Get().GetAllCommands().Num() > 0;
#else
	return false;
#endif
}

// ============================================================================
// ListECACommands
// ============================================================================

FString UMCPECAProxy::ListECACommands(const FString& CategoryFilter)
{
#if WITH_ECA_BRIDGE
	FECACommandRegistry& Registry = FECACommandRegistry::Get();

	TArray<TSharedPtr<IECACommand>> Commands;
	if (CategoryFilter.IsEmpty())
	{
		Commands = Registry.GetAllCommands();
	}
	else
	{
		Commands = Registry.GetCommandsByCategory(CategoryFilter);
	}

	TArray<TSharedPtr<FJsonValue>> CommandsArray;
	CommandsArray.Reserve(Commands.Num());

	for (const TSharedPtr<IECACommand>& Cmd : Commands)
	{
		if (!Cmd.IsValid())
		{
			continue;
		}

		TSharedPtr<FJsonObject> CmdObj = MakeShared<FJsonObject>();
		CmdObj->SetStringField(TEXT("name"), Cmd->GetName());
		CmdObj->SetStringField(TEXT("description"), Cmd->GetDescription());
		CmdObj->SetStringField(TEXT("category"), Cmd->GetCategory());
		CmdObj->SetArrayField(TEXT("parameters"),
			MCPECAProxyInternal::ParamsToJsonArray(Cmd->GetParameters()));

		CommandsArray.Add(MakeShared<FJsonValueObject>(CmdObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("commands"), CommandsArray);
	Result->SetNumberField(TEXT("count"), CommandsArray.Num());
	if (!CategoryFilter.IsEmpty())
	{
		Result->SetStringField(TEXT("category_filter"), CategoryFilter);
	}

	return MCPECAProxyInternal::SerializeJson(Result);
#else
	return MakeUnavailableJsonString(TEXT("ListECACommands"));
#endif
}

// ============================================================================
// ListECACategories
// ============================================================================

FString UMCPECAProxy::ListECACategories()
{
#if WITH_ECA_BRIDGE
	TArray<FString> Categories = FECACommandRegistry::Get().GetCategories();
	Categories.Sort();

	TArray<TSharedPtr<FJsonValue>> CatArray;
	CatArray.Reserve(Categories.Num());
	for (const FString& Cat : Categories)
	{
		CatArray.Add(MakeShared<FJsonValueString>(Cat));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("categories"), CatArray);
	Result->SetNumberField(TEXT("count"), CatArray.Num());

	return MCPECAProxyInternal::SerializeJson(Result);
#else
	return MakeUnavailableJsonString(TEXT("ListECACategories"));
#endif
}

// ============================================================================
// CallECACommand
// ============================================================================

FJsonObjectParameter UMCPECAProxy::CallECACommand(const FJsonObjectParameter& Params)
{
#if WITH_ECA_BRIDGE
	// ── Extract "command" name ──────────────────────────────────────
	FString CommandName;
	if (!Params->TryGetStringField(TEXT("command"), CommandName) || CommandName.IsEmpty())
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"),
			TEXT("Missing required field 'command' (string) in CallECACommand params."));
		return FJsonObjectParameter(Err);
	}

	// ── Check command exists ───────────────────────────────────────
	FECACommandRegistry& Registry = FECACommandRegistry::Get();
	if (!Registry.HasCommand(CommandName))
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"),
			FString::Printf(TEXT("Unknown ECA command: '%s'. Use ListECACommands to see available commands."), *CommandName));
		return FJsonObjectParameter(Err);
	}

	// ── Extract "arguments" (optional, defaults to empty object) ───
	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonObject>* ArgsPtr = nullptr;
	if (Params->TryGetObjectField(TEXT("arguments"), ArgsPtr) && ArgsPtr)
	{
		Arguments = *ArgsPtr;
	}
	else
	{
		Arguments = MakeShared<FJsonObject>();
	}

	// ── Execute ────────────────────────────────────────────────────
	FECACommandResult CmdResult = Registry.ExecuteCommand(CommandName, Arguments);

	// ── Build response ─────────────────────────────────────────────
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), CmdResult.bSuccess);
	Response->SetStringField(TEXT("command"), CommandName);

	if (CmdResult.bSuccess)
	{
		if (CmdResult.ResultData.IsValid())
		{
			Response->SetObjectField(TEXT("result"), CmdResult.ResultData);
		}
	}
	else
	{
		Response->SetStringField(TEXT("error"), CmdResult.ErrorMessage);
	}

	return FJsonObjectParameter(Response);
#else
	return MakeUnavailableResponse(TEXT("CallECACommand"));
#endif
}
