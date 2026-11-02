#include "examples/core_modules/layered_generator.hpp"
#include "engine/voxel/terrain_worker.hpp"
#include "engine/voxel/mesher.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        using namespace vc::voxel;
        BlockRegistry blocks;
        const auto layer = blocks.add({"example:layer", true, {0.4f, 0.6f, 0.8f}});
        const WorldBounds bounds{-17, 13};
        World world(bounds);
        using SelectedGenerator = vc::examples::LayeredGenerator;
        BasicTerrainWorker<SelectedGenerator> worker(SelectedGenerator(bounds, layer, -3), 2);
        const std::array positions{Position{-1, -1, -1}, Position{0, -1, -1}};
        for (const auto p : positions) if (!worker.request(p)) throw std::runtime_error("Queue rejected work");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (world.chunks().size() < positions.size()) {
            if (auto chunk = worker.takeReady()) world.insert(std::move(*chunk));
            else {
                if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Generation timed out");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        if (world.block({-1, -3, -1}) != layer || world.block({0, -3, -1}) != layer ||
            world.block({-1, -2, -1}) != air) throw std::runtime_error("Wrong generated terrain");
        const vc::AtlasLayout atlas(16, 1, 1);
        std::size_t triangles = 0;
        for (const auto& [p, chunk] : world.chunks()) {
            (void)p;
            triangles += meshChunk(chunk, world, blocks, atlas).indices.size() / 3;
        }
        if (!triangles) throw std::runtime_error("Generated terrain did not mesh");
        std::cout << "Layered core module: " << world.chunks().size() << " chunks, " << triangles << " triangles\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
