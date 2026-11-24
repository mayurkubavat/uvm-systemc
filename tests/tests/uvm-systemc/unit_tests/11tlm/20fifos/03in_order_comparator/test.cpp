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

// Test of uvm_in_order_comparator: two streams with different timing
// are compared in order, for a built-in type and for a class type.
// Matches and mismatches are counted and reported, and each pair is
// published on pair_ap.

#include <systemc>
#include <uvm>

#include <iostream>
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

  virtual void do_copy( const uvm::uvm_object& rhs )
  {
    const packet* p = dynamic_cast<const packet*>(&rhs);
    if (p != nullptr)
    {
      addr = p->addr;
      data = p->data;
    }
  }
};

template <typename PAIR>
class pair_listener : public uvm_subscriber<PAIR>
{
 public:
  pair_listener( uvm_component_name name ) : uvm_subscriber<PAIR>(name) {}

  virtual void write( const PAIR& p )
  {
    std::cout << this->get_name() << ": " << p.convert2string() << std::endl;
  }
};

class test : public uvm_test
{
 public:
  uvm_analysis_port<int> int_before_ap;
  uvm_analysis_port<int> int_after_ap;
  uvm_analysis_port<packet> pkt_before_ap;
  uvm_analysis_port<packet> pkt_after_ap;

  uvm_in_order_built_in_comparator<int> int_comp;
  uvm_in_order_class_comparator<packet> pkt_comp;

  pair_listener<uvm_built_in_pair<int> > int_pairs;
  pair_listener<uvm_class_pair<packet, packet> > pkt_pairs;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name )
  : uvm_test(name),
    int_before_ap("int_before_ap"),
    int_after_ap("int_after_ap"),
    pkt_before_ap("pkt_before_ap"),
    pkt_after_ap("pkt_after_ap"),
    int_comp("int_comp"),
    pkt_comp("pkt_comp"),
    int_pairs("int_pairs"),
    pkt_pairs("pkt_pairs")
  {}

  virtual void connect_phase( uvm_phase& phase )
  {
    int_before_ap.connect(int_comp.before_export);
    int_after_ap.connect(int_comp.after_export);
    int_comp.pair_ap.connect(int_pairs);

    pkt_before_ap.connect(pkt_comp.before_export);
    pkt_after_ap.connect(pkt_comp.after_export);
    pkt_comp.pair_ap.connect(pkt_pairs);
  }

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // the expected stream is complete before the actual one starts
    for (int i = 1; i <= 4; i++)
      int_before_ap.write(i);

    sc_core::wait(10, sc_core::SC_NS);

    int_after_ap.write(1);
    int_after_ap.write(2);
    sc_core::wait(10, sc_core::SC_NS);
    int_after_ap.write(30); // mismatch
    int_after_ap.write(4);
    sc_core::wait(10, sc_core::SC_NS);

    std::cout << "int_comp: matches " << int_comp.m_matches
              << ", mismatches " << int_comp.m_mismatches << std::endl;

    // the two streams alternate
    pkt_after_ap.write(packet(1, 100));
    pkt_before_ap.write(packet(1, 100));
    sc_core::wait(10, sc_core::SC_NS);
    pkt_before_ap.write(packet(2, 200));
    pkt_after_ap.write(packet(2, 201)); // mismatch
    sc_core::wait(10, sc_core::SC_NS);

    std::cout << "pkt_comp: matches " << pkt_comp.m_matches
              << ", mismatches " << pkt_comp.m_mismatches << std::endl;

    pkt_comp.flush();
    std::cout << "pkt_comp after flush: matches " << pkt_comp.m_matches
              << ", mismatches " << pkt_comp.m_mismatches << std::endl;

    // a transaction without counterpart is not compared
    int_before_ap.write(5);
    sc_core::wait(10, sc_core::SC_NS);

    phase.drop_objection(this);
  }

  void check_pairs()
  {
    uvm_class_pair<packet> p1("p1", packet(1, 2), packet(3, 4));
    uvm_class_pair<packet> p2("p2", packet(1, 2), packet(3, 5));
    uvm_built_in_pair<int> b1("b1");
    uvm_built_in_pair<int> b2("b2");

    std::cout << "class pairs which differ compare " << p1.compare(p2) << std::endl;
    p2.copy(p1);
    std::cout << "after copy they compare " << p1.compare(p2)
              << ", " << p2.convert2string() << std::endl;

    b1.first = 7;
    b1.second = 8;
    std::cout << "built-in pairs which differ compare " << b1.compare(b2) << std::endl;
    b2.copy(b1);
    std::cout << "after copy they compare " << b1.compare(b2)
              << ", " << b2.convert2string() << std::endl;

    uvm::uvm_object* c = b1.clone();
    if (c == nullptr)
    {
      UVM_ERROR("TEST", "clone of a pair returned a null pointer");
    }
    else
    {
      std::cout << "clone: " << c->convert2string() << std::endl;
      delete c;
    }
  }

  virtual void report_phase( uvm_phase& phase )
  {
    check_pairs();

    std::cout << "type names: " << int_comp.get_type_name() << ", "
              << pkt_comp.get_type_name() << std::endl;

    if (int_comp.m_matches == 3 && int_comp.m_mismatches == 1)
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
