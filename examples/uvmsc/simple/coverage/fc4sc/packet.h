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

#ifndef PACKET_H_
#define PACKET_H_

#include <systemc>
#include <uvm>

class packet : public uvm::uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(packet);

  packet( const std::string& name = "packet" )
  : uvm::uvm_sequence_item(name),
    addr(0),
    is_write(false)
  {}

  packet( int addr_, bool is_write_ )
  : uvm::uvm_sequence_item("packet"),
    addr(addr_),
    is_write(is_write_)
  {}

  int addr;
  bool is_write;
};

#endif /* PACKET_H_ */
