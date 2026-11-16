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

// Test of the TLM-1 master and slave ports, exports and imps: blocking,
// non-blocking and combined ports connected to imps, directly and via
// exports, a master imp with separate request and response
// implementations, and master and slave ports connected to the exports
// of a SystemC tlm::tlm_req_rsp_channel.

#include <systemc>
#include <tlm>
#include <uvm>

#include <deque>
#include <iostream>
#include <sstream>
#include <string>

using namespace uvm;

static std::string to_rsp( int req )
{
  std::ostringstream s;
  s << "rsp" << req;
  return s.str();
}

// request side of a master: accepts requests
class req_sink
{
 public:
  explicit req_sink( const std::string& name ) : m_name(name) {}

  std::deque<int> reqs;

  void put( const int& req )
  {
    std::cout << sc_core::sc_time_stamp() << ": " << m_name << " put " << req << std::endl;
    reqs.push_back(req);
  }
  bool try_put( const int& req ) { put(req); return true; }
  bool can_put() { return true; }

 private:
  std::string m_name;
};

// response side of a master: provides a response per request
class rsp_source
{
 public:
  explicit rsp_source( req_sink& sink ) : m_sink(sink) {}

  std::string get()
  {
    while (m_sink.reqs.empty())
      sc_core::wait(1, sc_core::SC_NS);
    std::string rsp = to_rsp(m_sink.reqs.front());
    m_sink.reqs.pop_front();
    return rsp;
  }
  std::string peek() const { return m_sink.reqs.empty() ? "" : to_rsp(m_sink.reqs.front()); }
  bool try_get( std::string& rsp )
  {
    if (m_sink.reqs.empty()) return false;
    rsp = to_rsp(m_sink.reqs.front());
    m_sink.reqs.pop_front();
    return true;
  }
  bool can_get() { return !m_sink.reqs.empty(); }
  bool try_peek( std::string& rsp ) const
  {
    if (m_sink.reqs.empty()) return false;
    rsp = to_rsp(m_sink.reqs.front());
    return true;
  }
  bool can_peek() const { return !m_sink.reqs.empty(); }

 private:
  req_sink& m_sink;
};

// implements the master interfaces itself (blocking and non-blocking
// imps) and with separate request and response implementations
class master_target : public uvm_component
{
 public:
  req_sink sink;
  rsp_source source;
  req_sink own_sink;
  rsp_source own_source;

  uvm_blocking_master_imp<int, std::string, master_target> b_imp;
  uvm_nonblocking_master_imp<int, std::string, master_target> nb_imp;
  uvm_master_imp<int, std::string, master_target, req_sink, rsp_source> imp;

  UVM_COMPONENT_UTILS(master_target);

  master_target( uvm_component_name name )
  : uvm_component(name), sink("sink"), source(sink), own_sink("master_target"),
    own_source(own_sink), b_imp("b_imp", this), nb_imp("nb_imp", this),
    imp("imp", this, &sink, &source)
  {}

  void put( const int& req ) { own_sink.put(req); }
  bool try_put( const int& req ) { return own_sink.try_put(req); }
  bool can_put() { return own_sink.can_put(); }
  std::string get() { return own_source.get(); }
  std::string peek() const { return own_source.peek(); }
  bool try_get( std::string& rsp ) { return own_source.try_get(rsp); }
  bool can_get() { return own_source.can_get(); }
  bool try_peek( std::string& rsp ) const { return own_source.try_peek(rsp); }
  bool can_peek() const { return own_source.can_peek(); }
};

// implements the slave interfaces: provides requests, accepts responses
class slave_target : public uvm_component
{
 public:
  uvm_blocking_slave_imp<int, std::string, slave_target> b_imp;
  uvm_nonblocking_slave_imp<int, std::string, slave_target> nb_imp;
  uvm_slave_imp<int, std::string, slave_target> imp;

  int next;

  UVM_COMPONENT_UTILS(slave_target);

  slave_target( uvm_component_name name )
  : uvm_component(name), b_imp("b_imp", this), nb_imp("nb_imp", this), imp("imp", this),
    next(100)
  {}

  int get() { return next++; }
  int peek() const { return next; }
  bool try_get( int& req ) { req = next++; return true; }
  bool can_get() { return true; }
  bool try_peek( int& req ) const { req = next; return true; }
  bool can_peek() const { return true; }

  void put( const std::string& rsp )
  {
    std::cout << sc_core::sc_time_stamp() << ": slave_target put " << rsp << std::endl;
  }
  bool try_put( const std::string& rsp ) { put(rsp); return true; }
  bool can_put() { return true; }
};

// hides the targets behind exports
class wrapper : public uvm_component
{
 public:
  uvm_master_export<int, std::string> master_export;
  uvm_slave_export<int, std::string> slave_export;
  uvm_blocking_master_export<int, std::string> b_master_export;
  uvm_nonblocking_master_export<int, std::string> nb_master_export;
  uvm_blocking_slave_export<int, std::string> b_slave_export;
  uvm_nonblocking_slave_export<int, std::string> nb_slave_export;
  master_target mt;
  slave_target st;

  UVM_COMPONENT_UTILS(wrapper);

  wrapper( uvm_component_name name )
  : uvm_component(name), master_export("master_export"), slave_export("slave_export"),
    b_master_export("b_master_export"), nb_master_export("nb_master_export"),
    b_slave_export("b_slave_export"), nb_slave_export("nb_slave_export"),
    mt("mt"), st("st")
  {}

  void connect_phase( uvm_phase& phase )
  {
    master_export.connect(mt.imp);
    slave_export.connect(st.imp);
    b_master_export.connect(mt.b_imp);
    nb_master_export.connect(mt.nb_imp);
    b_slave_export.connect(st.b_imp);
    nb_slave_export.connect(st.nb_imp);
  }
};

