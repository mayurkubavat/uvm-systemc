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

// Test of uvm_algorithmic_comparator: a stream of bytes is compared
// with a stream of packets. The transformer turns each byte into the
// packet which is expected for it.

#include <systemc>
#include <uvm>

#include <iostream>
#include <sstream>
#include <string>

using namespace uvm;

class packet : public uvm::uvm_object
{
 public:
  int addr;
  int data;

  UVM_OBJECT_UTILS(packet);

  packet( const std::string& name = "packet" )
  : uvm::uvm_object(name), addr(0), data(0) {}

  packet( int addr_, int data_ )
  : uvm::uvm_object("packet"), addr(addr_), data(data_) {}

  virtual std::string convert2string() const
  {
    std::ostringstream str;
    str << "addr=" << addr << " data=" << data;
    return str.str();
  }

  virtual bool do_compare( const uvm::uvm_object& rhs,
                           const uvm_comparer* comparer = nullptr ) const
  {
    const packet* p = dynamic_cast<const packet*>(&rhs);
    return (p != nullptr) && (addr == p->addr) && (data == p->data);
  }
};

// The model of the design: the upper nibble of a byte is the address
// and the lower nibble is the data of the packet
class byte_to_packet
{
 public:
  byte_to_packet() : m_count(0) {}

  packet transform( const unsigned char& b )
  {
    m_count++;
    return packet(b >> 4, b & 0xf);
  }

  int m_count;
};

class test : public uvm_test
{
 public:
  typedef uvm_algorithmic_comparator<unsigned char, packet, byte_to_packet> comp_type;

  uvm_analysis_port<unsigned char> before_ap;
  uvm_analysis_port<packet> after_ap;

  byte_to_packet transformer;
  comp_type comp;
  comp_type comp_without_transformer;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name )
  : uvm_test(name),
    before_ap("before_ap"),
    after_ap("after_ap"),
    comp("comp", &transformer),
    comp_without_transformer("comp_without_transformer")
  {}

  virtual void connect_phase( uvm_phase& phase )
  {
    before_ap.connect(comp.before_export);
    after_ap.connect(comp.after_export);
  }

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // the stream of bytes is ahead of the stream of packets
    before_ap.write(0x12);
    before_ap.write(0x34);
    sc_core::wait(10, sc_core::SC_NS);

    after_ap.write(packet(1, 2));
    sc_core::wait(10, sc_core::SC_NS);
    after_ap.write(packet(3, 5)); // mismatch
    sc_core::wait(10, sc_core::SC_NS);

    // the stream of packets is ahead
    after_ap.write(packet(15, 0));
    sc_core::wait(10, sc_core::SC_NS);
    before_ap.write(0xf0);
    sc_core::wait(10, sc_core::SC_NS);

    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    std::cout << "type name: " << comp.get_type_name() << std::endl;
    std::cout << "transformed " << transformer.m_count
              << " transactions" << std::endl;

    if (transformer.m_count == 3)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "** UVM TEST FAILED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  test t("t");
  run_test();
  return 0;
}
