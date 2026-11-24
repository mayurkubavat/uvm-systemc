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

// Test of the request and response implementations of master and slave
// imps: an implementation imp which is not of the type REQ_IMP or
// RSP_IMP, without req_imp or rsp_imp, is a fatal error at construction,
// and an implementation imp of a class without virtual member functions
// can be used when req_imp and rsp_imp are given.

#include <systemc>
#include <tlm>
#include <uvm>

#include <iostream>
#include <stdexcept>

using namespace uvm;

struct req_sink
{
  void put( const int& req ) { std::cout << "req_sink put " << req << std::endl; }
  bool try_put( const int& req ) { put(req); return true; }
  bool can_put() { return true; }
};

struct rsp_source
{
  int get() { return 7; }
  int peek() const { return 7; }
  bool try_get( int& rsp ) { rsp = 7; return true; }
  bool can_get() { return true; }
  bool try_peek( int& rsp ) const { rsp = 7; return true; }
  bool can_peek() const { return true; }
};

// no virtual member functions
struct plain_target
{
  req_sink sink;
  rsp_source source;
};

class target : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(target);

  // req_imp and rsp_imp are left out, and target is neither a req_sink
  // nor a rsp_source
  uvm_master_imp<int, int, target, req_sink, rsp_source> imp;

  target( uvm_component_name name ) : uvm_component(name), imp("imp", this) {}
};

class plain_holder : public uvm_component
{
 public:
  UVM_COMPONENT_UTILS(plain_holder);

  plain_target pt;
  uvm_master_imp<int, int, plain_target, req_sink, rsp_source> imp;

  plain_holder( uvm_component_name name )
  : uvm_component(name), imp("imp", &pt, &pt.sink, &pt.source) {}
};

int sc_main(int, char*[])
{
  plain_holder h("h");
  h.imp.put(1);
  std::cout << "plain imp get " << h.imp.get() << std::endl;

  try
  {
    target t("t");
    std::cout << "error: the imp without implementation was constructed" << std::endl;
  }
  catch (const std::runtime_error& e)
  {
    std::cout << "caught: " << e.what() << std::endl;
  }
  return 0;
}
