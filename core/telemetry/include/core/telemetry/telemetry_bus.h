#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "core/telemetry/telemetry_snapshot.h"

namespace core::telemetry {

class TelemetryBus {
 public:
  using SnapshotPtr = std::shared_ptr<const TelemetrySnapshot>;
  using Callback = std::function<void(SnapshotPtr)>;

  TelemetryBus();
  ~TelemetryBus();

  TelemetryBus(const TelemetryBus&) = delete;
  TelemetryBus& operator=(const TelemetryBus&) = delete;

  void Publish(std::shared_ptr<TelemetrySnapshot> snapshot);

  [[nodiscard]] SnapshotPtr Latest() const noexcept;
  [[nodiscard]] std::uint64_t PublishCount() const noexcept;

  [[nodiscard]] std::uint64_t Subscribe(Callback cb);
  void Unsubscribe(std::uint64_t id);
  void ClearSubscriptions();

 private:
  struct Sub {
    std::uint64_t id = 0;
    Callback cb;
  };

  std::atomic<SnapshotPtr> latest_{};
  std::atomic<std::uint64_t> publish_count_{0};

  mutable std::mutex subs_mu_;
  std::vector<Sub> subs_;
  std::atomic<std::uint64_t> next_sub_id_{1};
};

}  // namespace core::telemetry
