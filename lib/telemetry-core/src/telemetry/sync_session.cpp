#include "telemetry/sync_session.h"

#include <string.h>

namespace telemetry {

using ble::Command;
using ble::Opcode;
using ble::Result;
using ble::Status;

SyncSession::SyncSession(RingStore& store, ISystem& system)
: store_(store), system_(system) {}

static void setStatus(Status& s, uint8_t opcode, Result r, uint32_t seq) {
    s.opcode = opcode;
    s.result = static_cast<uint8_t>(r);
    s.seq    = seq;
}

void SyncSession::handleControl(const uint8_t* in, size_t len, Status& status) {
    stats_.commands++;

    Command cmd;
    ble::CommandError err = ble::decodeCommand(in, len, cmd);
    if (err != ble::CommandError::None) {
        stats_.bad_commands++;
        setStatus(status, len > 0 ? in[0] : 0, Result::BadArgument, 0);
        return;
    }

    if (!session_open_ && cmd.opcode != static_cast<uint8_t>(Opcode::OpenSession)) {
        setStatus(status, cmd.opcode, Result::NoSession, 0);
        return;
    }

    switch (static_cast<Opcode>(cmd.opcode)) {
        case Opcode::OpenSession:  openSession(cmd, status); break;
        case Opcode::SetTime:      setTime(cmd, status); break;
        case Opcode::ReadFrom:     readFrom(cmd, status); break;
        case Opcode::AckCollected: ackCollected(cmd, status); break;
        case Opcode::AckSecured:   ackSecured(cmd, status); break;
        case Opcode::CloseSession: closeSession(status); break;
    }
}

void SyncSession::openSession(const Command& cmd, Status& status) {
    session_open_ = true;
    memcpy(session_id_, cmd.session_id, ble::kSessionIdSize);
    stream_active_ = false;
    stats_.sessions_opened++;
    setStatus(status, cmd.opcode, Result::Ok, store_.cursor().next_seq);
}

void SyncSession::setTime(const Command& cmd, Status& status) {
    if (cmd.unix_time == 0 || !system_.setUnixTime(cmd.unix_time)) {
        setStatus(status, cmd.opcode, Result::BadArgument, 0);
        return;
    }
    setStatus(status, cmd.opcode, Result::Ok, cmd.unix_time);
}

void SyncSession::readFrom(const Command& cmd, Status& status) {
    const RingCursor& c = store_.cursor();
    uint32_t from = cmd.seq < c.tail_seq ? c.tail_seq : cmd.seq;

    stream_active_    = true;
    stream_next_seq_  = from;
    stream_unlimited_ = cmd.max_records == 0;
    stream_remaining_ = cmd.max_records;
    setStatus(status, cmd.opcode, Result::Ok, from);
}

void SyncSession::ackCollected(const Command& cmd, Status& status) {
    if (!store_.ackCollected(cmd.through_seq)) {
        setStatus(status, cmd.opcode, Result::SeqOutOfRange, store_.cursor().collected_through);
        return;
    }
    setStatus(status, cmd.opcode, Result::Ok, store_.cursor().collected_through);
}

void SyncSession::ackSecured(const Command& cmd, Status& status) {
    if (!store_.ackSecured(cmd.through_seq)) {
        setStatus(status, cmd.opcode, Result::SeqOutOfRange, store_.cursor().secured_through);
        return;
    }
    setStatus(status, cmd.opcode, Result::Ok, store_.cursor().secured_through);
}

void SyncSession::closeSession(Status& status) {
    session_open_  = false;
    stream_active_ = false;
    memset(session_id_, 0, sizeof session_id_);
    setStatus(status, static_cast<uint8_t>(Opcode::CloseSession), Result::Ok, store_.cursor().next_seq);
}

void SyncSession::onDisconnect() {
    session_open_  = false;
    stream_active_ = false;
    memset(session_id_, 0, sizeof session_id_);
}

bool SyncSession::moreToStream() {
    if (!stream_unlimited_ && stream_remaining_ == 0) return false;
    Record probe;
    return store_.read(stream_next_seq_, &probe, 1) == 1;
}

bool SyncSession::nextChunk(uint8_t* out, size_t max_len, size_t& len) {
    len = 0;
    if (!stream_active_) return false;

    size_t want = ble::recordsPerChunk(max_len);
    if (want == 0) return false;  // notification too small for even one record
    if (!stream_unlimited_ && stream_remaining_ < want) want = stream_remaining_;

    Record records[ble::kMaxRecordsPerChunk];
    size_t got = want > 0 ? store_.read(stream_next_seq_, records, want) : 0;

    // Pack only the leading run of consecutive seqs so a chunk never hides a
    // gap. The next chunk starts after the gap.
    size_t packed = 0;
    uint8_t* body = out + ble::kChunkHeaderSize;
    for (size_t i = 0; i < got; i++) {
        if (i > 0 && records[i].seq != records[i - 1].seq + 1) break;
        encodeRecord(records[i], body + packed * kRecordSize);
        packed++;
    }

    ble::ChunkHeader h;
    h.first_seq = packed > 0 ? records[0].seq : stream_next_seq_;
    h.count     = static_cast<uint8_t>(packed);
    h.crc       = ble::chunkCrc(body, packed * kRecordSize);

    if (packed > 0) {
        stream_next_seq_ = records[packed - 1].seq + 1;
        if (!stream_unlimited_) stream_remaining_ -= static_cast<uint32_t>(packed);
    } else {
        // Nothing readable at or after the cursor: the stream is exhausted.
        stream_remaining_ = 0;
        stream_unlimited_ = false;
    }

    if (!moreToStream()) {
        h.flags |= ble::kChunkLast;
        stream_active_ = false;
    }

    ble::encodeChunkHeader(h, out);
    len = ble::kChunkHeaderSize + packed * kRecordSize;
    stats_.chunks++;
    stats_.records_streamed += static_cast<uint32_t>(packed);
    return true;
}

void SyncSession::fillDeviceInfo(ble::DeviceInfo& out) {
    const RingCursor& c = store_.cursor();
    out = ble::DeviceInfo();
    system_.deviceId(out.device_id);
    out.boot_id           = system_.bootId();
    out.next_seq          = c.next_seq;
    out.collected_through = c.collected_through;
    out.secured_through   = c.secured_through;
    out.uptime_s          = system_.uptimeSeconds();
    out.unix_time         = system_.unixTime();
    out.dropped           = c.dropped;
    out.battery_mv        = system_.batteryMillivolts();
    uint32_t cap          = store_.capacity();
    out.capacity          = cap > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(cap);
    const char* fw = system_.firmwareVersion();
    memset(out.fw_version, 0, sizeof out.fw_version);
    strncpy(out.fw_version, fw, sizeof out.fw_version);
}

void SyncSession::encodeDeviceInfo(uint8_t* out) {
    ble::DeviceInfo d;
    fillDeviceInfo(d);
    ble::encodeDeviceInfo(d, out);
}

void SyncSession::fillAdvertising(ble::Advertising& out) {
    out = ble::Advertising();
    uint32_t pending = store_.pending();
    out.pending    = pending > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(pending);
    out.battery_mv = system_.batteryMillivolts();
    out.adv_flags  = 0;
    if (system_.unixTime() != 0) out.adv_flags |= ble::kAdvTimeSet;
    if (store_.cursor().dropped != 0) out.adv_flags |= ble::kAdvDropped;
}

void SyncSession::encodeAdvertising(uint8_t* out) {
    ble::Advertising a;
    fillAdvertising(a);
    ble::encodeAdvertising(a, out);
}

}  // namespace telemetry
