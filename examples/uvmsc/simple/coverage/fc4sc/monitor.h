//----------------------------------------------------------------------
//   Copyright 2026 Mayur Kubavat
//   All Rights Reserved Worldwide
//
//   Licensed under the Apache License, Version 2.0 (the
//   "License"); you may not use this file except in
//   compliance with the License.  You may obtain a copy of
//   the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in
//   writing, software distributed under the License is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//   CONDITIONS OF ANY KIND, either express or implied.  See
//   the License for the specific language governing
//   permissions and limitations under the License.
//----------------------------------------------------------------------

#ifndef MONITOR_H_
#define MONITOR_H_

#include <systemc>
#include <uvm>

#include "packet.h"

// Stand-in for a bus monitor: it publishes a fixed series of packets on
// its analysis port instead of observing an interface.

class monitor : public uvm::uvm_monitor
{
 public:
  uvm::uvm_analysis_port<packet> item_collected_port;

  monitor( uvm::uvm_component_name name )
  : uvm::uvm_monitor(name),
    item_collected_port("item_collected_port")
  {}

  UVM_COMPONENT_UTILS(monitor);

  virtual void run_phase( uvm::uvm_phase& phase )
  {
    static const int addr[] = { 0x04, 0x10, 0x48, 0x80, 0x3c, 0x9c };
    static const bool is_write[] = { true, false, true, true, false, true };

    phase.raise_objection(this);

    for (unsigned int i = 0; i < sizeof(addr) / sizeof(addr[0]); i++)
    {
      sc_core::wait(10, sc_core::SC_NS);
      item_collected_port.write(packet(addr[i], is_write[i]));
    }

    phase.drop_objection(this);
  }
};

#endif /* MONITOR_H_ */
