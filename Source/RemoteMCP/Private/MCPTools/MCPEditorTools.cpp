// Fill out your copyright notice in the Description page of Project Settings.


#include "MCPTools/MCPEditorTools.h"
#include "MCPTools/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "EditorLevelLibrary.h"
#include "FileHelpers.h"
#include "JsonObjectConverter.h"
#include "Misc/PackageName.h"
#include "Containers/Ticker.h"
#include "MCPSubsystem.h"

namespace
{
    FString NormalizeMapAssetPath(const FString& InPath)
    {
        FString Path = InPath;
        Path.TrimStartAndEndInline();

        if (Path.EndsWith(TEXT(".umap")))
        {
            Path.LeftChopInline(5, EAllowShrinking::No);
        }

        if (!Path.StartsWith(TEXT("/")))
        {
            Path = FString::Printf(TEXT("/Game/%s"), *Path);
        }

        return Path;
    }

    FJsonObjectParameter CreateMapLifecycleSuccess(
        const FString& Action,
        const FString& MapPath,
        bool bLoaded = false,
        bool bSessionDisrupted = false)
    {
        FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetStringField(TEXT("action"), Action);
        ResultObj->SetStringField(TEXT("map_path"), MapPath);
        ResultObj->SetBoolField(TEXT("loaded"), bLoaded);
        ResultObj->SetBoolField(TEXT("session_disrupted"), bSessionDisrupted);
        ResultObj->SetBoolField(TEXT("reconnect_required"), bSessionDisrupted);
        ResultObj->SetStringField(
            TEXT("risk_tier"),
            bSessionDisrupted ? TEXT("session-disrupting") : TEXT("safe"));
        if (bSessionDisrupted)
        {
            ResultObj->SetStringField(
                TEXT("restart_hint"),
                TEXT("Map lifecycle operations may restart or invalidate the current MCP session. Reconnect before issuing follow-up calls."));
        }
        return ResultObj;
    }

    UWorld* GetEditorWorldChecked()
    {
        return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }

    FLevelEditorViewportClient* GetPreferredLevelViewportClient()
    {
        if (!GEditor)
        {
            return nullptr;
        }

        if (FViewport* ActiveViewport = GEditor->GetActiveViewport())
        {
            for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
            {
                if (ViewportClient && ViewportClient->Viewport == ActiveViewport && ViewportClient->ViewportType == LVT_Perspective)
                {
                    return ViewportClient;
                }
            }
        }

        for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
        {
            if (ViewportClient && ViewportClient->Viewport && ViewportClient->ViewportType == LVT_Perspective)
            {
                return ViewportClient;
            }
        }

        return nullptr;
    }

    void ForceLevelViewportRefresh()
    {
        if (!GEditor)
        {
            return;
        }

        GEditor->RedrawLevelEditingViewports(true);

        if (FLevelEditorViewportClient* ViewportClient = GetPreferredLevelViewportClient())
        {
            ViewportClient->Invalidate();
            if (ViewportClient->Viewport)
            {
                ViewportClient->Viewport->Draw();
            }
        }
    }

