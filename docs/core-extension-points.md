# Compile-time core meshing module

Milestone 1 implements one extension point in the real CPU meshing pipeline.
`ChunkMesher` in `engine/voxel/mesher.hpp` requires this exact operation:

```cpp
static vc::Mesh build(const vc::voxel::Chunk&, const vc::voxel::World&,
    const vc::voxel::BlockRegistry&, const vc::AtlasLayout&,
    const vc::voxel::Lighting*);
```

The module reads immutable inputs, returns owning CPU mesh data and retains no
borrowed references. The call is static; no virtual/function-pointer dispatch or
policy allocation was added. A policy must be safe to call on the meshing worker;
shared mutable globals require its own synchronization and are best avoided.
Exceptions are caught by the existing worker failure channel and rethrown to its
owner. Mesh origins, face winding, atlas/light semantics and translucent ordering
remain obligations of the implementation. The concept checks types, not geometry.

`BasicChunkMeshQueue<M>`, `BasicMeshWorker<M>` and
`BasicBackgroundMeshQueue<M>` reuse budget, near-first scheduling, immutable
snapshots, invalidation tickets, eviction, failure propagation and worker teardown.
The old `ChunkMeshQueue`, `MeshWorker` and `BackgroundMeshQueue` names remain
greedy aliases for source compatibility. Built-in greedy/culled specializations
are explicitly instantiated once in `vc_meshing`. A custom policy's template
definitions are available from the headers; no central queue edit is needed.

`GreedyChunkMesher` calls the original greedy opaque-face algorithm.
`CulledChunkMesher` calls its existing reference/unit-face path. Both retain the
existing nonopaque/translucent handling. `meshChunk(..., bool greedy)` remains
available to old consumers and equivalence benchmarks.

## Select the actual client implementation

```sh
cmake --preset debug -DVC_CORE_MESHER=greedy
cmake --build --preset debug --parallel 4
ctest --preset debug

cmake --preset debug -DVC_CORE_MESHER=culled
cmake --build --preset debug --target voxelcraft --parallel 4
# Run graphics verification on a host with a display/device/validation layer.
```

Only `greedy` and `culled` are accepted. The default is `greedy`.
`vc_client_modules` supplies the selection to `client/core_modules.hpp`.
Local startup, synchronous smoke/edit work and both local/remote background
meshing use these host aliases. Switching policy recompiles host consumers;
the voxel, generation, storage, gameplay and server implementations do not need
algorithm edits or a runtime registry. Use a separate build directory to retain
both client variants. The server does not use or configure this client option.

To add a different source module, implement the operation in its own header and
choose it in the host composition alias (and extend the CMake selection if it
should be user-configurable). Link `vc_meshing`; instantiate
`BasicBackgroundMeshQueue<YourMesher>`. No individual voxel operation changes.
This requires rebuilding consuming C++ targets and is not DLL/SO loading.

## Verification obligations

- Existing greedy/reference tests compare surfaces, winding, materials, UVs,
  negative seams, height extremes, transparency and lighting.
- Existing worker tests check immutable neighbors/light, supersession, capacity,
  immediate eviction, failure propagation and stop/join. Greedy aliases retain
  these tests unchanged.
- `CoreModules.ReferencePolicyRunsThroughSynchronousAndBackgroundScheduling`
  instantiates an external reference policy, verifies its actual unit-face
  geometry and tests background results plus eviction of in-flight work.
- `CoreModules.AlternativeWorkerContainsModuleExceptionAndJoins` exercises a
  throwing external module. Static assertions reject a wrong result signature.
- `CoreSelection.greedy` and `.culled` build separate host consumers and verify
  the selected geometry (24 versus 24,576 vertices for an isolated solid chunk).

The reference policy creates substantially more geometry; it is a correctness
alternative, not a promised optimization. Measurements and unavailable GPU checks
are recorded in [progress](modularization-progress.md). World generation remains
concrete, versioned and unchanged; additional core extension points are future
work justified by actual consumers. Native ABI/loader/SDK work is Milestone 3.
