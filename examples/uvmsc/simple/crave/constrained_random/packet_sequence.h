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

#ifndef PACKET_SEQUENCE_H_
#define PACKET_SEQUENCE_H_

#include <sstream>
#include <string>

#include <systemc>
#include <uvm>
#include <crave2uvm.h>

#include "packet.h"

#define NUM_PACKETS 200

// Randomizes NUM_PACKETS packets and sends them to the driver.
// Every fourth packet is randomized with an inline constraint.

class packet_sequence : public uvm_randomized_sequence<packet>
{
 public:
  UVM_OBJECT_UTILS(packet_sequence);

  packet_sequence( crave::crv_object_name name = "packet_sequence" )
  : uvm_randomized_sequence<packet>(name)
  {}

  void body()
  {
    packet* req = packet::type_id::create("req");

    UVM_INFO(this->get_name(), "Starting sequence", uvm::UVM_MEDIUM);

    for (unsigned int i = 0; i < NUM_PACKETS; i++)
    {
      bool delayed = (i % 4 == 3);
      bool success;

      this->start_item(req);

      if (delayed)
        // SystemVerilog: req.randomize() with { delay > 5; }
        success = req->randomize_with(req->delay() > 5);
      else
        success = req->randomize();

      if (!success)
        UVM_ERROR(this->get_name(), "Randomization failed");

      // The soft constraint only gives way to the inline constraint.
      if (delayed ? (req->delay <= 5) : (req->delay != 0))
      {
        std::ostringstream str;
        str << "Unexpected delay " << req->delay << " in packet " << i;
        UVM_ERROR(this->get_name(), str.str());
      }

      this->finish_item(req);
    }

    UVM_INFO(this->get_name(), "Finishing sequence", uvm::UVM_MEDIUM);

    packet::type_id::destroy(req);
  }
};

#endif /* PACKET_SEQUENCE_H_ */
