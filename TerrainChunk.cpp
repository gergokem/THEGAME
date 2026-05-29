// Fill out your copyright notice in the Description page of Project Settings.

#include "TerrainChunk.h"
#include "MarchingCubesTables.h"
#include "FastNoiseLite.h"
#include "Async/Async.h"
#include "KismetProceduralMeshLibrary.h"
#include "UObject/UObjectGlobals.h" 

// ---------------------------------------------------------
// НАСТРОЙКА ГЕНЕРАТОРОВ ШУМА
// ---------------------------------------------------------
static void SetupNoiseGenerator(FastNoiseLite& OutNoise, const FNoiseSettings& Settings, int32 GlobalSeed)
{
	OutNoise.SetSeed(GlobalSeed + Settings.SeedOffset);
	OutNoise.SetFrequency(Settings.Frequency);

	switch (Settings.NoiseType)
	{
	case ENoiseGeneratorType::Perlin:
		OutNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin); break;
	case ENoiseGeneratorType::Cellular:
		OutNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular); break;
	case ENoiseGeneratorType::OpenSimplex2:
	default: OutNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2); break;
	}

	switch (Settings.FractalType)
	{
	case EFractalType::FBm:
		OutNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
		OutNoise.SetFractalOctaves(Settings.Octaves); break;
	case EFractalType::Ridged:
		OutNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
		OutNoise.SetFractalOctaves(Settings.Octaves); break;
	case EFractalType::None:
	default: OutNoise.SetFractalType(FastNoiseLite::FractalType_None); break;
	}
}

TArray<FString> UBiomeDataAsset::GetTextureOptions() const
{
	TArray<FString> Options;

	// Ищем загруженный WorldConfig в памяти редактора
	TArray<UObject*> WorldConfigs;
	GetObjectsOfClass(UWorldConfigDataAsset::StaticClass(), WorldConfigs);

	if (WorldConfigs.Num() > 0)
	{
		UWorldConfigDataAsset* WorldConfig = Cast<UWorldConfigDataAsset>(WorldConfigs[0]);
		if (WorldConfig)
		{
			// Пробегаемся по реестру текстур мира и формируем красивый список
			for (int32 i = 0; i < WorldConfig->GlobalTextureRegistry.Num(); ++i)
			{
				if (WorldConfig->GlobalTextureRegistry[i])
				{
					// Формат строки: "0: T_Grass_D" (C++ сам поймет индекс при парсинге)
					Options.Add(FString::Printf(TEXT("%d: %s"), i, *WorldConfig->GlobalTextureRegistry[i]->GetName()));
				}
				else
				{
					Options.Add(FString::Printf(TEXT("%d: [Empty Slot]"), i));
				}
			}
		}
	}

	// Если мир еще не создан или пуст, даем дефолтную заглушку
	if (Options.Num() == 0)
	{
		Options.Add(TEXT("0: [No World Config Found]"));
	}

	return Options;
}


