# Compile-time core extension: generation jobs

`engine/voxel/terrain_worker.hpp` defines the C++23 `ChunkGenerator` concept and
`BasicTerrainWorker<Generator>`. The policy must be move-constructible and provide
`Chunk generate(Position) const`. It is held by value and called directly by one
worker, with no virtual dispatch, per-voxel callback or type-erased policy. The
default `TerrainWorker` is an alias for `BasicTerrainWorker<TerrainGenerator>`;
its explicit instantiation remains in `terrain_worker.cpp`.

The existing bounded scheduling algorithm is shared: validate chunk coordinates,
deduplicate requests, count ready/in-flight work against capacity, publish owned
chunks, report exceptions on the caller, stop/wake/join before destroying data.
A policy must return the requested chunk position; mismatched results now fail on
the caller instead of leaving the request pending forever. `World` remains owned
by the main thread. A running policy is not forcibly interrupted mid-chunk; it must
finish bounded work for prompt shutdown.

## Executed alternative

`examples/core_modules/layered_generator.hpp` provides real independent terrain:
fill a configurable material below a surface within finite vertical bounds.
It uses namespaced host-registered content, handles negative chunk coordinates,
and allocates no per-cell objects. Select it at the composition site:

```cpp
using Generator = vc::examples::LayeredGenerator;
vc::voxel::BasicTerrainWorker<Generator> worker(
    Generator(bounds, material, -3), 2);
worker.request({-1, -1, -1});
// Poll takeReady() and move completed chunks into the main-thread World.
```

With testing enabled, build/run the standalone engine-only example:

```sh
cmake --preset server
cmake --build --preset server --target vc_core_module_example vc_core_module_tests
ctest --test-dir build/server -R '^CoreModule\.' --output-on-failure --no-tests=error
./build/server/vc_core_module_example
```

The executable actually generates two adjacent negative-coordinate chunks and
meshes them through the unchanged engine mesher, checking block content and
nonempty geometry. `CoreModule` tests also check finite/non-aligned height bounds,
capacity/deduplication/reuse, move-only policy ownership, error propagation and
wrong-position rejection. Existing terrain worker tests exercise the default
versioned generator. The TSan preset includes the new cases.

## Scope and compatibility

This extension selects the generation **job implementation**, not the complete
game terrain contract. The playable client/server continue to use the unchanged
`TerrainGenerator` and `ChunkStreamer`: spawn, retained-terrain sampling and
residency use its `sample`, `surfaceY`, `version` and version-specific surface band.
Replacing generation in a full game session must also supply those semantics and
a persisted generator identity/migration plan. Do not reopen existing worlds with
the example pretending to be terrain version 1–3. The example writes no saves.

Consumers can instantiate an alternative from their own source/header and rebuild
their composition target; the central queue/World/mesher algorithms need no edits.
Default clients do not instantiate or link the example policy. This is a C++
source extension, not a runtime mod, ABI, dynamic module loader or native SDK.
Other systems remain concrete until a real replacement need justifies a seam.
