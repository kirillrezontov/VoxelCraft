# Keep sources in place; targets express responsibility and dependency ownership.
find_package(Threads REQUIRED)
add_library(vc_voxel engine/voxel/registry.cpp engine/voxel/world.cpp)
target_include_directories(vc_voxel PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vc_voxel PUBLIC vc_core glm::glm)

add_library(vc_generation engine/voxel/generation.cpp engine/voxel/terrain_worker.cpp)
target_link_libraries(vc_generation PUBLIC vc_voxel Threads::Threads)

add_library(vc_streaming engine/voxel/streaming.cpp)
target_link_libraries(vc_streaming PUBLIC vc_generation)

add_library(vc_lighting engine/voxel/lighting.cpp)
target_link_libraries(vc_lighting PUBLIC vc_voxel)

add_library(vc_meshing engine/voxel/mesher.cpp engine/voxel/mesh_queue.cpp engine/voxel/mesh_worker.cpp)
target_link_libraries(vc_meshing PUBLIC vc_voxel vc_lighting vc_render_types Threads::Threads PRIVATE vc_render_data)

add_library(vc_physics engine/voxel/raycast.cpp engine/voxel/collision.cpp)
target_link_libraries(vc_physics PUBLIC vc_voxel)