// ---------------------------------------------------------
// ВЫБОРКА БИОМОВ
// ---------------------------------------------------------
UBiomeDataAsset* ATerrainChunk::GetDominantBiome(float GlobalX, float GlobalY, float GlobalZ, float SurfaceZ, float ZeroDownLevel, float ZeroUpLevel, UWorldConfigDataAsset* WorldConfig, const TArray<FDynamicNoise>& ExtraNoises, TMap<FName, float>& ReusableNoiseMap)
{
	if (!WorldConfig || WorldConfig->DA_Biomes.Num() == 0) return nullptr;
	ReusableNoiseMap.Reset();

	for (const FDynamicNoise& NoiseItem : ExtraNoises)
	{
		float FinalValue = 0.0f;
		const FNoiseSettings& Settings = NoiseItem.Settings;

		if (Settings.DimensionType == ENoiseDimensionType::Noise3D)
		{
			if (Settings.NoiseDomain == ENoiseDomain::Underground && GlobalZ > SurfaceZ) continue;
			if (Settings.NoiseDomain == ENoiseDomain::Overground && GlobalZ <= SurfaceZ) continue;

			float Raw = NoiseItem.NoiseGen.GetNoise(GlobalX, GlobalY, GlobalZ);
			FinalValue = FMath::Lerp(Settings.MinMax.X, Settings.MinMax.Y, (Raw + 1.0f) * 0.5f);
		}
		else if (Settings.DimensionType == ENoiseDimensionType::Noise2D_Flat)
		{
			float Raw = NoiseItem.NoiseGen.GetNoise(GlobalX, GlobalY);
			FinalValue = FMath::Lerp(Settings.MinMax.X, Settings.MinMax.Y, (Raw + 1.0f) * 0.5f);
		}
		else
		{
			float GradientVal = 0.0f;
			if (Settings.bContinuousGradient)
			{
				float Alpha = FMath::Clamp((GlobalZ - ZeroDownLevel) / (ZeroUpLevel - ZeroDownLevel), 0.0f, 1.0f);
				GradientVal = FMath::Lerp(Settings.ValueAtBedrock, Settings.ValueAtAtmosphere, Alpha);
			}
			else
			{
				float AnchorZ = Settings.bUseSeaLevelAsAnchor ? 0.0f : SurfaceZ;
				float BlendR = Settings.SurfaceBlendRange;

				if (GlobalZ < AnchorZ - BlendR) {
					float Alpha = FMath::Clamp((GlobalZ - ZeroDownLevel) / FMath::Max(1.0f, AnchorZ - BlendR - ZeroDownLevel), 0.0f, 1.0f);
					GradientVal = FMath::Lerp(Settings.ValueAtBedrock, Settings.ValueUnderSurface, Alpha);
				}
				else if (GlobalZ > AnchorZ + BlendR) {
					float Alpha = FMath::Clamp((GlobalZ - (AnchorZ + BlendR)) / FMath::Max(1.0f, ZeroUpLevel - (AnchorZ + BlendR)), 0.0f, 1.0f);
					GradientVal = FMath::Lerp(Settings.ValueOverSurface, Settings.ValueAtAtmosphere, Alpha);
				}
				else {
					float Alpha = FMath::Clamp((GlobalZ - (AnchorZ - BlendR)) / (BlendR * 2.0f), 0.0f, 1.0f);
					GradientVal = FMath::Lerp(Settings.ValueUnderSurface, Settings.ValueOverSurface, FMath::SmoothStep(0.0f, 1.0f, Alpha));
				}
			}
			float Raw = NoiseItem.NoiseGen.GetNoise(GlobalX, GlobalY);
			FinalValue = GradientVal + (Raw * Settings.AltitudeNoiseAmplitude);
		}
		ReusableNoiseMap.Add(NoiseItem.NoiseName, FinalValue);
	}

	UBiomeDataAsset* BestBiome = nullptr;
	float MaxWeight = 0.0f;

	for (UBiomeDataAsset* Biome : WorldConfig->DA_Biomes)
	{
		if (!Biome) continue;
		float Weight = Biome->CalculateBiomeWeight(ReusableNoiseMap);

		if (Weight >= 1000.0f) return Biome;
		if (Weight > MaxWeight)
		{
			MaxWeight = Weight;
			BestBiome = Biome;
		}
	}
	return BestBiome;
}

const FBiomeLayer* ATerrainChunk::GetBiomeLayer(UBiomeDataAsset* Biome, float Depth, FVector Normal, float GlobalX, float GlobalY, float GlobalZ, float LocalVoxelSize)
{
	if (!Biome) return nullptr;

	const TArray<FBiomeLayer>& TargetLayers = (Normal.Z > -0.2f) ? Biome->LayersUnderAir : Biome->LayersOverAir;
	if (TargetLayers.Num() == 0) return nullptr;

	float NoiseOffset = (FMath::Sin(GlobalX * 0.05f) + FMath::Cos(GlobalY * 0.05f) + FMath::Sin(GlobalX * 0.15f + GlobalY * 0.15f)) * 50.0f;
	float CurrentDepthThreshold = 0.0f;

	for (const FBiomeLayer& Layer : TargetLayers)
	{
		float LayerThickness = Layer.ThicknessPercentage * LocalVoxelSize;
		float LayerBoundary = CurrentDepthThreshold + LayerThickness + (NoiseOffset * Layer.TransitionNoiseScale);

		if (Depth <= LayerBoundary)
		{
			return &Layer;
		}
		CurrentDepthThreshold += LayerThickness;
	}
	return &TargetLayers.Last();
}

// ---------------------------------------------------------
// БАЗОВЫЕ МЕТОДЫ ЧАНКА
// ---------------------------------------------------------
ATerrainChunk::ATerrainChunk()
{
	PrimaryActorTick.bCanEverTick = false;
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	RootComponent = ProceduralMesh;
}

void ATerrainChunk::BeginPlay() { Super::BeginPlay(); }

