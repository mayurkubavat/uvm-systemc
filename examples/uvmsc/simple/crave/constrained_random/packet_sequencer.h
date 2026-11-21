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

#ifndef PACKET_SEQUENCER_H_
#define PACKET_SEQUENCER_H_

#include <systemc>
#include <uvm>

#include "packet.h"

class packet_sequencer : public uvm::uvm_sequencer<packet>
{
 public:
  UVM_COMPONENT_UTILS(packet_sequencer);

  packet_sequencer( uvm::uvm_component_name name )
  : uvm::uvm_sequencer<packet>(name)
  {}
};

#endif /* PACKET_SEQUENCER_H_ */
