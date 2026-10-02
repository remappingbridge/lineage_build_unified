#!/usr/bin/env python3
"""Exercise the actual patched UHID writer, including short queue allocations.

Pass a pristine bta_hh_co.cc from the audited LineageOS source. This is not a
replacement for compiling the complete Bluetooth module with Android Soong.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

assets = Path(__file__).resolve().parents[1]
original = Path(sys.argv[1]).read_bytes()
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    bt = root / "packages/modules/Bluetooth"
    source = bt / "system/btif/co/bta_hh_co.cc"
    source.parent.mkdir(parents=True)
    source.write_bytes(original)
    subprocess.run(["git", "init", "-q", str(bt)], check=True)
    for flags in (["--check"], [], [], ["--check"]):
        subprocess.run(["python3", str(assets / "apply.py"), str(root), *flags], check=True)
    patched = source.read_text()
    writer = patched.split("static int uhid_write(", 1)[1].split("static void uhid_flush_input_queue", 1)[0]
    harness = r'''
#include "p11_mt2_bridge.h"
#include <cassert>
#include <cstdlib>
#include <unistd.h>
struct Log {
  template<class... T> static void info(T...) {}
  template<class... T> static void error(T...) {}
};
using log = Log;
#define OSI_NO_INTR(x) do { x; } while (false)
bool osi_property_get_bool(const char*, bool) { return true; }
static p11_mt2::Bridge p11_mt2_bridge;
'''
    harness += "static int uhid_write(" + writer
    harness += r'''
int main() {
  int fds[2]; assert(pipe(fds)==0);
  uhid_event create{},received{};
  create.type=UHID_CREATE2;
  create.u.create2.bus=BUS_BLUETOOTH;
  create.u.create2.vendor=0x004c; create.u.create2.product=0x0265;
  const size_t create_size=sizeof(create.type)+sizeof(create.u.create2)-HID_MAX_DESCRIPTOR_SIZE;
  assert(uhid_write(fds[1],&create,create_size)==0);
  assert(read(fds[0],&received,sizeof(received))>0);
  assert(received.u.create2.rd_size==p11_mt2::Descriptor().size());
  // Android queues allocate exactly len bytes; reading sizeof(*ev) is an OOB.
  const size_t size=6+4;
  auto* short_event=static_cast<uhid_event*>(std::calloc(1,size));
  short_event->type=UHID_INPUT2;
  short_event->u.input2.size=4;
  short_event->u.input2.data[0]=0x31;
  assert(uhid_write(fds[1],short_event,size)==0);
  assert(read(fds[0],&received,sizeof(received))==6+p11_mt2::kReportBytes);
  assert(received.u.input2.data[0]==1 && received.u.input2.data[2]==16);
  std::free(short_event);
  // Unrelated UHID devices retain the exact report and wire length.
  p11_mt2::Bridge other;
  create.u.create2.product=0x1234;
  assert(uhid_write(fds[1],&create,create_size)==0);
  assert(read(fds[0],&received,sizeof(received))==static_cast<ssize_t>(create_size));
  uhid_event input{}; input.type=UHID_INPUT2; input.u.input2.size=3;
  input.u.input2.data[0]=2; input.u.input2.data[1]=42; input.u.input2.data[2]=99;
  assert(uhid_write(fds[1],&input,9)==0);
  assert(read(fds[0],&received,sizeof(received))==9);
  assert(std::memcmp(&input,&received,9)==0);
  close(fds[0]);close(fds[1]);
}
'''
    test = root / "test.cc"
    test.write_text(harness)
    binary = root / "test"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
                    "-I", str(assets), str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}, check=True)
    subprocess.run(["python3", str(assets / "apply.py"), str(root), "--revert"], check=True)
    assert source.read_bytes() == original
    source.write_text("incompatible source\n")
    failure = subprocess.run(["python3", str(assets / "apply.py"), str(root)], capture_output=True)
    assert failure.returncode and source.read_text() == "incompatible source\n"
    assert not (source.parent / "p11_mt2_bridge.h").exists()
    print("PASS: actual patched UHID writer, short queue allocation, unrelated reports, apply/revert/refusal")
