// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldConfigDataAsset.generated.h"

UENUM(BlueprintType)
enum class ENoiseGeneratorType : uint8
{
	Perlin UMETA(DisplayName = "Perlin"),
	OpenSimplex2 UMETA(DisplayName = "Simplex (OpenSimplex2)"),
	Cellular UMETA(DisplayName = "Cellular")
};

UENUM(BlueprintType)
enum class EFractalType : uint8
{
	None UMETA(DisplayName = "None"),
	FBm UMETA(DisplayName = "FBm (Standard Layering)"),
	Ridged UMETA(DisplayName = "Ridged (Sharp Mountains)")
};

UENUM(BlueprintType)
enum class ENoiseDimensionType : uint8
{
	Noise2D_Flat UMETA(DisplayName = "2D Noise (Flat - For Core Heights)"),
	Noise2D_Altitude UMETA(DisplayName = "2D + Z-Gradient (For Biomes)"),
	Noise3D UMETA(DisplayName = "3D Noise")
};

UENUM(BlueprintType)
enum class ENoiseDomain : uint8
{
	Anywhere UMETA(DisplayName = "Anywhere"),
	Underground UMETA(DisplayName = "Underground Only"),
	Overground UMETA(DisplayName = "Overground Only")
};

USTRUCT(BlueprintType)
struct FBiomeLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Layer")
	FName LayerName = "DefaultLayer";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Layer", meta = (GetOptions = "GetTextureOptions"))
	FName TextureName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Layer")
	float DigResistance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Layer")
	float ThicknessPercentage = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Layer")
	float TransitionNoiseScale = 0.0f;
};

USTRUCT(BlueprintType)
struct FNoiseSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise Type")
	ENoiseDimensionType DimensionType = ENoiseDimensionType::Noise3D;

	// --- 3D NOISE ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "3D Domain", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise3D", EditConditionHides))
	ENoiseDomain NoiseDomain = ENoiseDomain::Anywhere;

	// --- 2D + Z-GRADIENT ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude", EditConditionHides))
	bool bContinuousGradient = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude && !bContinuousGradient", EditConditionHides))
	bool bUseSeaLevelAsAnchor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude", EditConditionHides))
	float ValueAtBedrock = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude", EditConditionHides))
	float ValueAtAtmosphere = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude && !bContinuousGradient", EditConditionHides))
	float ValueUnderSurface = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude && !bContinuousGradient", EditConditionHides))
	float ValueOverSurface = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude && !bContinuousGradient", EditConditionHides))
	float SurfaceBlendRange = 500.0f;

	// Сила искажения градиента шумом
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Altitude Gradient", meta = (EditCondition = "DimensionType==ENoiseDimensionType::Noise2D_Altitude", EditConditionHides))
	float AltitudeNoiseAmplitude = 5.0f;


	// --- COMMON SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	ENoiseGeneratorType NoiseType = ENoiseGeneratorType::OpenSimplex2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	EFractalType FractalType = EFractalType::FBm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 SeedOffset = 0;

	// Прячем MinMax, если выбран Градиент!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (EditCondition = "DimensionType!=ENoiseDimensionType::Noise2D_Altitude", EditConditionHides))
	FVector2D MinMax = FVector2D(0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	float Frequency = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 Octaves = 4;
};

UCLASS(BlueprintType)
class THEGAME_API UBiomeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Ссылка на мир, чтобы прочитать массив текстур для выпадающего списка
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	UWorldConfigDataAsset* ParentWorldConfig = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Info")
	FName BiomeName = "Unknown Biome";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Info")
	FLinearColor DebugColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Generation")
	float SuperPriority = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Generation")
	TMap<FName, FVector2D> BStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Layers")
	TArray<FBiomeLayer> LayersUnderAir;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Layers")
	TArray<FBiomeLayer> LayersOverAir;

	// ТВОЯ ФУНКЦИЯ-АВТОМАТ ДЛЯ РЕДАКТОРА
	UFUNCTION(BlueprintCallable, Category = "Biome Logic")
	float CalculateBiomeWeight(const TMap<FName, float>& CurrentWorldNoises) const;

	// 2. ИСПРАВЛЕНО: Объявляем функцию-генератор опций для выпадающего списка
	UFUNCTION()
	TArray<FString> GetTextureOptions() const;
};

UCLASS(BlueprintType)
class THEGAME_API UStructureDataAsset : public UPrimaryDataAsset { GENERATED_BODY() };

UCLASS(BlueprintType)
class THEGAME_API UNPCDataAsset : public UPrimaryDataAsset { GENERATED_BODY() };

UCLASS(BlueprintType)
class THEGAME_API UWorldConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Limits")
	float ZeroDownLevel = -100000.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Limits")
	float ZeroUpLevel = 100000.0f; 

	// ГЛОБАЛЬНЫЙ МАТЕРИАЛ ДЛЯ ВСЕГО МИРА
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Material")
	UMaterialInterface* WorldMasterMaterial = nullptr;

	// РЕЕСТР ТЕКСТУР: Сюда ты закидываешь все текстуры игры, чтобы C++ знал их номера
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecosystem | Textures")
	TArray<UTexture2D*> GlobalTextureRegistry;

	// =========================================================================
	// ЖЕСТКО ЗАДАННАЯ ИЕРАРХИЯ ШУМОВ ИЗ ТАБЛИЦЫ
	// =========================================================================

	// 1. Базовый каркас мира (2D: X, Z). Задает общую высоту (океан, низина, плато).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation | Core")
	FNoiseSettings BaseHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation | Core")
	FNoiseSettings MountainRoughness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation | Core")
	FNoiseSettings SpaghettiCave;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation | Core")
	FNoiseSettings CheeseCave;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation | Core")
	FNoiseSettings EntranceModulator;


	// =========================================================================
	// ЭКОСИСТЕМА
	// =========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation | Extra")
	TMap<FName, FNoiseSettings> WorldStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecosystem | Biomes")
	TArray<UBiomeDataAsset*> DA_Biomes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecosystem | Structures")
	TArray<UStructureDataAsset*> DA_GStructures;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecosystem | Structures")
	TArray<UStructureDataAsset*> DA_MStructures;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ecosystem | NPC")
	TArray<UNPCDataAsset*> DA_GNPC;
};