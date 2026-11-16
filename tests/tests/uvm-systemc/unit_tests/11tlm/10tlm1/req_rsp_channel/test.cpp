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

// Drives requests and responses through every export of
// uvm_tlm_req_rsp_channel and checks that each request and response
// which is put into the channel is written to request_ap and response_ap.

#include <systemc>
#include <tlm>
#include <uvm>

#include <sstream>
#include <vector>

static void check( bool cond, const std::string& what )
{
  if (cond)
    UVM_INFO("CHECK", "ok: " + what, uvm::UVM_MEDIUM);
  else
    UVM_ERROR("CHECK", "failed: " + what);
}

static std::string to_str( const std::vector<int>& v )
{
  std::ostringstream s;
  for (std::size_t i = 0; i < v.size(); ++i)
    s << (i ? " " : "") << v[i];
  return s.str();
}

//----------------------------------------------------------------------
// class monitor: records the transactions written to an analysis port
//----------------------------------------------------------------------

class monitor : public uvm::uvm_subscriber<int>
{
 public:
  std::vector<int> seen;

  UVM_COMPONENT_UTILS(monitor);

  monitor( uvm::uvm_component_name name ) : uvm::uvm_subscriber<int>(name) {}

  void write( const int& t )
  {
    std::ostringstream msg;
    msg << "observed " << t;
    UVM_INFO("monitor", msg.str(), uvm::UVM_MEDIUM);
    seen.push_back(t);
  }
}; // class monitor

//----------------------------------------------------------------------
// class master: puts requests, gets and peeks responses
//----------------------------------------------------------------------

class master : public uvm::uvm_component
{
 public:
  uvm::uvm_put_port<int> req_put_port;       // -> put_request_export
  uvm::uvm_get_port<int> rsp_get_port;       // -> get_response_export
  uvm::uvm_get_peek_port<int> rsp_gp_port;   // -> get_peek_response_export
  sc_core::sc_port< tlm::tlm_master_if<int, int> > master_port; // -> master_export

  UVM_COMPONENT_UTILS(master);

  master( uvm::uvm_component_name name )
  : uvm::uvm_component(name),
    req_put_port("req_put_port"),
    rsp_get_port("rsp_get_port"),
    rsp_gp_port("rsp_gp_port"),
    master_port("master_port")
  {}

  void run_phase( uvm::uvm_phase& phase )
  {
    phase.raise_objection(this);

    req_put_port.put(1);
    check(req_put_port.nb_put(2), "nb_put(2) via put_request_export accepted");
    master_port->put(3);
    check(master_port->nb_put(4), "nb_put(4) via master_export accepted");
    check(!req_put_port.nb_put(99), "nb_put(99) via put_request_export refused (request FIFO full)");
    check(!master_port->nb_can_put(), "nb_can_put via master_export false (request FIFO full)");

    int rsp = 0;
    rsp_get_port.get(rsp);
    check(rsp == 10, "get via get_response_export returns 10");
    rsp_gp_port.peek(rsp);
    check(rsp == 11, "peek via get_peek_response_export returns 11");
    rsp_gp_port.get(rsp);
    check(rsp == 11, "get via get_peek_response_export returns 11");
    rsp = master_port->peek();
    check(rsp == 12, "peek via master_export returns 12");
    rsp = master_port->get();
    check(rsp == 12, "get via master_export returns 12");
    check(master_port->nb_get(rsp) && rsp == 13, "nb_get via master_export returns 13");
    check(!rsp_get_port.nb_get(rsp), "nb_get via get_response_export fails (response FIFO empty)");

    phase.drop_objection(this);
  }
}; // class master

//----------------------------------------------------------------------
// class slave: gets and peeks requests, puts responses
//----------------------------------------------------------------------

class slave : public uvm::uvm_component
{
 public:
  uvm::uvm_get_port<int> req_get_port;       // -> get_request_export
  uvm::uvm_get_peek_port<int> req_gp_port;   // -> get_peek_request_export
  uvm::uvm_put_port<int> rsp_put_port;       // -> put_response_export
  sc_core::sc_port< tlm::tlm_slave_if<int, int> > slave_port; // -> slave_export

  UVM_COMPONENT_UTILS(slave);