    void ScheduleSessionDisruptingMapAction(TFunction<void()> Action)
    {
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([Action = MoveTemp(Action)](float)
            {
                if (UMCPSubsystem* Subsystem = UMCPSubsystem::Get())
                {
                    Subsystem->ClearContextForSessionTransition();
                    Subsystem->ScheduleRestartAfterTransition();
                }

                Action();
                return false;
            }),
            0.0f);
    }

    void GetAllEditorActors(TArray<AActor*>& OutActors)
    {
        OutActors.Reset();
        UWorld* World = GetEditorWorldChecked();
        if (!World)
        {
            return;
        }
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), OutActors);
    }

    AActor* FindActorByExactName(const FString& ActorName)
    {
        TArray<AActor*> AllActors;
        GetAllEditorActors(AllActors);
        for (AActor* Actor : AllActors)
        {
            if (!Actor)
            {
                continue;
            }

            if (Actor->GetName() == ActorName)
            {
                return Actor;
            }

#if WITH_EDITOR
            if (Actor->GetActorLabel() == ActorName)
            {
                return Actor;
            }
#endif
        }
        return nullptr;
    }

    bool ActorMatchesPrefix(AActor* Actor, const FString& Prefix)
    {
        if (!Actor)
        {
            return false;
        }

        if (Actor->GetName().StartsWith(Prefix))
        {
            return true;
        }

#if WITH_EDITOR
        if (Actor->GetActorLabel().StartsWith(Prefix))
        {
            return true;
        }
#endif

        return false;
    }

    template<typename TActor>
    TActor* FindFirstActorOfClass(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }

        for (TActorIterator<TActor> It(World); It; ++It)
        {
            if (*It)
            {
                return *It;
            }
        }
        return nullptr;
    }

    FLinearColor ParseLinearColorFromJson(const FJsonObjectParameter& Params, const FString& FieldName, const FLinearColor& DefaultValue)
    {
        if (!Params->HasField(FieldName))
        {
            return DefaultValue;
        }

        TSharedPtr<FJsonValue> ColorValue = Params->Values.FindRef(FieldName);
        if (!ColorValue.IsValid() || ColorValue->Type != EJson::Array)
        {
            return DefaultValue;
        }

        const TArray<TSharedPtr<FJsonValue>>& Values = ColorValue->AsArray();
        if (Values.Num() < 3)
        {
            return DefaultValue;
        }

        return FLinearColor(
            Values[0]->AsNumber(),
            Values[1]->AsNumber(),
            Values[2]->AsNumber(),
            Values.Num() > 3 ? Values[3]->AsNumber() : 1.0f);
    }

    TSharedPtr<FJsonObject> MakeActorSummary(AActor* Actor)
    {
        if (!Actor)
        {
            return nullptr;
        }

        return FUnrealMCPCommonUtils::ActorToJsonObject(Actor, true);
    }
}


/**
 * 获取当前关卡中的所有Actor
 * 此函数不需要任何输入参数，将返回场景中所有Actor的信息
 */
FJsonObjectParameter UMCPEditorTools::HandleGetActorsInLevel(const FJsonObjectParameter& Params)
{
    // 获取所有Actor
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    // 将Actor信息转换为JSON数组
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }

    // 构建结果JSON对象
    FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);

    return ResultObj;
}

/**
 * 根据名称模式查找Actor
 * 使用字符串包含匹配方式查找名称包含指定模式的所有Actor
 */
FJsonObjectParameter UMCPEditorTools::HandleFindActorsByName(const FJsonObjectParameter& Params)
{
    // 获取要查找的模式
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }

    // 获取所有Actor
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    // 查找名称包含模式的Actor
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }

    // 构建结果JSON对象
    FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);

    return ResultObj;
}

/**
 * 在关卡中生成新的Actor
 * 根据指定的类型、名称和变换信息创建新的Actor
 */
