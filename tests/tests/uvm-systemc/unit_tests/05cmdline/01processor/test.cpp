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

// Test of the uvm_cmdline_processor query interface. The test supplies
// its own command line by calling sc_elab_and_sim, since the regression
// runs every test without arguments.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

void show( const std::string& what, const std::vector<std::string>& v )
{
  std::cout << what << " (" << v.size() << "):";
  for (unsigned int i = 0; i < v.size(); i++)
    std::cout << " [" << v[i] << "]";
  std::cout << std::endl;
}

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_cmdline_processor* clp = uvm_cmdline_processor::get_inst();
    std::vector<std::string> v;
    std::string value;
    int n;

    if (clp != uvm_cmdline_processor::get_inst())
      UVM_ERROR("TEST", "get_inst does not return a singleton");

    clp->get_args(v);
    show("args", v);

    clp->get_plusargs(v);
    show("plusargs", v);

    clp->get_uvm_args(v);
    show("uvm_args", v);

    n = clp->get_arg_matches("+foo", v);
    std::cout << "get_arg_matches +foo returns " << n << std::endl;
    show("matches +foo", v);

    n = clp->get_arg_matches("/.*foo.*/", v);
    show("matches /.*foo.*/", v);

    n = clp->get_arg_matches("/^-.*o$/", v);
    show("matches /^-.*o$/", v);

    n = clp->get_arg_matches("+none", v);
    std::cout << "get_arg_matches +none returns " << n << std::endl;
    show("matches +none", v);

    value = "unchanged";
    n = clp->get_arg_value("+foo=", value);
    std::cout << "get_arg_value +foo= returns " << n
              << ", value [" << value << "]" << std::endl;

    value = "unchanged";
    n = clp->get_arg_value("+none=", value);
    std::cout << "get_arg_value +none= returns " << n
              << ", value [" << value << "]" << std::endl;

    n = clp->get_arg_values("+foo=", v);
    std::cout << "get_arg_values +foo= returns " << n << std::endl;
    show("values +foo=", v);

    n = clp->get_arg_values("+none=", v);
    show("values +none=", v);

    std::cout << "tool name [" << clp->get_tool_name() << "]" << std::endl;

    if (clp->get_tool_version() != sc_core::sc_release())
      UVM_ERROR("TEST", "unexpected tool version " + clp->get_tool_version());

    std::cout << "** UVM TEST PASSED **" << std::endl;
  }
};

int sc_main(int, char*[])
{
  test test1("test1");
  run_test();
  return 0;
}

int main(int, char*[])
{
  const char* argv[] = {
    "test",
    "+foo=1,yes,on",
    "-uvmbar",
    "plain",
    "+UVM_SOMETHING",
    "+foobar",
    "-foo",
    "+barfoo",
    "+foo=5,no,off",
    "+uVm_mixed=1",
    "-uv"
  };
  int argc = sizeof(argv) / sizeof(argv[0]);

  return sc_core::sc_elab_and_sim(argc, const_cast<char**>(argv));
}
