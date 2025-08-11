#include "tracer.h"

#include <iomanip>

#include "dejaview/ext/base/file_utils.h"
#include "dejaview/ext/base/scoped_mmap.h"
#include "dejaview/ext/base/string_splitter.h"
#include "dejaview/tracing/internal/track_event_internal.h"

#include "protos/dejaview/trace/track_event/process_descriptor.pbzero.h"
#include "protos/dejaview/trace/track_event/thread_descriptor.pbzero.h"
#include "protos/dejaview/trace/track_event/source_location.pbzero.h"
#include "protos/dejaview/trace/track_event/track_descriptor.pbzero.h"
#include "protos/dejaview/trace/track_event/track_event.pbzero.h"
#include "protos/dejaview/trace/qemu/qemu_info.pbzero.h"

#include "dwarf/elf.h"

#include "qemu_helpers.h"

using Trace = dejaview::protos::pbzero::Trace;
using TracePacket = dejaview::protos::pbzero::TracePacket;
using TrackEvent_Type = dejaview::protos::pbzero::TrackEvent_Type;

Tracer::Tracer(std::string destPath, std::string kernelPath, std::string startingFrom, uint64_t minInsns)
    : m_destPath(destPath), m_kernelPath(kernelPath), m_symbolizer(),
      m_vmi(), m_minInsns(minInsns) {
  dejaview::base::ScopedMmap kernel_mmap = dejaview::base::ReadMmapWholeFile(kernelPath.c_str());
  if (!kernel_mmap.IsValid()) {
    QEMU_LOG() << "Error: Failed to read file: " << kernelPath << std::endl;
    exit(1);
  }
  std::string_view kernel_view(static_cast<char *>(kernel_mmap.data()),
                               kernel_mmap.length());
  ElfFile elf(kernel_view);

  m_symbolizer.Init(elf);
  if (m_vmi.Init(elf, &m_symbolizer) < 0) {
    QEMU_LOG() << "Virtual Machine Introspection failed to find some symbols. "
               << "Expect process lookup to fail." << std::endl;
  }
  if (!startingFrom.empty()) {
    m_startingFrom = m_symbolizer.lookupSymbol(startingFrom);
    if (m_startingFrom) {
      m_inhibited = true;
    } else {
      QEMU_LOG() << startingFrom << " not found" << std::endl;
    }
  }

  protos = new protozero::HeapBuffered<Trace>();
  auto* packet = (*protos)->add_packet();
  packet->set_trusted_packet_sequence_id(0);
  packet->set_incremental_state_cleared(true);
  packet->set_first_packet_on_sequence(true);

  StoreQemuInfo();
}

void Tracer::StoreQemuInfo() {
  auto* packet = (*protos)->add_packet();
  auto* qemu_info = packet->set_qemu_info();

  // Store the current-working-directory
  char *cwd = getcwd(NULL, 0);
  qemu_info->set_record_cwd(cwd);
  free(cwd);

  // And the QEMU command line
  std::string cmdline;
  std::vector<std::string> replay_cmd;
  if (dejaview::base::ReadFile("/proc/self/cmdline", &cmdline)) {
    dejaview::base::StringSplitter splitter(std::move(cmdline), '\0');
    while (splitter.Next()) {
      std::string token(splitter.cur_token(), splitter.cur_token_size());
      qemu_info->add_record_cmd(token);
    }
  }
}

