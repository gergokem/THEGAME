// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// Добавлено inline, чтобы избежать ошибок линковки (LNK2005) при подключении в несколько .cpp
inline const FVector CubeCorners[8] = {
    FVector(0, 0, 0), // 0
    FVector(1, 0, 0), // 1
    FVector(0, 1, 0), // 2
    FVector(1, 1, 0), // 3
    FVector(0, 0, 1), // 4
    FVector(1, 0, 1), // 5
    FVector(0, 1, 1), // 6
    FVector(1, 1, 1)  // 7
};

inline const int32 EdgeConnection[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Нижняя грань (Z=0)
    {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Верхняя грань (Z=1)
    {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Вертикальные ребра
};

// Структура для базовых ячеек (Regular Cells)
struct FRegularCellData
{
    uint8 GeometryCounts; // Старшие 4 бита: кол-во вершин, Младшие: кол-во треугольников
    uint8 VertexIndices[15]; // Индексы вершин
};

// Структура для ячеек перехода (Transition Cells - для LOD'ов)
struct FTransitionCellData
{
    uint8 GeometryCounts;
    uint8 VertexIndices[36];
};

// ============================================================================
// Экспортируемые таблицы Transvoxel
// ============================================================================

// 1. Базовые ячейки (Regular Cells)
extern const uint8 regularCellClass[256];
extern const FRegularCellData regularCellData[16];
extern const uint16 regularVertexData[256][12];

// 2. Ячейки перехода (Transition Cells)
extern const uint8 transitionCellClass[512];
extern const FTransitionCellData transitionCellData[56];
extern const uint8 transitionCornerData[13];
extern const uint16 transitionVertexData[512][12];