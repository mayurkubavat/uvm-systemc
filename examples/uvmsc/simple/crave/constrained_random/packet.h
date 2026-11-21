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

#include <set>
#include <sstream>
#include <string>

#include <systemc>
#include <uvm>
#include <crave2uvm.h>

#define MAX_DELAY   10u
#define MAX_PAYLOAD 0x7fu

// Sequence item with random fields and constraints, solved by CRAVE.
//
// SystemVerilog equivalent:
//
//   rand int unsigned addr;
//   rand int unsigned length;
//   rand int unsigned delay;
//   rand int unsigned payload[];
//
//   constraint c_addr    { addr inside {'h1000, 'h2000, 'h3000, 'h4000}; }
//   constraint c_length  { length dist { [1:4] :/ 80, [5:8] :/ 20 }; }
//   constraint c_delay   { delay <= 10; soft delay == 0; }
//   constraint c_payload { payload.size() == length;
//                          foreach (payload[i]) payload[i] <= 'h7f; }

class packet : public uvm_randomized_sequence_item
{
 public:
  crave::crv_variable<unsigned int> addr;
  crave::crv_variable<unsigned int> length;
  crave::crv_variable<unsigned int> delay;
  crave::crv_vector<unsigned int>   payload;

  crave::crv_constraint c_addr {
    crave::inside(addr(), valid_addresses())
  };

  crave::crv_constraint c_length {
    crave::dist(length(), crave::make_distribution(
      crave::weighted_range<unsigned int>(1, 4, 80),
      crave::weighted_range<unsigned int>(5, 8, 20)))
  };

  crave::crv_constraint c_delay { delay() <= MAX_DELAY };

  // holds unless it conflicts with a hard or an inline constraint
  crave::crv_soft_constraint c_no_delay { delay() == 0 };

  crave::crv_constraint c_payload {
    payload().size() == length(),
    crave::foreach(payload(), payload()[crave::_i] <= MAX_PAYLOAD)
  };

  UVM_OBJECT_UTILS(packet);

  packet( crave::crv_object_name name = "packet" )
  : uvm_randomized_sequence_item(name)
  {}

  // The sequencer passes items by value. The random variables and the
  // constraints belong to the object they are declared in, so a copy
  // only takes over the generated values.
  packet( const packet& rhs )
  : uvm_randomized_sequence_item(rhs.get_name())
  {
    *this = rhs;
  }

  packet& operator=( const packet& rhs )
  {
    if (this != &rhs)
    {
      uvm::uvm_sequence_item::operator=(rhs);
      addr   = static_cast<unsigned int>(rhs.addr);
      length = static_cast<unsigned int>(rhs.length);
      delay  = static_cast<unsigned int>(rhs.delay);
      payload.clear();
      for (unsigned int i = 0; i < rhs.payload_size(); i++)
        payload.push_back(rhs.payload[i]);
    }
    return *this;
  }

  static const std::set<unsigned int>& valid_addresses()
  {
    static const std::set<unsigned int> addresses = { 0x1000, 0x2000, 0x3000, 0x4000 };
    return addresses;
  }

  unsigned int payload_size() const
  {
    return const_cast<packet*>(this)->payload.size();
  }

  // Checks the hard constraints without the help of the solver.
  // Returns an empty string if all of them hold.
  std::string check_constraints() const
  {
    std::ostringstream str;

    if (valid_addresses().count(addr) == 0)
      str << " addr is not a valid address;";

    if (length < 1 || length > 8)
      str << " length is outside of [1:8];";

    if (delay > MAX_DELAY)
      str << " delay is larger than " << MAX_DELAY << ";";

    if (payload_size() != length)
      str << " payload size differs from length;";

    for (unsigned int i = 0; i < payload_size(); i++)
      if (payload[i] > MAX_PAYLOAD)
        str << " payload[" << i << "] is larger than 0x"
            << std::hex << MAX_PAYLOAD << std::dec << ";";

    return str.str();
  }

  std::string convert2string() const
  {
    std::ostringstream str;
    str << "addr: 0x" << std::hex << addr << std::dec
        << " length: " << length
        << " delay: " << delay
        << " payload:" << std::hex;
    for (unsigned int i = 0; i < payload_size(); i++)
      str << " 0x" << payload[i];
    return str.str();
  }
};

#endif /* PACKET_H_ */
