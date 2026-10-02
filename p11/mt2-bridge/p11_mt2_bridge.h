// SPDX-License-Identifier: Apache-2.0
// Magic Trackpad 2 Bluetooth -> standard HID multitouch compatibility bridge.
// Independently implemented using the protocol documented by Linux hid-magicmouse.c, commit
// 9d7b18668956c411a422d04c712994c5fdb23a4b (see README.md for attribution).
#pragma once

#include <linux/input.h>
#include <linux/uhid.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace p11_mt2 {
constexpr size_t kSlots = 16;
constexpr size_t kContactBytes = 11;
constexpr size_t kReportBytes = 3 + kSlots * kContactBytes;
constexpr char kProperty[] = "persist.bluetooth.p11_mt2_bridge";
constexpr uint8_t kEnableMultitouch[] = {0xf1, 0x02, 0x01};

// Contact count includes all 16 explicitly reported IDs, including lifted ones.
// Sending every ID every frame prevents stuck contacts when an Apple frame omits
// a previously active ID. The generic 4.19 MT class does not imply DROP_UNUSED.
inline std::vector<uint8_t> Descriptor() {
  std::vector<uint8_t> d = {
      0x05,0x0d, 0x09,0x05, 0xa1,0x01, // Digitizer / Touch Pad application
      0x85,0x01,                       // input report 1
      0x05,0x09, 0x09,0x01, 0x15,0x00, 0x25,0x01,
      0x75,0x01, 0x95,0x01, 0x81,0x02, // left button
      0x75,0x07, 0x81,0x03,            // padding
      0x05,0x0d, 0x09,0x54, 0x25,0x10,
      0x75,0x08, 0x81,0x02};           // contact count
  const uint8_t finger[] = {
      0x05,0x0d, 0x09,0x22, 0xa1,0x02, // Finger logical collection
      0x09,0x42, 0x15,0x00, 0x25,0x01,
      0x75,0x01, 0x95,0x01, 0x81,0x02, // tip switch
      0x75,0x07, 0x81,0x03,
      0x09,0x51, 0x25,0x0f, 0x75,0x08, 0x81,0x02, // contact ID
      // Units: 10^-2 cm. 7612 units / 160 mm and 5065 / 114.9 mm.
      0xa4, 0x05,0x01, 0x09,0x30, 0x26,0xbc,0x1d,
      0x35,0x00, 0x46,0x40,0x06, 0x55,0x0e, 0x65,0x11,
      0x75,0x10, 0x81,0x02, 0xb4,
      0xa4, 0x05,0x01, 0x09,0x31, 0x26,0xc9,0x13,
      0x35,0x00, 0x46,0x7d,0x04, 0x55,0x0e, 0x65,0x11,
      0x75,0x10, 0x81,0x02, 0xb4,
      0x09,0x30, 0x26,0xff,0x00, 0x75,0x08, 0x81,0x02, // pressure
      0x09,0x48, 0x09,0x49, 0x26,0xfc,0x03,
      0x75,0x10, 0x95,0x02, 0x81,0x02, // width and height
      0xc0};
  for (size_t i = 0; i < kSlots; ++i) d.insert(d.end(), std::begin(finger), std::end(finger));
  const uint8_t feature[] = {
      0x85,0x02, 0x05,0x0d, 0x09,0x55, // feature 2: Contact Count Maximum
      0x15,0x00, 0x25,0x10, 0x75,0x08, 0x95,0x01, 0xb1,0x02, 0xc0};
  d.insert(d.end(), std::begin(feature), std::end(feature));
  return d;
}

inline int Signed13(unsigned n) { return (n & 0x1000) ? int(n) - 8192 : int(n); }
inline void Put16(uint8_t* p, int n) { p[0] = n & 255; p[1] = (n >> 8) & 255; }