int32 ATerrainChunk::GetIndex(int32 X, int32 Y, int32 Z)
{
	int32 Size = GridSize + 2;
	return X + (Y * Size) + (Z * Size * Size);
}

FVector ATerrainChunk::InterpolateVerts(FVector V1, FVector V2, float D1, float D2)
{
	float Diff = D2 - D1;
	if (FMath::IsNearlyZero(Diff, 0.00001f)) return (V1 + V2) * 0.5f;
	float T = (0.0f - D1) / Diff;
	T = FMath::Clamp(T, 0.0f, 1.0f);
	return V1 + T * (V2 - V1);
}

// ---------------------------------------------------------
// АЛГОРИТМ MARCHING CUBES (С НОВЫМ РЕНДЕРОМ ААА)
// ---------------------------------------------------------
void ATerrainChunk::MarchCubeThreadSafe(
	int32 X, int32 Y, int32 Z,
	float ZeroDownLevel, float ZeroUpLevel, float LocalVoxelSize,
	const TArray<float>& Densities, const TArray<float>& TerrainHeights,
	FChunkMeshData& OutSectionData, // Единый кэш данных
	UWorldConfigDataAsset* BaseWorldConfig,
	const TArray<FDynamicNoise>& ExtraNoises,
	FVector ChunkWorldPos,
	TMap<FName, float>& ReusableNoiseMap)
{
	float CubeValues[8];
	int32 CubeIndex = 0;
	float IsoLevel = 0.0f;
	int32 Size = GridSize + 2;

	for (int32 i = 0; i < 8; i++)
	{
		int32 CornerX = X + FMath::RoundToInt(CubeCorners[i].X);
		int32 CornerY = Y + FMath::RoundToInt(CubeCorners[i].Y);
		int32 CornerZ = Z + FMath::RoundToInt(CubeCorners[i].Z);

		int32 Idx = GetIndex(CornerX, CornerY, CornerZ);
		CubeValues[i] = Densities[Idx];

		if (CubeValues[i] < IsoLevel)
		{
			CubeIndex |= (1 << i);
		}
	}

	if (CubeIndex == 0 || CubeIndex == 255) return;

	float Nx = (CubeValues[1] + CubeValues[3] + CubeValues[5] + CubeValues[7]) - (CubeValues[0] + CubeValues[2] + CubeValues[4] + CubeValues[6]);
	float Ny = (CubeValues[2] + CubeValues[3] + CubeValues[6] + CubeValues[7]) - (CubeValues[0] + CubeValues[1] + CubeValues[4] + CubeValues[5]);
	float Nz = (CubeValues[4] + CubeValues[5] + CubeValues[6] + CubeValues[7]) - (CubeValues[0] + CubeValues[1] + CubeValues[2] + CubeValues[3]);
	FVector TrueNormal = -FVector(Nx, Ny, Nz).GetSafeNormal();

	float CellCenterX = ChunkWorldPos.X + (X + 0.5f) * LocalVoxelSize;
	float CellCenterY = ChunkWorldPos.Y + (Y + 0.5f) * LocalVoxelSize;
	float CellCenterZ = ChunkWorldPos.Z + (Z + 0.5f) * LocalVoxelSize;
	float TargetZ = TerrainHeights[X + Y * Size];

	UBiomeDataAsset* CellBiome = GetDominantBiome(CellCenterX, CellCenterY, CellCenterZ, TargetZ, ZeroDownLevel, BaseWorldConfig->ZeroUpLevel, BaseWorldConfig, ExtraNoises, ReusableNoiseMap);

	uint8 CellClass = regularCellClass[CubeIndex];
	const FRegularCellData& CellData = regularCellData[CellClass];
	int32 TriangleCount = CellData.GeometryCounts & 0x0F;

	auto GetVertexFromEdgeCode = [X, Y, Z, LocalVoxelSize, &CubeValues](uint16 edgeCode) -> FVector
		{
			int vA = (edgeCode >> 4) & 0x000F;
			int vB = edgeCode & 0x000F;

			FVector CornerPosA = FVector(
				(X + CubeCorners[vA].X) * LocalVoxelSize,
				(Y + CubeCorners[vA].Y) * LocalVoxelSize,
				(Z + CubeCorners[vA].Z) * LocalVoxelSize);

			FVector CornerPosB = FVector(
				(X + CubeCorners[vB].X) * LocalVoxelSize,
				(Y + CubeCorners[vB].Y) * LocalVoxelSize,
				(Z + CubeCorners[vB].Z) * LocalVoxelSize);

			return InterpolateVerts(CornerPosA, CornerPosB, CubeValues[vA], CubeValues[vB]);
		};

	auto SafeGetDensity = [&](int32 lx, int32 ly, int32 lz) -> float {
		lx = FMath::Clamp(lx, 0, Size - 1);
		ly = FMath::Clamp(ly, 0, Size - 1);
		lz = FMath::Clamp(lz, 0, Size - 1);
		return Densities[lx + (ly * Size) + (lz * Size * Size)];
		};

	auto GetGridNormal = [&](int32 lx, int32 ly, int32 lz) -> FVector {
		float nx = SafeGetDensity(lx + 1, ly, lz) - SafeGetDensity(lx - 1, ly, lz);
		float ny = SafeGetDensity(lx, ly + 1, lz) - SafeGetDensity(lx, ly - 1, lz);
		float nz = SafeGetDensity(lx, ly, lz + 1) - SafeGetDensity(lx, ly, lz - 1);
		return FVector(nx, ny, nz).GetSafeNormal();
		};

	auto GetSmoothNormal = [&](uint16 edgeCode) -> FVector {
		int vA = (edgeCode >> 4) & 0x000F;
		int vB = edgeCode & 0x000F;
		int32 Ax = X + FMath::RoundToInt(CubeCorners[vA].X);
		int32 Ay = Y + FMath::RoundToInt(CubeCorners[vA].Y);
		int32 Az = Z + FMath::RoundToInt(CubeCorners[vA].Z);
		int32 Bx = X + FMath::RoundToInt(CubeCorners[vB].X);
		int32 By = Y + FMath::RoundToInt(CubeCorners[vB].Y);
		int32 Bz = Z + FMath::RoundToInt(CubeCorners[vB].Z);

		FVector Na = GetGridNormal(Ax, Ay, Az);
		FVector Nb = GetGridNormal(Bx, By, Bz);

		float d1 = CubeValues[vA];
		float d2 = CubeValues[vB];
		float diff = d2 - d1;
		float t = FMath::IsNearlyZero(diff) ? 0.5f : FMath::Clamp((0.0f - d1) / diff, 0.0f, 1.0f);
		return (Na + t * (Nb - Na)).GetSafeNormal();
		};

	for (int32 i = 0; i < TriangleCount; i++)
	{
		int32 abstract_Idx0 = CellData.VertexIndices[i * 3 + 0];
		int32 abstract_Idx1 = CellData.VertexIndices[i * 3 + 1];
		int32 abstract_Idx2 = CellData.VertexIndices[i * 3 + 2];

		uint16 edgeCode0 = regularVertexData[CubeIndex][abstract_Idx0];
		uint16 edgeCode1 = regularVertexData[CubeIndex][abstract_Idx1];
		uint16 edgeCode2 = regularVertexData[CubeIndex][abstract_Idx2];

		FVector P0 = GetVertexFromEdgeCode(edgeCode0);
		FVector P1 = GetVertexFromEdgeCode(edgeCode1);
		FVector P2 = GetVertexFromEdgeCode(edgeCode2);

		float GlobalZ0 = ChunkWorldPos.Z + P0.Z;
		float GlobalZ1 = ChunkWorldPos.Z + P1.Z;
		float GlobalZ2 = ChunkWorldPos.Z + P2.Z;

		float Depth0 = FMath::Max(0.0f, TargetZ - GlobalZ0);
		float Depth1 = FMath::Max(0.0f, TargetZ - GlobalZ1);
		float Depth2 = FMath::Max(0.0f, TargetZ - GlobalZ2);

		const FBiomeLayer* Layer0 = GetBiomeLayer(CellBiome, Depth0, TrueNormal, ChunkWorldPos.X + P0.X, ChunkWorldPos.Y + P0.Y, GlobalZ0, LocalVoxelSize);
		const FBiomeLayer* Layer1 = GetBiomeLayer(CellBiome, Depth1, TrueNormal, ChunkWorldPos.X + P1.X, ChunkWorldPos.Y + P1.Y, GlobalZ1, LocalVoxelSize);
		const FBiomeLayer* Layer2 = GetBiomeLayer(CellBiome, Depth2, TrueNormal, ChunkWorldPos.X + P2.X, ChunkWorldPos.Y + P2.Y, GlobalZ2, LocalVoxelSize);

		// ---------------------------------------------------------
		// УМНЫЙ ПОИСК ИНДЕКСОВ (АВТОМАТИЗАЦИЯ ПО ТЕКСТУРАМ)
		// ---------------------------------------------------------
		auto GetTextureIndex = [&](const FBiomeLayer* Layer) -> int32 {
			// 1. Проверяем на валидность и IsNone() вместо оператора "!"
			if (!Layer || Layer->TextureName.IsNone() || !BaseWorldConfig) return 0;

			FString LeftSide, RightSide;
			FString AssetName = Layer->TextureName.ToString();

			// 2. Если строка имеет вид "0: T_Grass_D", отсекаем индекс и берем чистое имя
			if (AssetName.Split(TEXT(": "), &LeftSide, &RightSide))
			{
				// Ищем в реестре текстуру, чье имя совпадает с правой частью строки
				int32 Idx = BaseWorldConfig->GlobalTextureRegistry.IndexOfByPredicate([&](const UTexture2D* Tex) {
					return Tex && Tex->GetName() == RightSide;
					});
				return Idx == INDEX_NONE ? 0 : Idx;
			}

			// 3. Резервный поиск (если ввели просто имя "T_Grass_D" без выпадающего списка)
			int32 Idx = BaseWorldConfig->GlobalTextureRegistry.IndexOfByPredicate([&](const UTexture2D* Tex) {
				return Tex && Tex->GetFName() == Layer->TextureName;
				});
			return Idx == INDEX_NONE ? 0 : Idx;
			};

		int32 TexA = GetTextureIndex(Layer0);
		int32 TexB = TexA;
		if (Layer1 && GetTextureIndex(Layer1) != TexA) TexB = GetTextureIndex(Layer1);
		else if (Layer2 && GetTextureIndex(Layer2) != TexA) TexB = GetTextureIndex(Layer2);

		auto GetVertexAlpha = [&](const FBiomeLayer* L) -> float {
			if (!L) return 0.0f;
			int32 LTex = GetTextureIndex(L);
			if (LTex == TexA) return 0.0f;
			if (LTex == TexB) return 1.0f;
			return 0.5f;
			};

		int32 VertIndexStart = OutSectionData.Vertices.Num();
		OutSectionData.Vertices.Add(P0);
		OutSectionData.Vertices.Add(P1);
		OutSectionData.Vertices.Add(P2);

		auto CalculateUV = [](FVector WorldPos, FVector Normal) -> FVector2D
			{
				FVector AbsN = Normal.GetAbs();
				if (AbsN.Z >= AbsN.X && AbsN.Z >= AbsN.Y) return FVector2D(WorldPos.X, WorldPos.Y);
				if (AbsN.X >= AbsN.Y && AbsN.X >= AbsN.Z) return FVector2D(WorldPos.Y, WorldPos.Z);
				return FVector2D(WorldPos.X, WorldPos.Z);
			};

		float UVScale = 100.0f;
		OutSectionData.UVs.Add(CalculateUV(P0 + ChunkWorldPos, TrueNormal) / UVScale);
		OutSectionData.UVs.Add(CalculateUV(P1 + ChunkWorldPos, TrueNormal) / UVScale);
		OutSectionData.UVs.Add(CalculateUV(P2 + ChunkWorldPos, TrueNormal) / UVScale);

		// ПИШЕМ ЧИСТЫЕ ИНДЕКСЫ В UV1 
		OutSectionData.UV1s.Add(FVector2D(TexA, TexB));
		OutSectionData.UV1s.Add(FVector2D(TexA, TexB));
		OutSectionData.UV1s.Add(FVector2D(TexA, TexB));

		// ПИШЕМ АЛЬФУ СМЕШИВАНИЯ В КРАСНЫЙ КАНАЛ VERTEX COLOR
		OutSectionData.Colors.Add(FLinearColor(GetVertexAlpha(Layer0), 0.0f, 0.0f, 1.0f));
		OutSectionData.Colors.Add(FLinearColor(GetVertexAlpha(Layer1), 0.0f, 0.0f, 1.0f));
		OutSectionData.Colors.Add(FLinearColor(GetVertexAlpha(Layer2), 0.0f, 0.0f, 1.0f));

		OutSectionData.Triangles.Add(VertIndexStart);
		OutSectionData.Triangles.Add(VertIndexStart + 1);
		OutSectionData.Triangles.Add(VertIndexStart + 2);

		FVector N0 = GetSmoothNormal(edgeCode0);
		FVector N1 = GetSmoothNormal(edgeCode1);
		FVector N2 = GetSmoothNormal(edgeCode2);

		OutSectionData.Normals.Add(N0);
		OutSectionData.Normals.Add(N1);
		OutSectionData.Normals.Add(N2);

		auto CalcTangent = [](FVector N) {
			FVector T = FVector::CrossProduct(FVector(0, 0, 1), N).GetSafeNormal();
			if (T.IsNearlyZero()) T = FVector(1, 0, 0);
			return FProcMeshTangent(T, false);
			};

		OutSectionData.Tangents.Add(CalcTangent(N0));
		OutSectionData.Tangents.Add(CalcTangent(N1));
		OutSectionData.Tangents.Add(CalcTangent(N2));
	}
}