class initiator : public uvm_component
{
 public:
  uvm_blocking_master_port<int, std::string> b_mport;
  uvm_nonblocking_master_port<int, std::string> nb_mport;
  uvm_master_port<int, std::string> mport;
  uvm_blocking_slave_port<int, std::string> b_sport;
  uvm_nonblocking_slave_port<int, std::string> nb_sport;
  uvm_slave_port<int, std::string> sport;

  UVM_COMPONENT_UTILS(initiator);

  initiator( uvm_component_name name )
  : uvm_component(name), b_mport("b_mport"), nb_mport("nb_mport"), mport("mport"),
    b_sport("b_sport"), nb_sport("nb_sport"), sport("sport")
  {}

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    std::string rsp;
    int req = 0;

    // master ports
    b_mport.put(1);
    b_mport.peek(rsp);
    std::cout << "b_mport peek " << rsp << std::endl;
    b_mport.get(rsp);
    std::cout << "b_mport get " << rsp << std::endl;

    std::cout << "nb_mport can_get " << nb_mport.can_get() << std::endl;
    bool ok = nb_mport.try_put(2);
    std::cout << "nb_mport can_put " << nb_mport.can_put() << " try_put " << ok << std::endl;
    ok = nb_mport.try_peek(rsp);
    std::cout << "nb_mport can_peek " << nb_mport.can_peek() << " try_peek " << ok
              << " " << rsp << std::endl;
    ok = nb_mport.try_get(rsp);
    std::cout << "nb_mport try_get " << ok << " " << rsp << std::endl;
    ok = nb_mport.try_get(rsp);
    std::cout << "nb_mport try_get " << ok << std::endl;

    mport.put(3);
    rsp = mport.peek();
    std::cout << "mport peek " << rsp << std::endl;
    rsp = mport.get();
    std::cout << "mport get " << rsp << std::endl;
    ok = mport.try_put(4);
    std::cout << "mport try_put " << ok << std::endl;
    ok = mport.try_get(rsp);
    std::cout << "mport try_get " << ok << " " << rsp << std::endl;

    // slave ports
    b_sport.peek(req);
    std::cout << "b_sport peek " << req << std::endl;
    b_sport.get(req);
    std::cout << "b_sport get " << req << std::endl;
    b_sport.put(to_rsp(req));

    ok = nb_sport.try_peek(req);
    std::cout << "nb_sport try_peek " << ok << " " << req << std::endl;
    ok = nb_sport.try_get(req);
    std::cout << "nb_sport try_get " << ok << " " << req << std::endl;
    ok = nb_sport.try_put(to_rsp(req));
    std::cout << "nb_sport can_put " << nb_sport.can_put() << " try_put " << ok << std::endl;

    req = sport.get();
    std::cout << "sport get " << req << std::endl;
    sport.put(to_rsp(req));

    phase.drop_objection(this);
  }
};

// master and slave ports connected to a SystemC request-response channel
class channel_master : public uvm_component
{
 public:
  uvm_master_port<int, std::string> port;
  UVM_COMPONENT_UTILS(channel_master);
  channel_master( uvm_component_name name ) : uvm_component(name), port("port") {}

  void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    for (int i = 10; i < 13; i++)
    {
      port.put(i);
      std::string rsp = port.get();
      std::cout << sc_core::sc_time_stamp() << ": channel_master " << i << " -> " << rsp << std::endl;
    }
    phase.drop_objection(this);
  }
};

class channel_slave : public uvm_component
{
 public:
  uvm_slave_port<int, std::string> port;
  UVM_COMPONENT_UTILS(channel_slave);
  channel_slave( uvm_component_name name ) : uvm_component(name), port("port") {}

  void run_phase( uvm_phase& phase )
  {
    while (true)
    {
      int req = port.get();
      sc_core::wait(10, sc_core::SC_NS);
      port.put(to_rsp(req));
    }
  }
};

class test : public uvm_test
{
 public:
  initiator i;
  master_target mt;
  slave_target st;
  wrapper w;
  channel_master cm;
  channel_slave cs;
  tlm::tlm_req_rsp_channel<int, std::string> channel;

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name )
  : uvm_test(name), i("initiator"), mt("mt"), st("st"), w("wrapper"), cm("cm"), cs("cs"),
    channel("channel")
  {}

  void connect_phase( uvm_phase& phase )
  {
    i.b_mport.connect(mt.b_imp);
    i.nb_mport.connect(w.nb_master_export);
    i.mport.connect(w.master_export);
    i.b_sport.connect(st.b_imp);
    i.nb_sport.connect(w.nb_slave_export);
    i.sport.connect(w.slave_export);

    cm.port.connect(channel.master_export);
    cs.port.connect(channel.slave_export);

    std::cout << i.b_mport.get_type_name() << " " << i.nb_mport.get_type_name() << " "
              << i.mport.get_type_name() << std::endl
              << i.b_sport.get_type_name() << " " << i.nb_sport.get_type_name() << " "
              << i.sport.get_type_name() << std::endl
              << w.b_master_export.get_type_name() << " " << w.nb_master_export.get_type_name()
              << " " << w.master_export.get_type_name() << std::endl
              << w.b_slave_export.get_type_name() << " " << w.nb_slave_export.get_type_name()
              << " " << w.slave_export.get_type_name() << std::endl
              << mt.b_imp.get_type_name() << " " << mt.nb_imp.get_type_name() << " "
              << mt.imp.get_type_name() << std::endl
              << st.b_imp.get_type_name() << " " << st.nb_imp.get_type_name() << " "
              << st.imp.get_type_name() << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
