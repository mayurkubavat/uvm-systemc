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

// Unit test for uvm_default_report_server transaction recording:
// set_message_database / get_message_database, do_copy preservation,
// UVM_RM_RECORD action & record_all_messages, uvm_report_message
// verbosity bounds & line bit-width, DB switching, freed stream reopen,
// and null report_object/report_handler safety.

#include <systemc>
#include <uvm>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace uvm;

static std::string read_file_str( const std::string& path )
{
  std::ifstream ifs(path.c_str());
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

class report_db_test : public uvm_test
{
 public:
  bool all_ok;

  explicit report_db_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(report_db_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("RPT_DB_TEST", "FAILED: " + desc);
    }
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    const std::string db1_file = "rpt_msg_db1.log";
    const std::string db2_file = "rpt_msg_db2.log";
    std::remove(db1_file.c_str());
    std::remove(db2_file.c_str());

    uvm_text_tr_database* db1 = new uvm_text_tr_database("db1");
    db1->set_file_name(db1_file);
    db1->open_db();

    uvm_text_tr_database* db2 = new uvm_text_tr_database("db2");
    db2->set_file_name(db2_file);
    db2->open_db();

    uvm_default_report_server* svr =
      dynamic_cast<uvm_default_report_server*>(uvm_report_server::get_server());
    check(svr != nullptr, "default report server is uvm_default_report_server");
    check(svr->get_message_database() == nullptr, "initial message_database is nullptr");

    svr->set_message_database(db1);
    check(svr->get_message_database() == db1, "set/get_message_database returns db1");

    // Verify uvm_report_server::do_copy copies message_database
    {
      uvm_default_report_server copy_svr("copy_svr");
      copy_svr.copy(*svr);
      check(copy_svr.get_message_database() == db1,
            "uvm_report_server::do_copy copies message_database");
    }

    // Enable UVM_DEBUG verbosity so UVM_HIGH/UVM_FULL/UVM_DEBUG reach report server
    set_report_verbosity_level(UVM_DEBUG);
    // Suppress UVM_DISPLAY for TEST_REC id and enable UVM_RM_RECORD
    set_report_id_action("TEST_REC", UVM_RM_RECORD);
    uvm_report_info("TEST_REC", "msg_none_line0", UVM_NONE, "f0.cpp", 0);
    uvm_report_info("TEST_REC", "msg_low_line0", UVM_LOW);
    uvm_report_info("TEST_REC", "msg_med_line42", UVM_MEDIUM, "f1.cpp", 42);
    uvm_report_info("TEST_REC", "msg_high", UVM_HIGH, "f2.cpp", 7);
    uvm_report_info("TEST_REC", "msg_full", UVM_FULL, "f3.cpp", 8);
    uvm_report_info("TEST_REC", "msg_debug", UVM_DEBUG, "f4.cpp", 9);

    // Also test record_all_messages (use UVM_COUNT so action != UVM_NO_ACTION reaches execute_report_message without stdout)
    set_report_id_action("TEST_ALL", UVM_COUNT);
    svr->record_all_messages = true;
    uvm_report_info("TEST_ALL", "recorded_via_flag", UVM_MEDIUM);
    svr->record_all_messages = false;

    // Test uvm_report_message::sprint() to verify do_print verbosity & line fixes
    {
      uvm_report_message* msg = uvm_report_message::new_report_message("msg_prt");
      msg->set_report_message(UVM_WARNING, "PRT_ID", "hello_print", UVM_DEBUG, "p.cpp", 0, "");
      std::string p_str = msg->sprint();
      check(p_str.find("UVM_DEBUG") != std::string::npos,
            "uvm_report_message::do_print prints UVM_DEBUG safely");
    }

    // Free cached stream and emit another message -> must reopen without warning
    {
      std::vector<uvm_tr_stream*> streams;
      db1->get_streams(streams);
      check(!streams.empty(), "db1 opened a MESSAGES stream");
      if (!streams.empty())
        streams[0]->free();
      uvm_report_info("TEST_REC", "after_stream_free", UVM_LOW);
    }

    // Switch to db2 -> must clear stale stream cache and record into db2
    svr->set_message_database(db2);
    check(svr->get_message_database() == db2, "switched message database to db2");
    uvm_report_info("TEST_REC", "msg_in_db2", UVM_LOW, "db2.cpp", 99);

    // Direct execute_report_message with null report_object & report_handler
    {
      uvm_report_message* direct_msg = uvm_report_message::new_report_message("direct_msg");
      direct_msg->set_report_object(nullptr);
      direct_msg->set_report_handler(nullptr);
      direct_msg->set_report_message(UVM_INFO, "DIRECT_REC", "null_ro_rh", UVM_LOW, "", 0, "");
      direct_msg->set_action(UVM_RM_RECORD);
      svr->execute_report_message(direct_msg, "");
    }

    // Restore server DB to nullptr before destroying db1/db2
    svr->set_message_database(nullptr);
    db1->close_db();
    db2->close_db();
    delete db1;
    delete db2;

    const std::string c1 = read_file_str(db1_file);
    const std::string c2 = read_file_str(db2_file);

    check(c1.find("MESSAGES") != std::string::npos, "db1 has MESSAGES stream");
    check(c1.find("msg_none_line0") != std::string::npos, "db1 recorded UVM_NONE line=0");
    check(c1.find("msg_low_line0") != std::string::npos, "db1 recorded UVM_LOW line=0");
    check(c1.find("msg_med_line42") != std::string::npos, "db1 recorded UVM_MEDIUM line=42");
    check(c1.find("UVM_DEBUG") != std::string::npos, "db1 recorded UVM_DEBUG verbosity");
    check(c1.find("recorded_via_flag") != std::string::npos, "db1 recorded via record_all_messages");
    check(c1.find("after_stream_free") != std::string::npos, "db1 recorded after stream free");
    check(c1.find("msg_in_db2") == std::string::npos, "db1 does not contain db2 message");

    check(c2.find("msg_in_db2") != std::string::npos, "db2 recorded after DB switch");
    check(c2.find("null_ro_rh") != std::string::npos, "db2 recorded null ro/rh message");

    std::remove(db1_file.c_str());
    std::remove(db2_file.c_str());

    std::cout << "report_db_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_test::report_phase(phase);
    uvm_report_server* svr = uvm_coreservice_t::get()->get_report_server();
    if (all_ok &&
        svr->get_severity_count(UVM_WARNING) == 0 &&
        svr->get_severity_count(UVM_ERROR) == 0 &&
        svr->get_severity_count(UVM_FATAL) == 0)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("report_db_test");
  return 0;
}