FJsonObjectParameter UMCPEditorTools::HandleSpawnActor(const FJsonObjectParameter& Params)
{
    // 获取必要的Actor类型参数
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // 获取Actor名称(必需参数)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // 获取可选的变换参数
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // 获取编辑器世界
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // 检查是否已存在同名Actor
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    // 设置生成参数
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    // 根据类型创建Actor
    AActor* NewActor = nullptr;
    if (ActorType == TEXT("StaticMeshActor"))
    {
        NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    // 如果Actor创建成功，设置缩放并返回Actor信息
    if (NewActor)
    {
        // 设置缩放(因为SpawnActor只接受位置和旋转)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // 返回创建的Actor详细信息
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

/**
 * 删除指定名称的Actor
 * 查找并删除名称完全匹配的Actor
 */
FJsonObjectParameter UMCPEditorTools::HandleDeleteActor(const FJsonObjectParameter& Params)
{
    // 获取要删除的Actor名称
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // 获取所有Actor
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    // 查找并删除指定Actor
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // 在删除前保存Actor信息用于响应
            FJsonObjectParameter ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);

            // 删除Actor
            Actor->Destroy();

            // 构建结果JSON对象
            FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

/**
 * 设置Actor的变换(位置、旋转、缩放)
 * 允许单独设置位置、旋转或缩放属性
 */
FJsonObjectParameter UMCPEditorTools::HandleSetActorTransform(const FJsonObjectParameter& Params)
{
    // 获取Actor名称
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // 查找目标Actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // 获取变换参数并保持现有值(如果未指定)
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // 设置新的变换
    TargetActor->SetActorTransform(NewTransform);
    ForceLevelViewportRefresh();

    // 返回更新后的Actor信息
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

/**
 * 获取Actor的所有属性
 * 返回Actor的详细属性信息
 */
FJsonObjectParameter UMCPEditorTools::HandleGetActorProperties(const FJsonObjectParameter& Params)
{
    // 获取Actor名称
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // 查找目标Actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // 返回Actor的详细属性
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

/**
 * 设置Actor的特定属性
 * 使用反射系统设置属性值
 */
FJsonObjectParameter UMCPEditorTools::HandleSetActorProperty(const FJsonObjectParameter& Params)
{
    // 获取Actor名称
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // 查找目标Actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // 获取属性名称
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // 获取属性值
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }

    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));

    // 使用工具函数设置属性
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // 属性设置成功
        FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);

        // 同时包含完整的Actor详细信息
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

/**
 * 在关卡中生成蓝图Actor
 * 根据指定的蓝图路径和名称创建Actor实例
 */
FJsonObjectParameter UMCPEditorTools::HandleSpawnBlueprintActor(const FJsonObjectParameter& Params)
{
    // 获取必要参数
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // 查找蓝图资源
    FString AssetPath = TEXT("/Game/Blueprints/") + BlueprintName;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // 获取变换参数
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // 生成Actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // 设置生成变换
    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    // 设置生成参数
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    // 通过蓝图类生成Actor
    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

/**
 * 设置编辑器视口的焦点
 * 可以聚焦到指定Actor或坐标位置
 */
FJsonObjectParameter UMCPEditorTools::HandleFocusViewport(const FJsonObjectParameter& Params)
{
    // 获取目标Actor名称(如果提供)
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // 获取位置(如果提供)
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // 获取与目标的距离
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // 获取朝向(如果提供)
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // 获取活动视口
    FLevelEditorViewportClient* ViewportClient = GetPreferredLevelViewportClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    ViewportClient->SetViewportType(LVT_Perspective);
    ViewportClient->SetViewMode(VMI_Lit);
    ViewportClient->SetRealtime(true);

    // 如果有目标Actor，聚焦到它
    if (HasTargetActor)
    {
        // 查找Actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // 聚焦到Actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // 否则使用提供的位置
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // 如果提供了朝向，设置视口朝向
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // 强制视口重绘
    ForceLevelViewportRefresh();

    // 返回成功结果
    FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

/**
 * 捕获编辑器视口的屏幕截图
 * 保存当前视口图像到指定路径
 */
FJsonObjectParameter UMCPEditorTools::HandleTakeScreenshot(const FJsonObjectParameter& Params)
{
    // 获取文件路径参数
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }

    // 确保文件路径有正确的扩展名
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    FString CameraName;
    Params->TryGetStringField(TEXT("camera_name"), CameraName);

    // 获取活动视口
    if (FLevelEditorViewportClient* ViewportClient = GetPreferredLevelViewportClient())
    {
        if (!CameraName.IsEmpty())
        {
            if (ACameraActor* CameraActor = Cast<ACameraActor>(FindActorByExactName(CameraName)))
            {
                ViewportClient->SetViewportType(LVT_Perspective);
                ViewportClient->SetViewMode(VMI_Lit);
                ViewportClient->SetRealtime(true);
                ViewportClient->SetActorLock(CameraActor);
                ViewportClient->SetViewLocation(CameraActor->GetActorLocation());
                ViewportClient->SetViewRotation(CameraActor->GetActorRotation());
                ForceLevelViewportRefresh();
            }
            else
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Capture camera not found: %s"), *CameraName));
            }
        }

        FViewport* Viewport = ViewportClient->Viewport;
        if (!Viewport)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Preferred viewport is invalid"));
        }
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);

        // 读取视口像素
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            // 压缩为PNG格式
            TArray64<uint8> CompressedBitmap;
            FImageUtils::PNGCompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);

            // 保存到文件
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
}

FJsonObjectParameter UMCPEditorTools::HandleLoadMap(const FJsonObjectParameter& Params)
{
    FString MapPath;
    if (!Params->TryGetStringField(TEXT("map_path"), MapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'map_path' parameter"));
    }

    const FString NormalizedMapPath = NormalizeMapAssetPath(MapPath);
    if (!UEditorAssetLibrary::DoesAssetExist(NormalizedMapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Map asset not found: %s"), *NormalizedMapPath));
    }

    if (UWorld* World = GetEditorWorldChecked())
    {
        const FString CurrentMapPath = World->GetPackage() ? World->GetPackage()->GetName() : TEXT("");
        if (CurrentMapPath == NormalizedMapPath)
        {
            return CreateMapLifecycleSuccess(TEXT("load_map"), NormalizedMapPath, true, false);
        }
    }

    ScheduleSessionDisruptingMapAction([NormalizedMapPath]()
    {
        UEditorLevelLibrary::LoadLevel(NormalizedMapPath);
    });

    return CreateMapLifecycleSuccess(TEXT("load_map"), NormalizedMapPath, true, true);
}

FJsonObjectParameter UMCPEditorTools::HandleSaveCurrentMap(const FJsonObjectParameter& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    const FString CurrentMapPath = World->GetPackage() ? World->GetPackage()->GetName() : TEXT("");
    if (CurrentMapPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Current map path is empty"));
    }

    if (!UEditorLevelLibrary::SaveCurrentLevel())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to save current map: %s"), *CurrentMapPath));
    }

    return CreateMapLifecycleSuccess(TEXT("save_current_map"), CurrentMapPath, false);
}

FJsonObjectParameter UMCPEditorTools::HandleSaveMapAs(const FJsonObjectParameter& Params)
{
    FString TargetMapPath;
    if (!Params->TryGetStringField(TEXT("target_map_path"), TargetMapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_map_path' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    const FString CurrentMapPath = World->GetPackage() ? World->GetPackage()->GetName() : TEXT("");
    if (CurrentMapPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Current map path is empty"));
    }

    const FString NormalizedTargetMapPath = NormalizeMapAssetPath(TargetMapPath);
    if (CurrentMapPath == NormalizedTargetMapPath)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Target map path must differ from current map"));
    }

    if (!UEditorLevelLibrary::SaveCurrentLevel())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to save current map before save-as: %s"), *CurrentMapPath));
    }

    if (UEditorAssetLibrary::DoesAssetExist(NormalizedTargetMapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Target map already exists: %s"), *NormalizedTargetMapPath));
    }

    const FString TargetFilename = FPackageName::LongPackageNameToFilename(
        NormalizedTargetMapPath,
        FPackageName::GetMapPackageExtension());

    if (!FEditorFileUtils::SaveMap(World, TargetFilename))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to save map as: %s"), *NormalizedTargetMapPath));
    }

    const FString CurrentMapAfterSave = World->GetPackage() ? World->GetPackage()->GetName() : NormalizedTargetMapPath;
    return CreateMapLifecycleSuccess(TEXT("save_map_as"), CurrentMapAfterSave, CurrentMapAfterSave == NormalizedTargetMapPath, false);
}

FJsonObjectParameter UMCPEditorTools::HandleCreateBlankMap(const FJsonObjectParameter& Params)
{
    FString MapPath;
    if (!Params->TryGetStringField(TEXT("map_path"), MapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'map_path' parameter"));
    }

    const FString NormalizedMapPath = NormalizeMapAssetPath(MapPath);
    if (UEditorAssetLibrary::DoesAssetExist(NormalizedMapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Map already exists: %s"), *NormalizedMapPath));
    }

    ScheduleSessionDisruptingMapAction([NormalizedMapPath]()
    {
        UEditorLevelLibrary::NewLevel(NormalizedMapPath);
    });

    return CreateMapLifecycleSuccess(TEXT("create_blank_map"), NormalizedMapPath, true, true);
}

FJsonObjectParameter UMCPEditorTools::HandleCreateMapFromTemplate(const FJsonObjectParameter& Params)
{
    FString MapPath;
    FString TemplateMapPath;
    if (!Params->TryGetStringField(TEXT("map_path"), MapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'map_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("template_map_path"), TemplateMapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'template_map_path' parameter"));
    }

    const FString NormalizedMapPath = NormalizeMapAssetPath(MapPath);
    const FString NormalizedTemplateMapPath = NormalizeMapAssetPath(TemplateMapPath);

    if (UEditorAssetLibrary::DoesAssetExist(NormalizedMapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Map already exists: %s"), *NormalizedMapPath));
    }

    if (!UEditorAssetLibrary::DoesAssetExist(NormalizedTemplateMapPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Template map not found: %s"), *NormalizedTemplateMapPath));
    }

    ScheduleSessionDisruptingMapAction([NormalizedMapPath, NormalizedTemplateMapPath]()
    {
        UEditorLevelLibrary::NewLevelFromTemplate(NormalizedMapPath, NormalizedTemplateMapPath);
    });

    return CreateMapLifecycleSuccess(TEXT("create_map_from_template"), NormalizedMapPath, true, true);
}

FJsonObjectParameter UMCPEditorTools::HandleSpawnStaticMeshActor(const FJsonObjectParameter& Params)
{
    FString ActorName;
    FString MeshPath;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }

    UWorld* World = GetEditorWorldChecked();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    if (FindActorByExactName(ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
    }

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!StaticMesh)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn static mesh actor"));
    }

    if (UStaticMeshComponent* MeshComponent = NewActor->GetStaticMeshComponent())
    {
        MeshComponent->SetStaticMesh(StaticMesh);
    }

    FTransform Transform = NewActor->GetTransform();
    Transform.SetScale3D(Scale);
    NewActor->SetActorTransform(Transform);

#if WITH_EDITOR
    NewActor->SetActorLabel(ActorName);
#endif

    ForceLevelViewportRefresh();

    return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
}

FJsonObjectParameter UMCPEditorTools::HandleFindActorsByPrefix(const FJsonObjectParameter& Params)
{
    FString Prefix;
    if (!Params->TryGetStringField(TEXT("prefix"), Prefix))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'prefix' parameter"));
    }

    TArray<AActor*> AllActors;
    GetAllEditorActors(AllActors);

    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (ActorMatchesPrefix(Actor, Prefix))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }

    FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("prefix"), Prefix);
    ResultObj->SetNumberField(TEXT("count"), MatchingActors.Num());
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::HandleDeleteActorsByPrefix(const FJsonObjectParameter& Params)
{
    FString Prefix;
    if (!Params->TryGetStringField(TEXT("prefix"), Prefix))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'prefix' parameter"));
    }

    TArray<AActor*> AllActors;
    GetAllEditorActors(AllActors);

    TArray<TSharedPtr<FJsonValue>> DeletedActors;
    for (AActor* Actor : AllActors)
    {
        if (!ActorMatchesPrefix(Actor, Prefix))
        {
            continue;
        }

        DeletedActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        Actor->Destroy();
    }

    FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("prefix"), Prefix);
    ResultObj->SetNumberField(TEXT("deleted_count"), DeletedActors.Num());
    ResultObj->SetArrayField(TEXT("deleted_actors"), DeletedActors);
    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::HandleResetTestbed(const FJsonObjectParameter& Params)
{
    return HandleDeleteActorsByPrefix(Params);
}

FJsonObjectParameter UMCPEditorTools::HandleEnsureCaptureCamera(const FJsonObjectParameter& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FVector Location(0.0f, 0.0f, 300.0f);
    FRotator Rotation(-20.0f, 180.0f, 0.0f);
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }

    UWorld* World = GetEditorWorldChecked();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    AActor* ExistingActor = FindActorByExactName(ActorName);
    ACameraActor* CameraActor = Cast<ACameraActor>(ExistingActor);
    if (!CameraActor && ExistingActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor exists but is not a camera: %s"), *ActorName));
    }

    if (!CameraActor)
    {
        FActorSpawnParameters SpawnParams;
        CameraActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
        if (!CameraActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create capture camera"));
        }
    }

    CameraActor->SetActorLocation(Location);
    CameraActor->SetActorRotation(Rotation);

#if WITH_EDITOR
    CameraActor->SetActorLabel(ActorName);
#endif

    ForceLevelViewportRefresh();

    FJsonObjectParameter ResultObj = FUnrealMCPCommonUtils::ActorToJsonObject(CameraActor, true);
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("action"), TEXT("ensure_capture_camera"));
    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::HandleFindLightingRig(const FJsonObjectParameter& Params)
{
    UWorld* World = GetEditorWorldChecked();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    ADirectionalLight* DirectionalLight = FindFirstActorOfClass<ADirectionalLight>(World);
    ASkyLight* SkyLight = FindFirstActorOfClass<ASkyLight>(World);
    AExponentialHeightFog* HeightFog = FindFirstActorOfClass<AExponentialHeightFog>(World);
    ASkyAtmosphere* SkyAtmosphere = FindFirstActorOfClass<ASkyAtmosphere>(World);

    FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("action"), TEXT("find_lighting_rig"));
    ResultObj->SetBoolField(TEXT("has_directional_light"), DirectionalLight != nullptr);
    ResultObj->SetBoolField(TEXT("has_sky_light"), SkyLight != nullptr);
    ResultObj->SetBoolField(TEXT("has_exponential_height_fog"), HeightFog != nullptr);
    ResultObj->SetBoolField(TEXT("has_sky_atmosphere"), SkyAtmosphere != nullptr);

    if (DirectionalLight)
    {
        ResultObj->SetObjectField(TEXT("directional_light"), MakeActorSummary(DirectionalLight));
    }
    if (SkyLight)
    {
        ResultObj->SetObjectField(TEXT("sky_light"), MakeActorSummary(SkyLight));
    }
    if (HeightFog)
    {
        ResultObj->SetObjectField(TEXT("exponential_height_fog"), MakeActorSummary(HeightFog));
    }
    if (SkyAtmosphere)
    {
        ResultObj->SetObjectField(TEXT("sky_atmosphere"), MakeActorSummary(SkyAtmosphere));
    }

    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::HandleEnsureBasicLightingRig(const FJsonObjectParameter& Params)
{
    UWorld* World = GetEditorWorldChecked();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FString Prefix = TEXT("GymLight");
    Params->TryGetStringField(TEXT("prefix"), Prefix);

    FString DirectionalLightName = Prefix + TEXT("_DirectionalLight");
    FString SkyLightName = Prefix + TEXT("_SkyLight");
    FString FogName = Prefix + TEXT("_ExponentialHeightFog");
    FString SkyAtmosphereName = Prefix + TEXT("_SkyAtmosphere");
    Params->TryGetStringField(TEXT("directional_light_name"), DirectionalLightName);
    Params->TryGetStringField(TEXT("sky_light_name"), SkyLightName);
    Params->TryGetStringField(TEXT("fog_name"), FogName);
    Params->TryGetStringField(TEXT("sky_atmosphere_name"), SkyAtmosphereName);

    ADirectionalLight* DirectionalLight = Cast<ADirectionalLight>(FindActorByExactName(DirectionalLightName));
    ASkyLight* SkyLight = Cast<ASkyLight>(FindActorByExactName(SkyLightName));
    AExponentialHeightFog* HeightFog = Cast<AExponentialHeightFog>(FindActorByExactName(FogName));
    ASkyAtmosphere* SkyAtmosphere = Cast<ASkyAtmosphere>(FindActorByExactName(SkyAtmosphereName));

    if (!DirectionalLight)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *DirectionalLightName;
        DirectionalLight = World->SpawnActor<ADirectionalLight>(
            ADirectionalLight::StaticClass(),
            FVector(-500.0f, -500.0f, 800.0f),
            FRotator(-35.0f, 35.0f, 0.0f),
            SpawnParams);
        if (!DirectionalLight)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create directional light"));
        }
#if WITH_EDITOR
        DirectionalLight->SetActorLabel(DirectionalLightName);
#endif
    }

    if (UDirectionalLightComponent* DirectionalLightComponent = Cast<UDirectionalLightComponent>(DirectionalLight->GetLightComponent()))
    {
        DirectionalLightComponent->bAtmosphereSunLight = true;
        DirectionalLightComponent->AtmosphereSunLightIndex = 0;
    }

    if (!SkyLight)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *SkyLightName;
        SkyLight = World->SpawnActor<ASkyLight>(
            ASkyLight::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams);
        if (!SkyLight)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create sky light"));
        }
