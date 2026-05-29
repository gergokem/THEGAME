// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldConfigDataAsset.h"
#include "Math/UnrealMathUtility.h"

float UBiomeDataAsset::CalculateBiomeWeight(const TMap<FName, float>& CurrentWorldNoises) const
{
	// 1. Проверка на Сверх-приоритет (Override режимов игры)
	if (SuperPriority > 0.0f)
	{
		return SuperPriority * 1000.0f; // Гарантированно перебивает всё остальное
	}

	// Если у биома вообще нет требований (BStats пуст) - он базовый, вес минимальный
	if (BStats.Num() == 0)
	{
		return 0.01f;
	}

	float TotalWeight = 1.0f;

	// 2. Жесткая отсечка и расчет приоритета по каждому требованию биома
	for (const auto& StatPair : BStats)
	{
		FName NoiseName = StatPair.Key;
		FVector2D MinMax = StatPair.Value;

		// Если генератор мира не передал такой шум (ошибка настроек) - биом не спавнится
		if (!CurrentWorldNoises.Contains(NoiseName))
		{
			return 0.0f;
		}

		float CurrentNoiseValue = CurrentWorldNoises[NoiseName];

		// ЖЕСТКАЯ ОТСЕЧКА: Если шум выходит за пределы MinMax - биом мгновенно умирает (вес 0)
		if (CurrentNoiseValue < MinMax.X || CurrentNoiseValue > MinMax.Y)
		{
			return 0.0f;
		}

		// РАСЧЕТ ПРИОРИТЕТА: Насколько мы близки к центру желаемого диапазона?
		float Range = MinMax.Y - MinMax.X;
		if (Range <= 0.0f) continue; // Защита от деления на ноль

		float Center = MinMax.X + (Range * 0.5f);
		// Вычисляем дистанцию до центра (от 0 до 1, где 0 - идеальный центр)
		float DistanceFromCenter = FMath::Abs(CurrentNoiseValue - Center) / (Range * 0.5f);

		// Вес конкретно этого параметра: 1.0 в центре, падает до 0.0 к краям
		float ParamWeight = 1.0f - DistanceFromCenter;

		// Перемножаем веса всех параметров (Температура * Влажность * ...)
		TotalWeight *= ParamWeight;
	}

	return TotalWeight;
}