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

// uvm_report_server::do_copy copies the severity and id counts and the
// quit counts from another report server. Copying from an object that
// is not a report server reports UVM/REPORT/SERVER/RPTCOPY and leaves
// the destination unchanged.

#include <systemc>
#include <uvm>

using namespace uvm;

class not_a_server : public uvm::uvm_object
{
public:
  not_a_server( const std::string& name = "not_a_server" ) : uvm::uvm_object(name) {}
};

class test : public uvm_test
{
public:

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    uvm_report_server* glb = uvm_report_server::get_server();

    UVM_INFO("MSG1", "first message", UVM_LOW);
    UVM_INFO("MSG1", "second message", UVM_LOW);
    UVM_WARNING("MSG2", "a warning");

    glb->set_max_quit_count(7);
    glb->set_quit_count(3);

    // copy from a report server
    uvm_default_report_server dst("dst");
    dst.copy(*glb);

    std::cout << "copy: MSG1=" << dst.get_id_count("MSG1")
              << " MSG2=" << dst.get_id_count("MSG2")
              << " INFO=" << dst.get_severity_count(UVM_INFO)
              << " WARNING=" << dst.get_severity_count(UVM_WARNING)
              << " max_quit=" << dst.get_max_quit_count()
              << " quit=" << dst.get_quit_count() << std::endl;

    // copy from an object which is not a report server
    uvm_default_report_server dst2("dst2");
    dst2.set_max_quit_count(5);
    not_a_server other;
    dst2.copy(other);

    std::cout << "bad copy: MSG1=" << dst2.get_id_count("MSG1")
              << " max_quit=" << dst2.get_max_quit_count()
              << " quit=" << dst2.get_quit_count() << std::endl;

    glb->set_max_quit_count(0);
    glb->set_quit_count(0);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