#if WITH_EDITOR
        SkyLight->SetActorLabel(SkyLightName);
#endif
    }

    if (!HeightFog)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *FogName;
        HeightFog = World->SpawnActor<AExponentialHeightFog>(
            AExponentialHeightFog::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams);
        if (!HeightFog)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create exponential height fog"));
        }
#if WITH_EDITOR
        HeightFog->SetActorLabel(FogName);
#endif
    }

    if (!SkyAtmosphere)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *SkyAtmosphereName;
        SkyAtmosphere = World->SpawnActor<ASkyAtmosphere>(
            ASkyAtmosphere::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams);
        if (!SkyAtmosphere)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create sky atmosphere"));
        }
#if WITH_EDITOR
        SkyAtmosphere->SetActorLabel(SkyAtmosphereName);
#endif
    }

    FJsonObjectParameter ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("action"), TEXT("ensure_basic_lighting_rig"));
    ResultObj->SetObjectField(TEXT("directional_light"), MakeActorSummary(DirectionalLight));
    ResultObj->SetObjectField(TEXT("sky_light"), MakeActorSummary(SkyLight));
    ResultObj->SetObjectField(TEXT("exponential_height_fog"), MakeActorSummary(HeightFog));
    ResultObj->SetObjectField(TEXT("sky_atmosphere"), MakeActorSummary(SkyAtmosphere));
    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::HandleSetDirectionalLight(const FJsonObjectParameter& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    ADirectionalLight* LightActor = Cast<ADirectionalLight>(FindActorByExactName(ActorName));
    if (!LightActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Directional light not found: %s"), *ActorName));
    }

    UDirectionalLightComponent* LightComponent = Cast<UDirectionalLightComponent>(LightActor->GetLightComponent());
    if (!LightComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to access directional light component"));
    }

    if (Params->HasField(TEXT("intensity")))
    {
        LightComponent->SetIntensity(Params->GetNumberField(TEXT("intensity")));
    }
    if (Params->HasField(TEXT("light_color")))
    {
        LightComponent->SetLightColor(ParseLinearColorFromJson(Params, TEXT("light_color"), FLinearColor::White));
    }
    if (Params->HasField(TEXT("temperature")))
    {
        LightComponent->bUseTemperature = true;
        LightComponent->Temperature = Params->GetNumberField(TEXT("temperature"));
    }
    if (Params->HasField(TEXT("indirect_intensity")))
    {
        LightComponent->IndirectLightingIntensity = Params->GetNumberField(TEXT("indirect_intensity"));
    }
    if (Params->HasField(TEXT("source_angle")))
    {
        LightComponent->LightSourceAngle = Params->GetNumberField(TEXT("source_angle"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        LightActor->SetActorRotation(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
    }

    LightComponent->MarkRenderStateDirty();
    ForceLevelViewportRefresh();

    FJsonObjectParameter ResultObj = FUnrealMCPCommonUtils::ActorToJsonObject(LightActor, true);
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("action"), TEXT("set_directional_light"));
    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::HandleSetSkyLight(const FJsonObjectParameter& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    ASkyLight* LightActor = Cast<ASkyLight>(FindActorByExactName(ActorName));
    if (!LightActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Sky light not found: %s"), *ActorName));
    }

    USkyLightComponent* LightComponent = LightActor->GetLightComponent();
    if (!LightComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to access sky light component"));
    }

    if (Params->HasField(TEXT("intensity")))
    {
        LightComponent->SetIntensity(Params->GetNumberField(TEXT("intensity")));
    }
    if (Params->HasField(TEXT("light_color")))
    {
        LightComponent->SetLightColor(ParseLinearColorFromJson(Params, TEXT("light_color"), FLinearColor::White));
    }

    LightComponent->RecaptureSky();
    LightComponent->MarkRenderStateDirty();
    ForceLevelViewportRefresh();

    FJsonObjectParameter ResultObj = FUnrealMCPCommonUtils::ActorToJsonObject(LightActor, true);
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("action"), TEXT("set_sky_light"));
    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::HandleSetExponentialHeightFog(const FJsonObjectParameter& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    AExponentialHeightFog* FogActor = Cast<AExponentialHeightFog>(FindActorByExactName(ActorName));
    if (!FogActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Exponential height fog not found: %s"), *ActorName));
    }

    UExponentialHeightFogComponent* FogComponent = FogActor->FindComponentByClass<UExponentialHeightFogComponent>();
    if (!FogComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to access exponential height fog component"));
    }

    if (Params->HasField(TEXT("fog_density")))
    {
        FogComponent->FogDensity = Params->GetNumberField(TEXT("fog_density"));
    }
    if (Params->HasField(TEXT("fog_height_falloff")))
    {
        FogComponent->FogHeightFalloff = Params->GetNumberField(TEXT("fog_height_falloff"));
    }
    if (Params->HasField(TEXT("start_distance")))
    {
        FogComponent->StartDistance = Params->GetNumberField(TEXT("start_distance"));
    }
    if (Params->HasField(TEXT("fog_inscattering_color")))
    {
        FogComponent->SetFogInscatteringColor(ParseLinearColorFromJson(
            Params,
            TEXT("fog_inscattering_color"),
            FogComponent->FogInscatteringLuminance));
    }

    FogComponent->MarkRenderStateDirty();
    ForceLevelViewportRefresh();

    FJsonObjectParameter ResultObj = FUnrealMCPCommonUtils::ActorToJsonObject(FogActor, true);
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("action"), TEXT("set_exponential_height_fog"));
    return ResultObj;
}

FJsonObjectParameter UMCPEditorTools::ConvertObjectToJson(UObject* TargetObject)
{
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	FJsonObjectConverter::UStructToJsonObject(TargetObject->GetClass(),TargetObject,ResultObj.ToSharedRef());
	return {ResultObj};
}


