// sync_session.h
//
// The sensor side of the BLE sync protocol, schema/PROTOCOL.md §3, as a state
// machine with no BLE dependency. A transport adapter feeds it Control writes
// and pulls Status and Data payloads to notify. This is the seam the tests
// and the simulator exercise.
//
//   phone writes Control  -> handleControl() -> Status to notify
//   after READ_FROM       -> nextChunk() until it returns false
//   on connect            -> encodeDeviceInfo() for the Device Info read
//   while advertising     -> encodeAdvertising() for the manufacturer data
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "telemetry/ble_codec.h"
#include "telemetry/ring_store.h"
#include "telemetry/system.h"

namespace telemetry {

class SyncSession {
public:
    struct Stats {
        uint32_t commands         = 0;
        uint32_t bad_commands     = 0;
        uint32_t chunks           = 0;
        uint32_t records_streamed = 0;
        uint32_t sessions_opened  = 0;
    };

    SyncSession(RingStore& store, ISystem& system);

    // Handles one Control write. Always fills `status` for the notification.
    void handleControl(const uint8_t* in, size_t len, ble::Status& status);

    // Fills the next Data chunk for the READ_FROM in progress. `max_len` is
    // the notification payload limit. Returns false when there is nothing to
    // send. The final chunk of a read carries ble::kChunkLast.
    bool nextChunk(uint8_t* out, size_t max_len, size_t& len);

    bool streaming() const { return stream_active_; }

    void fillDeviceInfo(ble::DeviceInfo& out);
    void encodeDeviceInfo(uint8_t* out);

    void fillAdvertising(ble::Advertising& out);
    void encodeAdvertising(uint8_t* out);

    // The link dropped: forget the session and any stream in progress.
    void onDisconnect();

    bool sessionOpen() const { return session_open_; }
    const uint8_t* sessionId() const { return session_id_; }
    const Stats& stats() const { return stats_; }

private:
    void openSession(const ble::Command& cmd, ble::Status& status);
    void setTime(const ble::Command& cmd, ble::Status& status);
    void readFrom(const ble::Command& cmd, ble::Status& status);
    void ackCollected(const ble::Command& cmd, ble::Status& status);
    void ackSecured(const ble::Command& cmd, ble::Status& status);
    void closeSession(ble::Status& status);
    bool moreToStream();

    RingStore& store_;
    ISystem&   system_;
    Stats      stats_;

    bool    session_open_ = false;
    uint8_t session_id_[ble::kSessionIdSize] = {0};

    bool     stream_active_    = false;
    uint32_t stream_next_seq_  = 0;
    uint32_t stream_remaining_ = 0;  // 0 while unlimited
    bool     stream_unlimited_ = false;
};

}  // namespace telemetry