  slave( uvm::uvm_component_name name )
  : uvm::uvm_component(name),
    req_get_port("req_get_port"),
    req_gp_port("req_gp_port"),
    rsp_put_port("rsp_put_port"),
    slave_port("slave_port")
  {}

  void run_phase( uvm::uvm_phase& phase )
  {
    int req = 0;
    req_get_port.get(req);
    check(req == 1, "get via get_request_export returns 1");
    req_gp_port.peek(req);
    check(req == 2, "peek via get_peek_request_export returns 2");
    req_gp_port.get(req);
    check(req == 2, "get via get_peek_request_export returns 2");
    req = slave_port->peek();
    check(req == 3, "peek via slave_export returns 3");
    req = slave_port->get();
    check(req == 3, "get via slave_export returns 3");
    check(slave_port->nb_get(req) && req == 4, "nb_get via slave_export returns 4");

    rsp_put_port.put(10);
    check(rsp_put_port.nb_put(11), "nb_put(11) via put_response_export accepted");
    slave_port->put(12);
    check(slave_port->nb_put(13), "nb_put(13) via slave_export accepted");
    check(!slave_port->nb_put(99), "nb_put(99) via slave_export refused (response FIFO full)");
  }
}; // class slave

//----------------------------------------------------------------------
// class env
//----------------------------------------------------------------------

class env : public uvm::uvm_env
{
 public:
  uvm::uvm_tlm_req_rsp_channel<int> chan;
  uvm::uvm_tlm_req_rsp_channel<int> unnamed_chan;
  master m;
  slave s;
  monitor req_mon;
  monitor rsp_mon;

  UVM_COMPONENT_UTILS(env);

  env( uvm::uvm_component_name name = "env" )
  : uvm::uvm_env(name),
    chan("chan", 4, 4),
    unnamed_chan(),
    m("master"),
    s("slave"),
    req_mon("req_mon"),
    rsp_mon("rsp_mon")
  {}

  void connect_phase( uvm::uvm_phase& phase )
  {
    uvm::uvm_env::connect_phase(phase);

    m.req_put_port.connect(chan.put_request_export);
    m.rsp_get_port.connect(chan.get_response_export);
    m.rsp_gp_port.connect(chan.get_peek_response_export);
    m.master_port.bind(chan.master_export);

    s.req_get_port.connect(chan.get_request_export);
    s.req_gp_port.connect(chan.get_peek_request_export);
    s.rsp_put_port.connect(chan.put_response_export);
    s.slave_port.bind(chan.slave_export);

    chan.request_ap.connect(req_mon.analysis_export);
    chan.response_ap.connect(rsp_mon.analysis_export);
  }

  void end_of_elaboration_phase( uvm::uvm_phase& phase )
  {
    const sc_core::sc_object* objs[] = {
      &chan.request_ap, &chan.response_ap,
      &chan.put_request_export, &chan.put_response_export,
      &chan.get_request_export, &chan.get_response_export,
      &chan.get_peek_request_export, &chan.get_peek_response_export,
      &chan.master_export, &chan.slave_export,
      &unnamed_chan.master_export };
    for (const sc_core::sc_object* o : objs)
      UVM_INFO("env", std::string("channel member ") + o->name(), uvm::UVM_MEDIUM);
  }

  void check_phase( uvm::uvm_phase& phase )
  {
    check(to_str(req_mon.seen) == "1 2 3 4",
          "request_ap wrote [" + to_str(req_mon.seen) + "], expected [1 2 3 4]");
    check(to_str(rsp_mon.seen) == "10 11 12 13",
          "response_ap wrote [" + to_str(rsp_mon.seen) + "], expected [10 11 12 13]");
  }
}; // class env

//----------------------------------------------------------------------
// class test
//----------------------------------------------------------------------

class test : public uvm::uvm_test
{
 public:
  env e;

  UVM_COMPONENT_UTILS(test);

  test( uvm::uvm_component_name name = "test" )
  : uvm::uvm_test(name),
    e("env")
  {}
}; // class test

//----------------------------------------------------------------------
// main program (sc_main)
//----------------------------------------------------------------------

int sc_main(int, char*[])
{
  test t("test");

  uvm::run_test();

  return 0;
}
