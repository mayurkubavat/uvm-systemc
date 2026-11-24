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

// Reports with the UVM_LOG action go to the file selected by the report
// handler: the (severity,id) file, else the id file, else the severity
// file, else the default file. The file is selected before the report
// catchers run, so a demoted warning still goes to the warning file.
// report_summarize(file) and
// uvm_report_catcher::summarize_report_catcher(file) send the summary
// to the given file instead of the standard output, and leave the
// action and file settings of the reporter as they were.

#include <systemc>
#include <uvm>

#include <sstream>

using namespace uvm;

class demote_catcher : public uvm_report_catcher
{
public:
  demote_catcher( const std::string& name = "demote_catcher" ) : uvm_report_catcher(name) {}

  virtual action_e do_catch()
  {
    if (get_id() == "DEMOTE")
      set_severity(UVM_INFO);
    return THROW;
  }
};

static void show_file( const std::string& name, std::ostringstream& s )
{
  std::cout << "---- " << name << " ----" << std::endl;
  std::cout << s.str();
  std::cout << "---- end of " << name << " ----" << std::endl;
}

class test : public uvm_test
{
public:
  std::ostringstream f_default, f_warning, f_id, f_error_id;
  std::ostringstream f_catcher, f_summary;
  demote_catcher* catcher{nullptr};

  UVM_COMPONENT_UTILS(test);

  test( uvm_component_name name ) : uvm_test(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    catcher = new demote_catcher();
    uvm_report_cb::add(nullptr, catcher);

    set_report_severity_action(UVM_INFO, UVM_LOG);
    set_report_severity_action(UVM_WARNING, UVM_LOG | UVM_COUNT);
    set_report_severity_action(UVM_ERROR, UVM_LOG | UVM_COUNT);

    set_report_default_file(&f_default);
    set_report_severity_file(UVM_WARNING, &f_warning);
    set_report_id_file("ID", &f_id);
    set_report_severity_id_file(UVM_ERROR, "ID", &f_error_id);

    UVM_INFO("OTHER", "info to the default file", UVM_LOW);
    UVM_WARNING("OTHER", "warning to the severity file");
    UVM_INFO("ID", "info to the id file", UVM_LOW);
    UVM_WARNING("ID", "warning to the id file");
    UVM_ERROR("ID", "error to the (severity,id) file");
    UVM_WARNING("DEMOTE", "demoted to info, still to the severity file");

    set_report_severity_action(UVM_INFO, UVM_DISPLAY);
    set_report_severity_action(UVM_WARNING, UVM_DISPLAY | UVM_COUNT);
    set_report_severity_action(UVM_ERROR, UVM_DISPLAY | UVM_COUNT);
    set_report_default_file(0);

    show_file("default file", f_default);
    show_file("warning file", f_warning);
    show_file("id file", f_id);
    show_file("error,id file", f_error_id);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_root* root = uvm_root::get();
    uvm_report_server* svr = uvm_report_server::get_server();

    std::cout << "summarize_report_catcher to a file" << std::endl;
    uvm_report_catcher::summarize_report_catcher(&f_catcher);
    show_file("catcher file", f_catcher);

    std::cout << "report_summarize to a file" << std::endl;
    svr->report_summarize(&f_summary);
    show_file("summary file", f_summary);

    std::cout << "UVM/REPORT/SERVER action is UVM_DISPLAY: "
              << (root->get_report_action(UVM_INFO, "UVM/REPORT/SERVER") == UVM_DISPLAY)
              << ", file is the default: "
              << (root->get_report_file_handle(UVM_INFO, "UVM/REPORT/SERVER") == nullptr)
              << std::endl;
    std::cout << "UVM/REPORT/CATCHER action is UVM_DISPLAY: "
              << (root->get_report_action(UVM_INFO, "UVM/REPORT/CATCHER") == UVM_DISPLAY)
              << ", file is the default: "
              << (root->get_report_file_handle(UVM_INFO, "UVM/REPORT/CATCHER") == nullptr)
              << std::endl;

    uvm_report_cb::do_delete(nullptr, catcher);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
