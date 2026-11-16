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

// Test of the TLM-1 transport ports, exports and imps: blocking,
// non-blocking and combined transport ports, connected directly to imps
// and via an export, with different request and response types.

#include <systemc>
#include <tlm>
#include <uvm>

#include <iostream>
#include <sstream>
#include <string>

using namespace uvm;

class target : public uvm_component
{
 public:
  uvm_blocking_transport_imp<int, std::string, target> b_imp;
  uvm_nonblocking_transport_imp<int, std::string, target> nb_imp;
  uvm_transport_imp<int, std::string, target> imp;

  UVM_COMPONENT_UTILS(target);

  target( uvm_component_name name )
  : uvm_component(name), b_imp("b_imp", this), nb_imp("nb_imp", this), imp("imp", this)
  {}

  void transport( const int& req, std::string& rsp )
  {
    sc_core::wait(5, sc_core::SC_NS);
    std::ostringstream s;
    s << "rsp" << req;
    rsp = s.str();
    std::cout << sc_core::sc_time_stamp() << ": " << get_full_name() << " transport " << req
              << " -> " << rsp << std::endl;
  }

  bool nb_transport( const int& req, std::string& rsp )
  {
    if (req < 0)
    {
      std::cout << sc_core::sc_time_stamp() << ": " << get_full_name() << " nb_transport " << req
                << " refused" << std::endl;
      return false;
    }
    std::ostringstream s;
    s << "nb_rsp" << req;
    rsp = s.str();
    std::cout << sc_core::sc_time_stamp() << ": " << get_full_name() << " nb_transport " << req
              << " -> " << rsp << std::endl;
    return true;
  }
};

// hides the target behind an export
class wrapper : public uvm_component
{
 public:
  uvm_transport_export<int, std::string> transport_export;
  target t;

  UVM_COMPONENT_UTILS(wrapper);

  wrapper( uvm_component_name name )
  : uvm_component(name), transport_export("transport_export"), t("target")
  {}

  void connect_phase( uvm_phase& phase )
  {
    // an imp is both an export and an interface; connect it as interface
    uvm_tlm_transport_if<int, std::string>& imp_if = t.imp;
    transport_export.connect(imp_if);
  }
};

class initiator : public uvm_component
{
 public:
  uvm_blocking_transport_port<int, std::string> b_port;
  uvm_nonblocking_transport_port<int, std::string> nb_port;
  uvm_transport_port<int, std::string> port;

  UVM_COMPONENT_UTILS(initiator);

  initiator( uvm_component_name name )
  : uvm_component(name), b_port("b_port"), nb_port("nb_port"), port("port")
  {}

  void show( const std::string& what, bool ok, const std::string& rsp )
  {
    std::cout << sc_core::sc_time_stamp() << ": initiator " << what << ": "
              << (ok ? "true" : "false") << " " << rsp << std::endl;
  }

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    std::string rsp;

    b_port.transport(1, rsp);
    show("b_port.transport(1, rsp)", true, rsp);
    rsp = b_port.transport(2);
    show("b_port.transport(2)", true, rsp);

    rsp = "";
    show("nb_port.nb_transport(3, rsp)", nb_port.nb_transport(3, rsp), rsp);
    rsp = "";
    show("nb_port.nb_transport(-1, rsp)", nb_port.nb_transport(-1, rsp), rsp);

    port.transport(4, rsp);
    show("port.transport(4, rsp)", true, rsp);
    rsp = port.transport(5);
    show("port.transport(5)", true, rsp);
    rsp = "";
    show("port.nb_transport(6, rsp)", port.nb_transport(6, rsp), rsp);

    phase.drop_objection(this);
  }
};

class test : public uvm_test
{
 public:
  initiator i;
  target t;
  wrapper w;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name )
  : uvm_test(name), i("initiator"), t("target"), w("wrapper")
  {}

  void connect_phase( uvm_phase& phase )
  {
    i.b_port.connect(t.b_imp);
    i.nb_port.connect(t.nb_imp);
    i.port.connect(w.transport_export);

    std::cout << i.b_port.get_type_name() << std::endl
              << i.nb_port.get_type_name() << std::endl
              << i.port.get_type_name() << std::endl
              << t.b_imp.get_type_name() << std::endl
              << t.nb_imp.get_type_name() << std::endl
              << t.imp.get_type_name() << std::endl
              << w.transport_export.get_type_name() << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