// ---------------------------------------------------------
// ГЛАВНЫЙ ПОТОК ГЕНЕРАЦИИ
// ---------------------------------------------------------
void ATerrainChunk::GenerateChunkAsync(int32 GlobalSeed, float ZeroDownLevel, float ZeroUpLevel, UWorldConfigDataAsset* BaseWorldConfig, const TMap<FName, FNoiseSettings>& Overrides)
{
	if (bIsGenerating.load() || !BaseWorldConfig) return;
	bIsGenerating.store(true);

	FVector ChunkWorldPos = GetActorLocation();
	float LocalVoxelSize = VoxelSize;
	int32 Size = GridSize + 2; // Вычисляем размер заранее

	UMaterialInterface* TargetMaterial = BaseWorldConfig->WorldMasterMaterial;

	FNoiseSettings ActiveBaseHeight = Overrides.Contains(FName("BaseHeight")) ? Overrides.FindRef(FName("BaseHeight")) : BaseWorldConfig->BaseHeight;
	FNoiseSettings ActiveMountain = Overrides.Contains(FName("MountainRoughness")) ? Overrides.FindRef(FName("MountainRoughness")) : BaseWorldConfig->MountainRoughness;
	FNoiseSettings ActiveSpaghetti = Overrides.Contains(FName("SpaghettiCave")) ? Overrides.FindRef(FName("SpaghettiCave")) : BaseWorldConfig->SpaghettiCave;
	FNoiseSettings ActiveCheese = Overrides.Contains(FName("CheeseCave")) ? Overrides.FindRef(FName("CheeseCave")) : BaseWorldConfig->CheeseCave;
	FNoiseSettings ActiveEntrance = Overrides.Contains(FName("EntranceModulator")) ? Overrides.FindRef(FName("EntranceModulator")) : BaseWorldConfig->EntranceModulator;

	TArray<FDynamicNoise> LocalExtraNoises;
	for (const auto& StatPair : BaseWorldConfig->WorldStats)
	{
		FDynamicNoise NewNoise;
		NewNoise.NoiseName = StatPair.Key;
		NewNoise.Settings = StatPair.Value;
		SetupNoiseGenerator(NewNoise.NoiseGen, StatPair.Value, GlobalSeed);
		LocalExtraNoises.Add(NewNoise);
	}

	// ====================================================================
	// ОПТИМИЗАЦИЯ ПАМЯТИ: Выносим ВСЕ аллокации TArray на GameThread
	// Это снимает локи с менеджера памяти (FMalloc) в рабочих потоках
	// ====================================================================
	TArray<float> PreAllocDensities;
	PreAllocDensities.SetNumUninitialized(Size * Size * Size);

	TArray<float> PreAllocTerrainHeights;
	PreAllocTerrainHeights.SetNumUninitialized(Size * Size);

	FChunkMeshData PreAllocSectionData;
	// Резервируем память с запасом, чтобы избежать Resize() внутри Async-потока
	PreAllocSectionData.Vertices.Reserve(5000);
	PreAllocSectionData.Triangles.Reserve(15000);
	PreAllocSectionData.Normals.Reserve(5000);
	PreAllocSectionData.UVs.Reserve(5000);
	PreAllocSectionData.UV1s.Reserve(5000);
	PreAllocSectionData.Colors.Reserve(5000);
	PreAllocSectionData.Tangents.Reserve(5000);

	TMap<FName, float> PreAllocNoiseMap;
	PreAllocNoiseMap.Reserve(LocalExtraNoises.Num());

	// Захватываем массивы через MoveTemp (мгновенный перенос владения указателями без копирования данных)
	Async(EAsyncExecution::ThreadPool, [this, ChunkWorldPos, LocalVoxelSize, ActiveBaseHeight, ActiveMountain, ActiveSpaghetti, ActiveCheese, ActiveEntrance, LocalExtraNoises, TargetMaterial, GlobalSeed, ZeroDownLevel, ZeroUpLevel, BaseWorldConfig, Size,
		MovedDensities = MoveTemp(PreAllocDensities),
		MovedHeights = MoveTemp(PreAllocTerrainHeights),
		MovedSectionData = MoveTemp(PreAllocSectionData),
		MovedNoiseMap = MoveTemp(PreAllocNoiseMap)]() mutable
		{
			// Создаем ссылки (алиасы) на перемещенные данные, чтобы не переписывать твой внутренний код
			TArray<float>& LocalDensities = MovedDensities;
			TArray<float>& LocalTerrainHeights = MovedHeights;
			FChunkMeshData& LocalSectionData = MovedSectionData;
			TMap<FName, float>& ThreadLocalNoiseMap = MovedNoiseMap;

			FastNoiseLite BaseNoise, RoughNoise, SpaghettiNoise, CheeseNoise, EntranceNoise;
			SetupNoiseGenerator(BaseNoise, ActiveBaseHeight, GlobalSeed);
			SetupNoiseGenerator(RoughNoise, ActiveMountain, GlobalSeed + 1);
			SetupNoiseGenerator(SpaghettiNoise, ActiveSpaghetti, GlobalSeed + 2);
			SetupNoiseGenerator(CheeseNoise, ActiveCheese, GlobalSeed + 3);
			SetupNoiseGenerator(EntranceNoise, ActiveEntrance, GlobalSeed + 4);

			int64 GlobalOriginX = FMath::RoundToInt64(ChunkWorldPos.X / LocalVoxelSize);
			int64 GlobalOriginY = FMath::RoundToInt64(ChunkWorldPos.Y / LocalVoxelSize);
			int64 GlobalOriginZ = FMath::RoundToInt64(ChunkWorldPos.Z / LocalVoxelSize);

			auto SmoothMin = [](float a, float b, float k) {
				float h = FMath::Clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
				return FMath::Lerp(b, a, h) - k * h * (1.0f - h);
				};

			auto SmoothMax = [](float a, float b, float k) {
				float h = FMath::Clamp(0.5f - 0.5f * (b - a) / k, 0.0f, 1.0f);
				return FMath::Lerp(b, a, h) + k * h * (1.0f - h);
				};

			for (int32 Y = 0; Y < Size; Y++)
			{
				for (int32 X = 0; X < Size; X++)
				{
					float GlobalX = static_cast<float>((GlobalOriginX + X) * LocalVoxelSize);
					float GlobalY = static_cast<float>((GlobalOriginY + Y) * LocalVoxelSize);

					float BaseVal = (BaseNoise.GetNoise(GlobalX, GlobalY) + 1.0f) * 0.5f;
					float MountVal = (RoughNoise.GetNoise(GlobalX, GlobalY) + 1.0f) * 0.5f;

					float TargetZ = FMath::Lerp(ActiveBaseHeight.MinMax.X, ActiveBaseHeight.MinMax.Y, BaseVal);
					TargetZ += MountVal * ActiveMountain.MinMax.Y * FMath::Pow(BaseVal, 2.0f);

					LocalTerrainHeights[X + Y * Size] = TargetZ;
				}
			}

			for (int32 Z = 0; Z < Size; Z++)
			{
				for (int32 Y = 0; Y < Size; Y++)
				{
					for (int32 X = 0; X < Size; X++)
					{
						float GlobalX = static_cast<float>((GlobalOriginX + X) * LocalVoxelSize);
						float GlobalY = static_cast<float>((GlobalOriginY + Y) * LocalVoxelSize);
						float GlobalZ = static_cast<float>((GlobalOriginZ + Z) * LocalVoxelSize);

						float TargetZ = LocalTerrainHeights[X + Y * Size];
						float TerrainSDF = TargetZ - GlobalZ;
						float FinalDensity = TerrainSDF;

						if (TerrainSDF > -1500.0f)
						{
							float CheeseRaw = CheeseNoise.GetNoise(GlobalX, GlobalY, GlobalZ);
							float SpagRaw = SpaghettiNoise.GetNoise(GlobalX, GlobalY, GlobalZ);

							float TargetRadius = 800.0f;
							float WaveY = FMath::Sin(GlobalX * 0.0005f) * 3000.0f;
							float WaveZ = 500.0f + FMath::Cos(GlobalX * 0.0003f) * 800.0f;

							float DistY = GlobalY - WaveY;
							float DistZ = GlobalZ - WaveZ;
							float DistanceToLine = FMath::Sqrt((DistY * DistY) + (DistZ * DistZ));

							float BaseCaveSDF = DistanceToLine - TargetRadius;
							float MicroDistortion = SpagRaw * FMath::Abs(ActiveSpaghetti.MinMax.Y);
							float GraphCaveSDF = BaseCaveSDF + MicroDistortion;

							float NormalizedCheese = (CheeseRaw + 1.0f) * 0.5f;
							float CheeseAlpha = FMath::Clamp((NormalizedCheese - 0.7f) / 0.3f, 0.0f, 1.0f);
							float CheeseSDF = FMath::Lerp(ActiveCheese.MinMax.X, ActiveCheese.MinMax.Y, CheeseAlpha);

							float CaveToolSDF = SmoothMin(CheeseSDF, GraphCaveSDF, 300.0f);

							float EntranceVal = (EntranceNoise.GetNoise(GlobalX, GlobalY) + 1.0f) * 0.5f;
							float EntranceMask = FMath::Clamp(FMath::Lerp(ActiveEntrance.MinMax.X, ActiveEntrance.MinMax.Y, EntranceVal), 0.0f, 1.0f);

							float SurfaceProtect = 0.0f;
							if (TerrainSDF < 800.0f)
							{
								SurfaceProtect = FMath::Clamp(1.0f - (TerrainSDF / 800.0f), 0.0f, 1.0f);
							}

							CaveToolSDF += SurfaceProtect * 1500.0f * (1.0f - EntranceMask);
							FinalDensity = SmoothMin(TerrainSDF, CaveToolSDF, 250.0f);
						}

						if (GlobalZ < ZeroDownLevel)
						{
							float BedrockSDF = ZeroDownLevel - GlobalZ;
							FinalDensity = SmoothMax(FinalDensity, BedrockSDF, 200.0f);
						}

						float SafeRange = LocalVoxelSize * 6.0f;
						FinalDensity = FMath::Clamp(FinalDensity, -SafeRange, SafeRange);

						if (FMath::Abs(FinalDensity) < 0.001f)
						{
							FinalDensity = (FinalDensity < 0.0f) ? -0.001f : 0.001f;
						}

						int32 Idx = X + (Y * Size) + (Z * Size * Size);
						LocalDensities[Idx] = FinalDensity;
					}
				}
			}

			for (int32 Z = 0; Z < GridSize; Z++)
			{
				for (int32 Y = 0; Y < GridSize; Y++)
				{
					for (int32 X = 0; X < GridSize; X++)
					{
						MarchCubeThreadSafe(X, Y, Z, ZeroDownLevel, ZeroUpLevel, LocalVoxelSize, LocalDensities, LocalTerrainHeights,
							LocalSectionData, BaseWorldConfig, LocalExtraNoises, ChunkWorldPos, ThreadLocalNoiseMap);
					}
				}
			}

			AsyncTask(ENamedThreads::GameThread, [this, TargetMaterial, MovedDensities = MoveTemp(LocalDensities), MovedSectionData = MoveTemp(LocalSectionData)]() mutable
				{
					if (!IsValid(this)) return;
					this->VoxelDensities = MoveTemp(MovedDensities);
					ProceduralMesh->ClearAllMeshSections();

					if (MovedSectionData.Vertices.Num() > 0)
					{
						UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
							MovedSectionData.Vertices,
							MovedSectionData.Triangles,
							MovedSectionData.UVs,
							MovedSectionData.Normals,
							MovedSectionData.Tangents
						);

						ProceduralMesh->CreateMeshSection_LinearColor(
							0,
							MovedSectionData.Vertices,
							MovedSectionData.Triangles,
							MovedSectionData.Normals,
							MovedSectionData.UVs,
							MovedSectionData.UV1s,
							TArray<FVector2D>(),
							TArray<FVector2D>(),
							MovedSectionData.Colors,
							MovedSectionData.Tangents,
							true);

						if (TargetMaterial)
						{
							ProceduralMesh->SetMaterial(0, TargetMaterial);
						}
					}
					bIsGenerating.store(false);
				});
		});
}