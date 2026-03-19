// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Structure/JsonParameter.h"
#include "UObject/Object.h"
#include "MCPEditorTools.generated.h"

/**
 * 提供编辑器工具函数的蓝图函数库
 * 这个类包含一系列用于远程控制Unreal编辑器的工具函数
 * 所有方法都使用JSON参数输入和输出，便于远程调用
 * Reference https://github.com/chongdashu/unreal-mcp
 */
UCLASS()
class REMOTEMCP_API UMCPEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 获取当前关卡中的所有Actor
	 * @param Params - 输入参数(不需要特定参数)
	 * @return 包含所有Actor信息的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleGetActorsInLevel(const FJsonObjectParameter& Params);
	
	/**
	 * 根据名称模式查找Actor
	 * @param Params - 输入参数，必须包含"pattern"字段
	 * @return 包含匹配Actor信息的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleFindActorsByName(const FJsonObjectParameter& Params);
	
	/**
	 * 在关卡中生成新的Actor
	 * @param Params - 输入参数，必须包含"type"和"name"字段，可选"location"、"rotation"和"scale"
	 * @return 新生成Actor的信息
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSpawnActor(const FJsonObjectParameter& Params);
	
	/**
	 * 删除指定名称的Actor
	 * @param Params - 输入参数，必须包含"name"字段
	 * @return 包含被删除Actor信息的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleDeleteActor(const FJsonObjectParameter& Params);
	
	/**
	 * 设置Actor的变换(位置、旋转、缩放)
	 * @param Params - 输入参数，必须包含"name"字段，可选"location"、"rotation"和"scale"
	 * @return 更新后Actor的信息
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSetActorTransform(const FJsonObjectParameter& Params);
	
	/**
	 * 获取Actor的所有属性
	 * @param Params - 输入参数，必须包含"name"字段
	 * @return 包含Actor详细属性的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleGetActorProperties(const FJsonObjectParameter& Params);
	
	/**
	 * 设置Actor的特定属性
	 * @param Params - 输入参数，必须包含"name"、"property_name"和"property_value"字段
	 * @return 操作结果和更新后的Actor信息
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSetActorProperty(const FJsonObjectParameter& Params);

	/**
	 * 在关卡中生成蓝图Actor
	 * @param Params - 输入参数，必须包含"blueprint_name"和"actor_name"字段，可选"location"、"rotation"和"scale"
	 * @return 新生成蓝图Actor的信息
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSpawnBlueprintActor(const FJsonObjectParameter& Params);

	/**
	 * 设置编辑器视口的焦点
	 * @param Params - 输入参数，必须包含"target"或"location"字段之一，可选"distance"和"orientation"
	 * @return 操作结果
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleFocusViewport(const FJsonObjectParameter& Params);
	
	/**
	 * 捕获编辑器视口的屏幕截图
	 * @param Params - 输入参数，必须包含"filepath"字段
	 * @return 包含截图文件路径的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleTakeScreenshot(const FJsonObjectParameter& Params);

	/**
	 * 加载指定资产路径的关卡
	 * @param Params - 输入参数，必须包含 "map_path" 字段
	 * @return 包含加载结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleLoadMap(const FJsonObjectParameter& Params);

	/**
	 * 保存当前编辑器中活动关卡
	 * @param Params - 输入参数(不需要特定字段)
	 * @return 包含保存结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSaveCurrentMap(const FJsonObjectParameter& Params);

	/**
	 * 将当前活动关卡另存为新的资产路径
	 * @param Params - 输入参数，必须包含 "target_map_path" 字段
	 * @return 包含另存为结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSaveMapAs(const FJsonObjectParameter& Params);

	/**
	 * 创建一张新的空白关卡
	 * @param Params - 输入参数，必须包含 "map_path" 字段
	 * @return 包含创建结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleCreateBlankMap(const FJsonObjectParameter& Params);

	/**
	 * 从模板创建一张新关卡
	 * @param Params - 输入参数，必须包含 "map_path" 和 "template_map_path" 字段
	 * @return 包含创建结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleCreateMapFromTemplate(const FJsonObjectParameter& Params);

	/**
	 * 创建带静态网格的Actor
	 * @param Params - 输入参数，必须包含 "name" 和 "mesh_path" 字段
	 * @return 包含创建结果的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSpawnStaticMeshActor(const FJsonObjectParameter& Params);

	/**
	 * 按名称前缀查找Actor
	 * @param Params - 输入参数，必须包含 "prefix" 字段
	 * @return 包含匹配Actor数组的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleFindActorsByPrefix(const FJsonObjectParameter& Params);

	/**
	 * 按名称前缀删除Actor
	 * @param Params - 输入参数，必须包含 "prefix" 字段
	 * @return 包含删除结果的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleDeleteActorsByPrefix(const FJsonObjectParameter& Params);

	/**
	 * 重置测试场景(当前实现等价于按前缀删除)
	 * @param Params - 输入参数，必须包含 "prefix" 字段
	 * @return 包含重置结果的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleResetTestbed(const FJsonObjectParameter& Params);

	/**
	 * 创建或更新一个用于证据采集的相机
	 * @param Params - 输入参数，必须包含 "name"，可选 location/rotation
	 * @return 包含相机结果的JSON对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleEnsureCaptureCamera(const FJsonObjectParameter& Params);

	/**
	 * 查找当前关卡中的基础 lighting rig
	 * @param Params - 输入参数(当前不需要特定字段)
	 * @return 包含 directional light / sky light / fog 信息的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleFindLightingRig(const FJsonObjectParameter& Params);

	/**
	 * 确保当前关卡中存在最小 lighting rig
	 * @param Params - 可包含 "prefix"、"directional_light_name"、"sky_light_name"、"fog_name"
	 * @return 包含 lighting rig 信息的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleEnsureBasicLightingRig(const FJsonObjectParameter& Params);

	/**
	 * 设置 DirectionalLight 的常用属性
	 * @param Params - 必须包含 "name"，可选 intensity / light_color / temperature / indirect_intensity / source_angle / rotation
	 * @return 包含更新结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSetDirectionalLight(const FJsonObjectParameter& Params);

	/**
	 * 设置 SkyLight 的常用属性
	 * @param Params - 必须包含 "name"，可选 intensity / light_color
	 * @return 包含更新结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSetSkyLight(const FJsonObjectParameter& Params);

	/**
	 * 设置 ExponentialHeightFog 的常用属性
	 * @param Params - 必须包含 "name"，可选 fog_density / fog_height_falloff / start_distance / fog_inscattering_color
	 * @return 包含更新结果的 JSON 对象
	 */
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter HandleSetExponentialHeightFog(const FJsonObjectParameter& Params);

	
	UFUNCTION(BlueprintCallable, Category = "MCP|Blueprint")
	static FJsonObjectParameter ConvertObjectToJson(UObject* TargetObject);
};
