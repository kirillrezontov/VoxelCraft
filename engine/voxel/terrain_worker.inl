namespace vc::voxel {
template<ChunkGenerator Generator>
BasicTerrainWorker<Generator>::BasicTerrainWorker(Generator generator, std::size_t capacity)
    : generator_(std::move(generator)), capacity_(capacity) {
    if (!capacity || capacity > 128) throw std::invalid_argument("Terrain worker capacity must be 1..128");
    thread_ = std::jthread([this](std::stop_token stop) { run(stop); });
}
template<ChunkGenerator Generator>
bool BasicTerrainWorker<Generator>::request(Position position) {
    (void)chunkOrigin(position);
    std::lock_guard lock(mutex_);
    if (failure_) std::rethrow_exception(failure_);
    if (pending_.contains(position) || pending_.size() >= capacity_) return false;
    pending_.insert(position);
    try { requests_.push_back(position); }
    catch (...) { pending_.erase(position); throw; }
    wake_.notify_one();
    return true;
}
template<ChunkGenerator Generator>
std::optional<Chunk> BasicTerrainWorker<Generator>::takeReady() {
    std::lock_guard lock(mutex_);
    if (failure_) std::rethrow_exception(failure_);
    if (ready_.empty()) return std::nullopt;
    auto chunk = std::move(ready_.front()); ready_.pop_front();
    pending_.erase(chunk.position());
    return chunk;
}
template<ChunkGenerator Generator>
std::size_t BasicTerrainWorker<Generator>::pendingCount() const { std::lock_guard lock(mutex_); return pending_.size(); }
template<ChunkGenerator Generator>
WorkMetrics BasicTerrainWorker<Generator>::metrics() const { std::lock_guard lock(mutex_);return metrics_; }
template<ChunkGenerator Generator>
void BasicTerrainWorker<Generator>::run(std::stop_token stop) {
    try {
        while (!stop.stop_requested()) {
            Position position;
            {
                std::unique_lock lock(mutex_);
                if (!wake_.wait(lock, stop, [this] { return !requests_.empty(); })) return;
                position = requests_.front(); requests_.pop_front();
            }
            const auto started=std::chrono::steady_clock::now();
            auto chunk = generator_.generate(position);
            if (chunk.position() != position) throw std::runtime_error("Generator returned a different chunk position");
            if (stop.stop_requested()) return;
            std::lock_guard lock(mutex_);
            ready_.push_back(std::move(chunk));metrics_.record(started);
        }
    } catch (...) {
        std::lock_guard lock(mutex_); failure_ = std::current_exception();
    }
}
}
