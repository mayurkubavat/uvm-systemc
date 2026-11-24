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

// Test of uvm_tlm_transport_channel: a transport port connected to the
// transport_export of the channel, and a slave port connected to its
// slave_export, which serves the requests. nb_transport fails while the
// response is not available. The channel and its exports are named
// below the parent; a channel without a name gets a unique name.

#include <systemc>
#include <tlm>
#include <uvm>

#include <iostream>
#include <sstream>
#include <string>

using namespace uvm;

class initiator : public uvm_component
{
 public:
  uvm_transport_port<int, std::string> port;
  UVM_COMPONENT_UTILS(initiator);
  initiator( uvm_component_name name ) : uvm_component(name), port("port") {}

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    std::string rsp;
    for (int i = 1; i <= 3; i++)
    {
      port.transport(i, rsp);
      std::cout << sc_core::sc_time_stamp() << ": initiator transport " << i
                << " -> " << rsp << std::endl;
    }

    // the request is accepted, but the response is not available yet
    rsp = "";
    bool ok = port.nb_transport(4, rsp);
    std::cout << sc_core::sc_time_stamp() << ": initiator nb_transport 4: " << ok << std::endl;

    // as in IEEE 1800.2, the failed nb_transport leaves request 4 in the
    // channel: the next transport gets the response to request 4
    sc_core::wait(20, sc_core::SC_NS);
    rsp = port.transport(5);
    std::cout << sc_core::sc_time_stamp() << ": initiator transport 5 -> " << rsp << std::endl;
    phase.drop_objection(this);
  }
};

class responder : public uvm_component
{
 public:
  uvm_slave_port<int, std::string> port;
  UVM_COMPONENT_UTILS(responder);
  responder( uvm_component_name name ) : uvm_component(name), port("port") {}

  void run_phase( uvm_phase& phase )
  {
    while (true)
    {
      int req = port.get();
      std::cout << sc_core::sc_time_stamp() << ": responder got " << req << std::endl;
      sc_core::wait(10, sc_core::SC_NS);
      std::ostringstream s;
      s << "rsp" << req;
      port.put(s.str());
    }
  }
};

class test : public uvm_test
{
 public:
  initiator i;
  responder r;
  uvm_tlm_transport_channel<int, std::string> channel;
  uvm_tlm_transport_channel<int, std::string> unnamed;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name )
  : uvm_test(name), i("initiator"), r("responder"), channel("channel")
  {}

  void connect_phase( uvm_phase& phase )
  {
    i.port.connect(channel.transport_export);
    r.port.connect(channel.slave_export);

    std::cout << channel.name() << " (" << channel.kind() << ")" << std::endl
              << channel.transport_export.name() << " ("
              << channel.transport_export.get_type_name() << ")" << std::endl
              << unnamed.name() << std::endl
              << unnamed.transport_export.name() << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
