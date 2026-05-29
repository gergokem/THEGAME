# TheGame

## Description
TheGame is an Unreal Engine project developed in C++ that features procedural terrain generation. It utilizes the Marching Cubes algorithm and FastNoiseLite to dynamically generate and render complex, volumetric terrain, including mountains, caves, and different biomes.

## Features
- **Procedural Terrain Generation:** Uses the Marching Cubes algorithm to create smooth, volumetric terrain.
- **Noise-Driven Environments:** Integrates `FastNoiseLite` for various noise types (Perlin, Cellular, OpenSimplex2) to shape the terrain, mountains, and caves.
- **Dynamic Biomes:** Supports biome blending and dynamic texturing based on height, depth, and noise parameters.
- **Asynchronous Generation:** Generates chunk meshes asynchronously to maintain performance and avoid locking the main game thread.
- **Cave Systems:** Includes complex cave generation ("Spaghetti" and "Cheese" caves) with entrance modulators.

## Installation / How to run

1. Open the project in Unreal Engine.
2. Compile the C++ code if necessary.
3. To generate the terrain in your level:
   - Spawn the `TerrainChunk` actor into the world.
   - In your Blueprint (e.g., Level Blueprint or a Manager Actor), get a reference to the spawned `TerrainChunk`.
   - Call the `GenerateChunkAsync` node on the `TerrainChunk`.
   - Fill in the required parameters for the node, particularly the `WorldConfigDataAsset` which defines the biomes, materials, and noise settings.

## Dependencies
- Unreal Engine (Core, CoreUObject, Engine, InputCore)
- ProceduralMeshComponent (Unreal Engine Plugin)
- FastNoiseLite (included in the source)

## License
See the LICENSE file for details.
