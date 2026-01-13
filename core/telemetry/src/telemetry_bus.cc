#include "core/telemetry/telemetry_bus.h"

#include <algorithm>
#include <utility>

namespace core::telemetry {

TelemetryBus::TelemetryBus() {
  latest_.store(SnapshotPtr{}, std::memory_order_release);
}

TelemetryBus::~TelemetryBus() = default;

void TelemetryBus::Publish(std::shared_ptr<TelemetrySnapshot> snapshot) {
  if (!snapshot) return;

  SnapshotPtr frozen = std::const_pointer_cast<const TelemetrySnapshot>(std::move(snapshot));
  latest_.store(std::move(frozen), std::memory_order_release);
  publish_count_.fetch_add(1, std::memory_order_relaxed);

  std::vector<Callback> callbacks;
  {
    std::lock_guard<std::mutex> lk(subs_mu_);
    callbacks.reserve(subs_.size());
    for (const auto& s : subs_) {
      if (s.cb) callbacks.push_back(s.cb);
    }
  }

  SnapshotPtr cur = latest_.load(std::memory_order_acquire);
  if (!cur) return;

  for (auto& cb : callbacks) {
    try { cb(cur); } catch (...) {}
  }
}

TelemetryBus::SnapshotPtr TelemetryBus::Latest() const noexcept {
  return latest_.load(std::memory_order_acquire);
}

std::uint64_t TelemetryBus::PublishCount() const noexcept {
  return publish_count_.load(std::memory_order_relaxed);
}

std::uint64_t TelemetryBus::Subscribe(Callback cb) {
  if (!cb) return 0;
  const std::uint64_t id = next_sub_id_.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard<std::mutex> lk(subs_mu_);
  subs_.push_back(Sub{.id = id, .cb = std::move(cb)});
  return id;
}

void TelemetryBus::Unsubscribe(std::uint64_t id) {
  if (id == 0) return;
  std::lock_guard<std::mutex> lk(subs_mu_);
  subs_.erase(std::remove_if(subs_.begin(), subs_.end(),
                             [id](const Sub& s) { return s.id == id; }),
              subs_.end());
}

void TelemetryBus::ClearSubscriptions() {
  std::lock_guard<std::mutex> lk(subs_mu_);
  subs_.clear();
}

}  // namespace core::telemetry
