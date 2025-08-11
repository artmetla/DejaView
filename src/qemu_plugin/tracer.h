#ifndef SRC_QEMU_PLUGIN_TRACER_H_
#define SRC_QEMU_PLUGIN_TRACER_H_

#include <cinttypes>

#include <string>
#include <unordered_map>
#include <deque>
#include <stack>

#include "symbolizer.h"
#include "dejaview/protozero/scattered_heap_buffer.h"
#include "protos/dejaview/trace/trace.pbzero.h"
#include "protos/dejaview/trace/trace_packet.pbzero.h"
#include "qemu_helpers.h"

#include "vmi.h"

class Tracer {
public:
  Tracer(std::string destPath, std::string kernelPath, std::string startingFrom, uint64_t minInsns);

  inline void LogCall(uint64_t addr, uint64_t /*vcpu_id*/, uint64_t ts) {
    if (m_inhibited) {
      if (addr == m_startingFrom) {
        m_inhibited = false;
      } else {
        return;
      }
    }
    uint64_t track_uuid = GetTrackUuid();
    // When context switching
    if (track_uuid != m_prevTrackUuid) {
      // Close all slices of the previous track
      for (uint64_t _ : track_backtrace[m_prevTrackUuid]) {
        tracing_event e = {};
        e.addr = 0;
        e.ts = ts;
        e.track_uuid = m_prevTrackUuid;
        m_queue.push_back(e);
      }
      // And re-open all slices of the current track
      for (uint64_t parent : track_backtrace[track_uuid]) {
        tracing_event e = {};
        e.addr = parent;
        e.ts = ts;
        e.track_uuid = track_uuid;
        m_queue.push_back(e);
      }

      m_prevTrackUuid = track_uuid;
    }

    track_backtrace[track_uuid].push_back(addr);
    tracing_event e = {};
    e.addr = addr;
    e.ts = ts;
    e.track_uuid = track_uuid;
    m_queue.push_back(e);
    m_vmi.LogCall(addr);
  }

  inline void LogRet(uint64_t /*vcpu_id*/, uint64_t ts) {
    if (m_inhibited) {
        return;
    }
    uint64_t track_uuid = GetTrackUuid();
    if (track_backtrace[track_uuid].empty()) {
      // No slice left to close
      return;
    }
    auto last_event = m_queue.back();
    if (last_event.addr &&
      last_event.track_uuid == track_uuid &&
      ts - last_event.ts < m_minInsns) {
      m_queue.pop_back();
    } else {
      tracing_event e = {};
      e.addr = 0;
      e.ts = ts;
      e.track_uuid = track_uuid;
      m_queue.push_back(e);
    }
    track_backtrace[track_uuid].pop_back();
  }
  void WriteToDisk();

private:
  void StoreQemuInfo();
  uint64_t GetTrackUuid();
  uint64_t GetFunctionUuid(dejaview::protos::pbzero::TracePacket *packet, uint64_t addr);

  std::string m_destPath;
  std::string m_kernelPath;
  uint64_t m_startingFrom;
  bool m_inhibited;
  Symbolizer m_symbolizer;
  VMI m_vmi;
  protozero::HeapBuffered<dejaview::protos::pbzero::Trace> *protos;
  std::unordered_map<uint64_t, uint64_t> pid_to_uuid;
  std::unordered_map<uint64_t, uint64_t> addr_to_uuid;
  std::unordered_map<uint64_t, std::vector<uint64_t>> track_backtrace;

  struct tracing_event {
    uint64_t addr;
    uint64_t ts;
    uint64_t track_uuid;
  };
  std::deque<tracing_event> m_queue;
  uint64_t m_minInsns;
  uint64_t m_prevTrackUuid;
};

#endif  // SRC_QEMU_PLUGIN_TRACER_H_
