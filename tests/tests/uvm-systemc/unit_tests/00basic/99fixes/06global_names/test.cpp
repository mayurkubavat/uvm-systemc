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

// Regression test for upstream issue #353. The headers do not declare
// uvm_* names at global scope, so the names of namespace uvm can be
// used unqualified at global scope after a using-directive.

#include <systemc>
#include <uvm>

#include <iostream>

using namespace uvm;

uvm_object* global_object = nullptr;
uvm_printer* global_printer = nullptr;
uvm_event_callback* global_callback = nullptr;

int sc_main(int, char*[])
{
  uvm_event* ev = new uvm_event("ev");
  global_object = ev;

  std::cout << "global uvm_object is " << global_object->get_name() << std::endl;
  std::cout << "printer and callback are "
            << ((global_printer == nullptr && global_callback == nullptr) ? "unset" : "set")
            << std::endl;
  std::cout << "** UVM TEST PASSED **" << std::endl;

  delete ev;
  return 0;
}
