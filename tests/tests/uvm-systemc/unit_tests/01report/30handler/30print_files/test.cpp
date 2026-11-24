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

// uvm_report_handler::do_print shows the file of each file setting:
// "<null>" for no file, the standard streams by name, and any other
// stream by its address.

#include <systemc>
#include <uvm>

#include <sstream>

using namespace uvm;

// true if the UVM_FILE row of the printed table for name shows value
static bool shows( const std::string& table, const std::string& name,
                   const std::string& value )
{
  std::istringstream in(table);
  std::string line;
  while (std::getline(in, line))
  {
    std::istringstream row(line);
    std::string n, type, size, val;
    row >> n >> type >> size >> val;
    if (n == name && type == "UVM_FILE")
      return val == value;
  }
  return false;
}

class test : public uvm_test
{
public:
  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    std::ostringstream log;

    set_report_id_file("TO_LOG", &log);
    set_report_id_file("TO_ERR", &std::cerr);
    set_report_severity_file(UVM_FATAL, &std::cout);

    std::string s = get_report_handler()->sprint();

    std::ostringstream addr;
    addr << "@" << static_cast<const void*>(&log);

    std::cout << "default file is <null>: " << shows(s, "default_file_handle", "<null>") << std::endl;
    std::cout << "TO_LOG shows the stream address: " << shows(s, "[TO_LOG]", addr.str()) << std::endl;
    std::cout << "TO_ERR shows stderr: " << shows(s, "[TO_ERR]", "stderr") << std::endl;
    std::cout << "UVM_FATAL shows stdout: " << shows(s, "[UVM_FATAL]", "stdout") << std::endl;
    std::cout << "UVM_INFO shows <null>: " << shows(s, "[UVM_INFO]", "<null>") << std::endl;

    set_report_id_file("TO_LOG", 0);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
