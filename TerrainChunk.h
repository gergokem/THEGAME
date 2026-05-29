// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "WorldConfigDataAsset.h"
#include "FastNoiseLite.h"
#include "TerrainChunk.generated.h"

struct FDynamicNoise
{
	FName NoiseName;
	FastNoiseLite NoiseGen;
	// Удали FVector2D MinMax; отсюда!
	FNoiseSettings Settings; // Теперь мы храним тут ВСЕ настройки шума
};

struct FChunkMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FVector2D> UV1s; // << СЮДА ЗАПИШЕМ TEX_A и TEX_B
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
};


UCLASS()
class THEGAME_API ATerrainChunk : public AActor
{
	GENERATED_BODY()

public:
	ATerrainChunk();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
	UProceduralMeshComponent* ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float VoxelSize = 100.0f;

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	bool IsGenerating() const { return bIsGenerating; }

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void GenerateChunkAsync(int32 GlobalSeed, float ZeroDownLevel, float ZeroUpLevel, UWorldConfigDataAsset* BaseWorldConfig, const TMap<FName, FNoiseSettings>& Overrides);

private:
	static const int32 GridSize = 32;
	std::atomic<bool> bIsGenerating{ false };

	TArray<float> VoxelDensities;

	// Полностью статические утилиты для потокобезопасного расчета (не используют и не требуют 'this')
	static int32 GetIndex(int32 X, int32 Y, int32 Z);
	static FVector InterpolateVerts(FVector V1, FVector V2, float D1, float D2);

	static void MarchCubeThreadSafe(
	int32 X, int32 Y, int32 Z,
	float ZeroDownLevel, float ZeroUpLevel, float LocalVoxelSize,
	const TArray<float>& Densities, const TArray<float>& TerrainHeights,
	FChunkMeshData& OutSectionData, // << ТЕПЕРЬ ТУТ ПРОСТО ССЫЛКА НА ОДНУ ДАТУ
	UWorldConfigDataAsset* BaseWorldConfig,
	const TArray<FDynamicNoise>& ExtraNoises,
	FVector ChunkWorldPos,
	TMap<FName, float>& ReusableNoiseMap,
	const TMap<FName, int32>& TextureIndexCache);

	static UBiomeDataAsset* GetDominantBiome(float GlobalX, float GlobalY, float GlobalZ, float SurfaceZ, float ZeroDownLevel, float ZeroUpLevel, UWorldConfigDataAsset* WorldConfig, const TArray<FDynamicNoise>& ExtraNoises, TMap<FName, float>& ReusableNoiseMap);
	static const FBiomeLayer* GetBiomeLayer(UBiomeDataAsset* Biome, float Depth, FVector Normal, float GlobalX, float GlobalY, float GlobalZ, float LocalVoxelSize);
};
