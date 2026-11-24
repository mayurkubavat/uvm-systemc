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

// Test of uvm_tlm_analysis_fifo: transactions written by an analysis
// port in zero time are buffered and retrieved in order by a component
// which consumes time.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

class monitor : public uvm_component
{
 public:
  uvm_analysis_port<std::string> ap;

  UVM_COMPONENT_UTILS(monitor);

  monitor( uvm_component_name name ) : uvm_component(name), ap("ap") {}

  void run_phase( uvm_phase& phase )
  {
    const char* words[] = { "one", "two", "three", "four", "five" };

    phase.raise_objection(this);

    // all in one delta cycle
    for (int i = 0; i < 5; i++)
      ap.write(words[i]);

    phase.drop_objection(this);
  }
};

class checker_comp : public uvm_component
{
 public:
  uvm_tlm_analysis_fifo<std::string> fifo;
  uvm_tlm_analysis_fifo<std::string> direct_fifo;

  UVM_COMPONENT_UTILS(checker_comp);

  checker_comp( uvm_component_name name )
  : uvm_component(name), fifo("fifo"), direct_fifo("direct_fifo") {}

  void run_phase( uvm_phase& phase )
  {
    std::string s;

    phase.raise_objection(this);

    sc_core::wait(5, sc_core::SC_NS);
    std::cout << "fifo: is_unbounded " << fifo.is_unbounded() << ", used " << fifo.used()
              << ", is_full " << fifo.is_full() << std::endl;

    while (!fifo.is_empty())
    {
      fifo.get(s);
      sc_core::wait(5, sc_core::SC_NS);
      std::cout << sc_core::sc_time_stamp() << " got " << s << std::endl;
    }

    std::cout << "direct_fifo: used " << direct_fifo.used() << std::endl;
    std::cout << "type name " << fifo.get_type_name() << std::endl;

    phase.drop_objection(this);
  }
};

class test : public uvm_test
{
 public:
  monitor mon;
  checker_comp chk;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name), mon("mon"), chk("chk") {}

  virtual void connect_phase( uvm_phase& phase )
  {
    // through the export, and to the FIFO itself
    mon.ap.connect(chk.fifo.analysis_export);
    mon.ap.connect(chk.direct_fifo);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    if (chk.fifo.is_empty() && chk.direct_fifo.used() == 5)
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
