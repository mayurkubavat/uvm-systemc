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

// Unit test for uvm_process:
// self(), from_handle(), status() (RUNNING/WAITING/SUSPENDED/FINISHED/KILLED),
// kill() (external & self), await() (other/finished/self/SC_METHOD guard),
// suspend()/resume(), srandom()/get_randstate()/set_randstate().

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

class proc_await_catcher : public uvm_report_catcher
{
 public:
  int await_err_count;

  proc_await_catcher() : await_err_count(0) {}

  virtual action_e do_catch()
  {
    if (get_id() == "UVM/PROC/AWAIT" && get_severity() == UVM_ERROR)
    {
      await_err_count++;
      return CAUGHT;
    }
    return THROW;
  }
};

class uvm_process_test : public uvm_test
{
 public:
  bool all_ok;

  explicit uvm_process_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(uvm_process_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("PROC_TEST", "FAILED: " + desc);
    }
  }

  void worker_body( uvm_process** out_self, int* counter, bool self_kill )
  {
    uvm_process* p = uvm_process::self();
    if (out_self != nullptr)
      *out_self = p;
    if (p != nullptr && p->status() == uvm_process::RUNNING)
      (*counter)++;
    sc_core::wait(10, sc_core::SC_NS);
    if (self_kill && p != nullptr)
      p->kill();
    (*counter)++;
    sc_core::wait(10, sc_core::SC_NS);
    (*counter)++;
  }

  void method_body( uvm_process* target )
  {
    uvm_process* m = uvm_process::self();
    check(m != nullptr, "SC_METHOD has non-null uvm_process::self()");
    if (m != nullptr && target != nullptr)
      target->await(); // must emit UVM/PROC/AWAIT without crashing
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // 1. self() & identity stability inside SC_THREAD
    uvm_process* main_p1 = uvm_process::self();
    uvm_process* main_p2 = uvm_process::self();
    check(main_p1 != nullptr, "self() non-null inside run_phase SC_THREAD");
    check(main_p1 == main_p2, "repeated self() returns identical uvm_process*");
    check(main_p1->status() == uvm_process::RUNNING, "current thread status is RUNNING");
    check(main_p1->get_process_handle().valid(), "get_process_handle() is valid");
    check(uvm_process::from_handle(sc_core::sc_process_handle()) == nullptr,
          "from_handle(invalid) returns nullptr");

    // Self-await must not deadlock
    main_p1->await();
    check(main_p1->status() == uvm_process::RUNNING, "self-await returns immediately");

    // 2. Spawn worker, check WAITING -> suspend -> SUSPENDED -> resume -> FINISHED + await
    uvm_process* w1_self = nullptr;
    int w1_cnt = 0;
    sc_core::sc_process_handle h1 = sc_core::sc_spawn(
      sc_bind(&uvm_process_test::worker_body, this, &w1_self, &w1_cnt, false));
    uvm_process* w1 = uvm_process::from_handle(h1);
    check(w1 != nullptr, "from_handle(spawned) non-null");
    check(w1->status() == uvm_process::WAITING, "newly spawned thread is WAITING");

    w1->suspend();
    check(w1->status() == uvm_process::SUSPENDED, "suspend() transitions status to SUSPENDED");
    sc_core::wait(25, sc_core::SC_NS);
    check(w1_cnt == 0, "suspended thread did not advance");

    w1->resume();
    w1->await();
    check(w1 == w1_self, "worker's self() matches parent's from_handle()");
    check(w1_cnt == 3, "resumed worker ran all steps to completion");
    check(w1->status() == uvm_process::FINISHED, "completed worker status is FINISHED");
    w1->await(); // awaiting already finished process returns immediately

    // 3. External kill()
    uvm_process* w2_self = nullptr;
    int w2_cnt = 0;
    sc_core::sc_process_handle h2 = sc_core::sc_spawn(
      sc_bind(&uvm_process_test::worker_body, this, &w2_self, &w2_cnt, false));
    uvm_process* w2 = uvm_process::from_handle(h2);
    sc_core::wait(5, sc_core::SC_NS);
    check(w2_cnt == 1, "w2 ran first step");
    w2->kill();
    w2->await();
    check(w2->status() == uvm_process::KILLED, "external kill() sets status to KILLED");
    check(w2_cnt == 1, "killed w2 did not execute post-kill steps");

    // 4. Self kill()
    uvm_process* w3_self = nullptr;
    int w3_cnt = 0;
    sc_core::sc_process_handle h3 = sc_core::sc_spawn(
      sc_bind(&uvm_process_test::worker_body, this, &w3_self, &w3_cnt, true));
    uvm_process* w3 = uvm_process::from_handle(h3);
    w3->await();
    check(w3->status() == uvm_process::KILLED, "self-kill sets status to KILLED");
    check(w3_cnt == 1, "self-killed w3 unwound before increment");

    // 5. SC_METHOD calling await() emits UVM/PROC/AWAIT error safely
    {
      proc_await_catcher ctchr;
      uvm_report_cb::add(nullptr, &ctchr);

      uvm_process* w4_self = nullptr;
      int w4_cnt = 0;
      sc_core::sc_process_handle h4 = sc_core::sc_spawn(
        sc_bind(&uvm_process_test::worker_body, this, &w4_self, &w4_cnt, false));
      uvm_process* w4 = uvm_process::from_handle(h4);

      sc_core::sc_spawn_options m_opts;
      m_opts.spawn_method();
      sc_core::sc_spawn(
        sc_bind(&uvm_process_test::method_body, this, w4),
        "await_method", &m_opts);

      w4->await();
      check(ctchr.await_err_count == 1, "SC_METHOD await() caught 1 UVM/PROC/AWAIT error");
      uvm_report_cb::do_delete(nullptr, &ctchr);
    }

    // 6. srandom / get_randstate / set_randstate
    main_p1->srandom(12345u);
    const std::string saved_state = main_p1->get_randstate();
    std::vector<std::mt19937::result_type> seq1, seq2;
    for (int i = 0; i < 4; ++i)
      seq1.push_back(main_p1->get_rng()());

    main_p1->set_randstate(saved_state);
    for (int i = 0; i < 4; ++i)
      seq2.push_back(main_p1->get_rng()());
    check(seq1 == seq2, "get_randstate/set_randstate reproduces identical RNG sequence");

    std::cout << "uvm_process_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
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
  bool pre_sim_null = (uvm_process::self() == nullptr);
  if (!pre_sim_null)
    std::cout << "UNEXPECTED_NON_NULL_SELF_BEFORE_SC_START" << std::endl;
  run_test("uvm_process_test");
  return 0;
}
