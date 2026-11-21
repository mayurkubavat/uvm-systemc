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

#ifndef PACKET_DRIVER_H_
#define PACKET_DRIVER_H_

#include <set>
#include <string>

#include <systemc>
#include <uvm>

#include "packet.h"

// Prints every packet it receives and checks that the constraints hold.

class packet_driver : public uvm::uvm_driver<packet>
{
 public:
  unsigned int num_packets;
  unsigned int num_violations;
  unsigned int num_short;
  unsigned int num_delayed;

  // the different values seen for the fields of the packets
  std::set<unsigned int> addresses;
  std::set<unsigned int> lengths;
  std::set<unsigned int> delays;
  std::set<unsigned int> payload_values;

  UVM_COMPONENT_UTILS(packet_driver);

  packet_driver( uvm::uvm_component_name name )
  : uvm::uvm_driver<packet>(name),
    num_packets(0), num_violations(0), num_short(0), num_delayed(0)
  {}

  void run_phase( uvm::uvm_phase& phase )
  {
    packet req;

    for(;;) // forever loop
    {
      this->seq_item_port->get_next_item(req);

      UVM_INFO(this->get_name(), req.convert2string(), uvm::UVM_MEDIUM);

      std::string violations = req.check_constraints();
      if (!violations.empty())
      {
        num_violations++;
        UVM_ERROR(this->get_name(), "Constraint violation:" + violations);
      }

      num_packets++;
      if (req.length <= 4) num_short++;
      if (req.delay != 0)  num_delayed++;

      addresses.insert(req.addr);
      lengths.insert(req.length);
      delays.insert(req.delay);
      for (unsigned int i = 0; i < req.payload_size(); i++)
        payload_values.insert(req.payload[i]);

      sc_core::wait(req.delay, sc_core::SC_NS);

      this->seq_item_port->item_done();
    }
  }
};

#endif /* PACKET_DRIVER_H_ */
