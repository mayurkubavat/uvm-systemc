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

#ifndef TESTBENCH_H_
#define TESTBENCH_H_

#include <iostream>

#include <systemc>
#include <uvm>

#include "packet.h"
#include "packet_driver.h"
#include "packet_sequence.h"
#include "packet_sequencer.h"

class testbench : public uvm::uvm_env
{
 public:
  packet_sequencer* sqr;
  packet_driver*    drv;

  UVM_COMPONENT_UTILS(testbench);

  testbench( uvm::uvm_component_name name )
  : uvm::uvm_env(name), sqr(nullptr), drv(nullptr)
  {
    // CRAVE follows the global seed of UVM, so the packets are the same
    // in every run and change with the command line argument +UVM_SEED
    crave::set_global_seed(uvm::uvm_get_random_seed());
  }

  void build_phase( uvm::uvm_phase& phase )
  {
    uvm::uvm_env::build_phase(phase);

    sqr = packet_sequencer::type_id::create("sequencer", this);
    drv = packet_driver::type_id::create("driver", this);
  }

  void connect_phase( uvm::uvm_phase& phase )
  {
    drv->seq_item_port(sqr->seq_item_export);
  }

  void run_phase( uvm::uvm_phase& phase )
  {
    phase.raise_objection(this);

    packet_sequence* seq = packet_sequence::type_id::create("packet_sequence");
    seq->start(sqr);
    packet_sequence::type_id::destroy(seq);

    phase.drop_objection(this);
  }

  void check_phase( uvm::uvm_phase& phase )
  {
    if (drv->num_packets != NUM_PACKETS)
      UVM_ERROR(this->get_name(), "Not all packets have been received");

    // weights are 80 for a short and 20 for a long packet
    if (drv->num_short <= drv->num_packets / 2)
      UVM_ERROR(this->get_name(), "Short packets are not in the majority");

    // only the packets with the inline constraint are delayed
    if (drv->num_delayed != NUM_PACKETS / 4)
      UVM_ERROR(this->get_name(), "Unexpected number of delayed packets");

    // Constraints which hold do not make the packets random. The values
    // have to be spread over the solutions of the constraints as well.
    if (drv->addresses.size() != packet::valid_addresses().size())
      UVM_ERROR(this->get_name(), "Not all addresses have been generated");

    if (drv->lengths.size() < 6) // out of 8
      UVM_ERROR(this->get_name(), "Too few different lengths have been generated");

    if (drv->delays.size() < 5) // out of 0 and [6:10]
      UVM_ERROR(this->get_name(), "Too few different delays have been generated");

    if (drv->payload_values.size() < 64) // out of 128
      UVM_ERROR(this->get_name(), "Too few different payload values have been generated");
  }

  // The generated values depend on the seed and on the solver, so only
  // the part between the tags is compared with the golden log file.
  void report_phase( uvm::uvm_phase& phase )
  {
    unsigned int errors =
      uvm::uvm_report_server::get_server()->get_severity_count(uvm::UVM_ERROR);

    std::cout << "test started" << std::endl;
    std::cout << "packets received: " << drv->num_packets << std::endl;
    std::cout << "packets delayed: " << drv->num_delayed << std::endl;
    std::cout << "constraint violations: " << drv->num_violations << std::endl;
    std::cout << "errors: " << errors << std::endl;
    std::cout << "test ended" << std::endl;
  }
};

#endif /* TESTBENCH_H_ */