// No partial frame escapes on malformed input. Only Bluetooth report 0x31 is
// accepted; USB 0x02 and relative mouse reports are intentionally not decoded.
inline bool Translate(const uint8_t* data, size_t size,
                      std::array<uint8_t, kReportBytes>& output) {
  if (!data || size < 4 || data[0] != 0x31 || (size - 4) % 9 || size > 139) return false;
  std::array<uint8_t, kReportBytes> frame{};
  frame[0] = 1;
  frame[1] = data[1] & 1;
  frame[2] = kSlots;
  for (size_t id = 0; id < kSlots; ++id) frame[3 + id*kContactBytes + 1] = id;
  unsigned seen = 0;
  for (size_t off = 4; off < size; off += 9) {
    const uint8_t* t = data + off;
    unsigned id = t[8] & 15;
    if (seen & (1u << id)) return false;
    seen |= 1u << id;
    uint8_t* f = frame.data() + 3 + id*kContactBytes;
    if ((t[3] & 0xc0) != 0x80) continue;
    const int x = Signed13(t[0] | ((unsigned(t[1]) & 31) << 8));
    const int y = -Signed13((t[1] >> 5) | (unsigned(t[2]) << 3) | ((unsigned(t[3]) & 3) << 11));
    f[0] = 1;
    Put16(f+2, std::clamp(x, -3678, 3934) + 3678);
    Put16(f+4, std::clamp(y, -2478, 2587) + 2478);
    f[6] = t[7];
    Put16(f+7, int(t[4]) << 2);
    Put16(f+9, int(t[5]) << 2);
  }
  output = frame;
  return true;
}

// State keyed by the real UHID fd, below the optional Android input queue.
// Guarded because descriptor writes and UHID outbound events can use different
// threads with hid_report_queuing disabled. No lock is held across I/O.
class Bridge {
 public:
  bool Active(int fd) {
    std::lock_guard<std::mutex> guard(lock_);
    return active_.count(fd) != 0;
  }
  // Returns false to consume/drop an unsupported report from an enabled MT2.
  bool Prepare(int fd, const uhid_event& source, uhid_event& target,
               size_t& length, bool enabled) {
    if (source.type == UHID_CREATE2) {
      const auto& c = source.u.create2;
      const bool match = enabled && c.bus == BUS_BLUETOOTH && c.vendor == 0x004c && c.product == 0x0265;
      {
        std::lock_guard<std::mutex> guard(lock_);
        active_.erase(fd);
        if (match) active_.insert(fd);
      }
      if (!match) return true;
      target = source;
      const auto descriptor = Descriptor();
      target.u.create2.rd_size = descriptor.size();
      std::memcpy(target.u.create2.rd_data, descriptor.data(), descriptor.size());
      length = sizeof(target.type) + sizeof(target.u.create2) - HID_MAX_DESCRIPTOR_SIZE + descriptor.size();
      return true;
    }
    if (source.type == UHID_DESTROY) {
      std::lock_guard<std::mutex> guard(lock_);
      active_.erase(fd);
      return true;
    }
    if (source.type != UHID_INPUT2 || !Active(fd)) return true;
    std::array<uint8_t, kReportBytes> report{};
    if (!Translate(source.u.input2.data, source.u.input2.size, report)) return false;
    target = source;
    target.u.input2.size = report.size();
    std::memcpy(target.u.input2.data, report.data(), report.size());
    length = sizeof(target.type) + sizeof(target.u.input2.size) + report.size();
    return true;
  }
  // Synthetic feature requests must never be forwarded to the real device.
  bool FeatureReply(int fd, const uhid_event& request, uhid_event& reply) {
    if (!Active(fd) || request.type != UHID_GET_REPORT) return false;
    reply = {};
    reply.type = UHID_GET_REPORT_REPLY;
    reply.u.get_report_reply.id = request.u.get_report.id;
    if (request.u.get_report.rtype == UHID_FEATURE_REPORT && request.u.get_report.rnum == 2) {
      reply.u.get_report_reply.size = 2;
      reply.u.get_report_reply.data[0] = 2;
      reply.u.get_report_reply.data[1] = kSlots;
    } else {
      reply.u.get_report_reply.err = EOPNOTSUPP;
    }
    return true;
  }
 private:
  std::mutex lock_;
  std::unordered_set<int> active_;
};
} // namespace p11_mt2
