#include "engine/voxel/snapshot_writer.hpp"
#include <stdexcept>
#include <utility>

namespace vc::voxel {
SnapshotWriter::SnapshotWriter(WorldStore& store):store_(store),thread_([this](std::stop_token stop){run(stop);}) {}
bool SnapshotWriter::request(World world,BlockRegistry registry,WorldMetadata metadata,std::string gameData) {
    std::lock_guard lock(mutex_);if(busy_) return false;
    request_.emplace(Job{std::move(world),std::move(registry),metadata,std::move(gameData)});busy_=true;wake_.notify_one();return true;
}
bool SnapshotWriter::busy() const { std::lock_guard lock(mutex_);return busy_; }
std::optional<SnapshotResult> SnapshotWriter::takeReady() {
    std::lock_guard lock(mutex_);if(!ready_) return {};
    auto result=std::move(ready_);ready_.reset();busy_=false;return result;
}
SnapshotResult SnapshotWriter::wait() {
    std::unique_lock lock(mutex_);if(!busy_) throw std::logic_error("No pending snapshot to wait for");
    wake_.wait(lock,[this]{return ready_.has_value();});
    auto result=std::move(*ready_);ready_.reset();busy_=false;return result;
}
void SnapshotWriter::run(std::stop_token stop) {
    for(;;) {
        std::optional<Job> job;
        {
            std::unique_lock lock(mutex_);(void)wake_.wait(lock,stop,[this]{return request_.has_value();});
            // An accepted request is completed even if destruction requests stop.
            if(!request_) return;
            job=std::move(request_);request_.reset();
        }
        SnapshotResult result;
        try { result.revision=store_.save(job->world,job->registry,job->metadata,job->gameData); }
        catch(...) { result.failure=std::current_exception(); }
        {
            std::lock_guard lock(mutex_);ready_=std::move(result);wake_.notify_all();
        }
        if(stop.stop_requested()) return;
    }
}
}