uint64_t Tracer::GetTrackUuid() {
  static uint64_t ret = 0;

  if (m_vmi.IsProcessInvalidated()) {
    uint32_t tgid = 0, pid = 0;
    std::string comm("Boot");
    m_vmi.GetProcessInfo(tgid, pid, comm);

    auto it = pid_to_uuid.find(pid);
    if (it == pid_to_uuid.end()) {
      ret = pid_to_uuid.size() + 1;

      auto* packet = (*protos)->add_packet();
      auto* desc = packet->set_track_descriptor();
      desc->set_uuid(ret);
      auto *process = desc->set_process();
      process->set_pid(static_cast<int32_t>(tgid));
      process->set_process_name(comm);

  /*
      auto* packet = (*protos)->add_packet();
      tp.track_descriptor.uuid = uuid64()
      TODO: generate the other uuid randomly too
      tp.track_descriptor.parent_uuid = proc_uuid
      tp.track_descriptor.thread.pid = proc.pid
      tp.track_descriptor.thread.tid = thread.tid
      tp.track_descriptor.thread.thread_name = "Thread"
  */

      pid_to_uuid.insert(std::make_pair(pid, ret));
    } else {
      ret = it->second;
    }
  }

  return ret;
}

uint64_t Tracer::GetFunctionUuid(dejaview::protos::pbzero::TracePacket *packet, uint64_t addr) {
  uint64_t ret;

  auto it = addr_to_uuid.find(addr);
  if (it == addr_to_uuid.end()) {
    ret = addr_to_uuid.size() + 1;
    auto* interned_data = packet->set_interned_data();

    std::string function_name, file_name;
    int line_number;
    if (m_symbolizer.lookupAddress(addr, function_name, file_name, line_number)) {
      auto* source_location = interned_data->add_source_locations();
      source_location->set_iid(ret);
      // Let's skip this since it's redundant with the slice name
      // source_location->set_function_name(function_name);
      source_location->set_file_name(file_name);
      source_location->set_line_number(static_cast<uint32_t>(line_number));

      auto* event_name = interned_data->add_event_names();
      event_name->set_iid(ret);
      event_name->set_name(function_name);
    } else {
      auto* event_name = interned_data->add_event_names();
      event_name->set_iid(ret);

      char *name;
      asprintf(&name, "0x%lX", addr);
      event_name->set_name(name);
    }

    addr_to_uuid.insert(std::make_pair(addr, ret));
  } else {
    ret = it->second;
  }

  return ret;
}

void Tracer::WriteToDisk() {
  QEMU_LOG() << "Saving to " << m_destPath << "...     ";

  const auto dest_fd = dejaview::base::OpenFile(m_destPath, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (dest_fd.get() == -1) {
    QEMU_LOG() << "\nFailed to open destination file; can't write trace.\n";
    return;
  }

  size_t queue_size = m_queue.size();
  size_t i = 0;
  int last_percentage = -1;
  while (!m_queue.empty()) {
      i++;
      int percentage = static_cast<int>((static_cast<double>(i) / static_cast<double>(queue_size)) * 100);
      if (percentage != last_percentage) {
        last_percentage = percentage;
        QEMU_LOG() << "\b\b\b\b" << std::setw(3) << percentage << "%";
      }

      tracing_event e = m_queue.front();

      // Create an event for the timeline
      auto* packet = (*protos)->add_packet();
      packet->set_timestamp(e.ts);
      packet->set_trusted_packet_sequence_id(0);
      uint64_t call_uuid = 0;
      if (e.addr)
        call_uuid = GetFunctionUuid(packet, e.addr);

      auto* event = packet->set_track_event();
      event->add_category_iids(1);
      event->set_track_uuid(e.track_uuid);
      if (e.addr) {
        event->set_name_iid(call_uuid);
        // TODO: skip if failed to lookup event->set_source_location_iid(call_uuid);
      }
      // TODO: Extract arguments and return value

      event->set_type(e.addr ? TrackEvent_Type::TYPE_SLICE_BEGIN
                             : TrackEvent_Type::TYPE_SLICE_END);

      m_queue.pop_front();
  }

  (*protos)->Finalize();
  auto serialized = protos->SerializeAsString();

  QEMU_LOG() << "\n";
  const auto exported_data =
      dejaview::base::WriteAll(dest_fd.get(), serialized.data(), serialized.size());
  if (exported_data <= 0) {
    QEMU_LOG() << "Failed to write trace to disk.\n";
    return;
  }
}
