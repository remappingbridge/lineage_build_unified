// SPDX-License-Identifier: Apache-2.0
#include "../p11_mt2_bridge.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>

using namespace p11_mt2;
int Get16(const uint8_t* p) { return p[0] | (p[1] << 8); }
std::array<uint8_t,9> Contact(int id, int x, int y, bool down=true) {
  const unsigned ux = unsigned(x) & 8191;
  const unsigned uy = unsigned(-y) & 8191;
  return {uint8_t(ux), uint8_t((ux >> 8) | ((uy & 7) << 5)), uint8_t(uy >> 3),
          uint8_t((uy >> 11) | (down ? 0x80 : 0)), 12, 8, 4, 70, uint8_t(id)};
}
std::vector<uint8_t> Frame(std::initializer_list<std::array<uint8_t,9>> contacts) {
  std::vector<uint8_t> r = {0x31,1,0,0};
  for (const auto& c : contacts) r.insert(r.end(),c.begin(),c.end());
  return r;
}
int main(int argc, char** argv) {
  std::array<uint8_t,kReportBytes> out{};
  // Genuine protocol boundaries and sparse IDs, not just sequential fingers.
  auto raw = Frame({Contact(0,-3678,-2478),Contact(5,3934,2587),Contact(9,0,0),Contact(15,-1,1)});
  assert(Translate(raw.data(),raw.size(),out));
  assert(out[0]==1 && out[1]==1 && out[2]==16);
  for (int id : {0,5,9,15}) assert(out[3+id*kContactBytes]==1);
  assert(Get16(&out[5])==0 && Get16(&out[7])==0);
  assert(Get16(&out[5+5*kContactBytes])==7612);
  assert(Get16(&out[7+5*kContactBytes])==5065);
  assert(Get16(&out[5+9*kContactBytes])==3678);
  assert(out[9+9*kContactBytes]==70);
  assert(Get16(&out[10+9*kContactBytes])==48);
  // Empty frame releases all IDs, including absent contacts; button still works.
  raw=Frame({});
  assert(Translate(raw.data(),raw.size(),out));
  for (int i=0;i<16;i++) assert(out[3+i*kContactBytes]==0 && out[4+i*kContactBytes]==i);
  raw=Frame({Contact(3,100,200,false)});
  assert(Translate(raw.data(),raw.size(),out) && !out[3+3*kContactBytes]);
  // Reject truncated/duplicate/foreign frames atomically.
  const auto before=out;
  raw=Frame({Contact(1,0,0),Contact(1,100,100)});
  assert(!Translate(raw.data(),raw.size(),out) && out==before);
  raw.pop_back();
  assert(!Translate(raw.data(),raw.size(),out) && out==before);
  raw={0x02,0,0,0};
  assert(!Translate(raw.data(),raw.size(),out));
  assert(!Translate(nullptr,0,out));
  // Exhaust the signed 13-bit coordinate encoding and clamp at physical limits.
  for (int v=-4096;v<4096;v++) {
    raw=Frame({Contact(7,v,v == -4096 ? 4096 : -v)});
    assert(Translate(raw.data(),raw.size(),out));
    assert(Get16(&out[5+7*kContactBytes])==std::clamp(v,-3678,3934)+3678);
    assert(Get16(&out[7+7*kContactBytes])==std::clamp(-v,-2478,2587)+2478);
  }
  // Bounded fuzz under address/undefined sanitizers.
  std::mt19937 rng(265);
  for (int i=0;i<20000;i++) {
    raw.resize(rng()%160);
    for(auto& b:raw) b=rng();
    Translate(raw.data(),raw.size(),out);
  }
  Bridge bridge;
  uhid_event create{},translated{};
  create.type=UHID_CREATE2;
  create.u.create2.bus=BUS_BLUETOOTH;
  create.u.create2.vendor=0x004c;
  create.u.create2.product=0x0265;
  size_t length=sizeof(create);
  assert(bridge.Prepare(8,create,translated,length,false) && !bridge.Active(8));
  assert(bridge.Prepare(8,create,translated,length,true) && bridge.Active(8));
  assert(translated.u.create2.rd_size==Descriptor().size());
  assert(length==sizeof(create.type)+sizeof(create.u.create2)-HID_MAX_DESCRIPTOR_SIZE+Descriptor().size());
  uhid_event req{},reply{};
  req.type=UHID_GET_REPORT;
  req.u.get_report.id=123;
  req.u.get_report.rnum=2;
  req.u.get_report.rtype=UHID_FEATURE_REPORT;
  assert(bridge.FeatureReply(8,req,reply));
  assert(reply.u.get_report_reply.id==123 && reply.u.get_report_reply.size==2);
  assert(reply.u.get_report_reply.data[0]==2 && reply.u.get_report_reply.data[1]==16);
  req.u.get_report.rnum=99;
  assert(bridge.FeatureReply(8,req,reply) && reply.u.get_report_reply.err==EOPNOTSUPP);
  assert(!bridge.FeatureReply(9,req,reply));
  uhid_event input{};
  input.type=UHID_INPUT2;
  raw=Frame({Contact(2,100,200)});
  input.u.input2.size=raw.size();
  std::memcpy(input.u.input2.data,raw.data(),raw.size());
  assert(bridge.Prepare(8,input,translated,length,false));
  assert(translated.u.input2.size==kReportBytes && length==6+kReportBytes);
  assert(translated.u.input2.data[3+2*kContactBytes]==1);
  input.u.input2.data[0]=2;
  assert(!bridge.Prepare(8,input,translated,length,false));
  assert(bridge.Prepare(9,input,translated,length,false)); // unrelated device untouched
  uhid_event destroy{};
  destroy.type=UHID_DESTROY;
  assert(bridge.Prepare(8,destroy,translated,length,false) && !bridge.Active(8));
  create.u.create2.vendor=0x05ac; // USB VID on BT is not the baseline device
  assert(bridge.Prepare(8,create,translated,length,true) && !bridge.Active(8));
  create.u.create2.vendor=0x004c;
  create.u.create2.bus=BUS_USB;
  assert(bridge.Prepare(8,create,translated,length,true) && !bridge.Active(8));
  // A reused descriptor fd cannot inherit activation from a prior device.
  create.u.create2.bus=BUS_BLUETOOTH;
  bridge.Prepare(8,create,translated,length,true);
  create.u.create2.product=0x1234;
  bridge.Prepare(8,create,translated,length,true);
  assert(!bridge.Active(8));
  if(argc==2) {
    auto desc=Descriptor();
    std::ofstream f(argv[1],std::ios::binary);
    f.write(reinterpret_cast<const char*>(desc.data()),desc.size());
  }
  std::cout << "PASS: protocol, boundaries, releases, malformed frames, feature replies, fd lifecycle, 20000 fuzz inputs\n";
}
