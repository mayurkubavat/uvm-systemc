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

// Test of uvm_barrier: processes wait at the barrier until as many of
// them as its threshold have arrived, and are then released together.

#include <systemc>
#include <uvm>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_barrier* barrier;
  std::vector<std::string> passed;

  test( uvm_component_name name ) : uvm_test(name), barrier(nullptr) {}

  // arrives at the barrier after 'delay' ns
  void arrive( std::string name, int delay )
  {
    sc_core::wait(delay, sc_core::SC_NS);
    barrier->wait_for();
    passed.push_back(sc_core::sc_time_stamp().to_string() + " " + name);
  }

  void spawn( const std::string& name, int delay )
  {
    sc_core::sc_spawn(sc_bind(&test::arrive, this, name, delay));
  }

  // The processes released together resume in an order which is not
  // specified, so those which passed are printed sorted.
  void status( const std::string& what )
  {
    std::sort(passed.begin(), passed.end());
    for (unsigned int i = 0; i < passed.size(); i++)
      std::cout << "  passed at " << passed[i] << std::endl;
    passed.clear();

    std::cout << sc_core::sc_time_stamp() << ": " << what << ": "
              << barrier->get_num_waiters() << " waiting, threshold "
              << barrier->get_threshold() << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    barrier = uvm_barrier_pool::get_global("sync");
    std::cout << "barrier from the pool: " << barrier->get_name() << std::endl;
    barrier->set_threshold(3);

    // three arrive at 10, 20 and 30 ns, all pass at 30 ns
    spawn("a1", 10); spawn("a2", 20); spawn("a3", 30);
    sc_core::wait(25, sc_core::SC_NS);
    status("two arrived");
    sc_core::wait(25, sc_core::SC_NS);
    status("after the third");

    // the barrier closed again by itself: the next one waits
    spawn("b1", 0);
    sc_core::wait(10, sc_core::SC_NS);
    status("one more arrived");

    // lowering the threshold to the number waiting releases them
    barrier->set_threshold(1);
    sc_core::wait(10, sc_core::SC_NS);
    status("threshold lowered");

    // reset with wakeup releases, cancel takes one waiter off the count
    barrier->set_threshold(4);
    spawn("c1", 0); spawn("c2", 0);
    sc_core::wait(10, sc_core::SC_NS);
    barrier->cancel();
    status("after cancel");
    barrier->reset(true);
    sc_core::wait(10, sc_core::SC_NS);
    status("after reset");

    // reset without wakeup leaves the waiting processes behind: they do
    // not pass with those which arrive afterwards
    barrier->set_threshold(3);
    spawn("e1", 0); spawn("e2", 0);
    sc_core::wait(5, sc_core::SC_NS);
    barrier->reset(false);
    status("after reset without wakeup");
    spawn("e3", 0); spawn("e4", 0);
    sc_core::wait(5, sc_core::SC_NS);
    status("two new ones");
    spawn("e5", 0);
    sc_core::wait(5, sc_core::SC_NS);
    status("the third new one");

    // without auto reset the barrier stays open once it was reached
    barrier->set_auto_reset(false);
    barrier->set_threshold(2);
    spawn("d1", 0); spawn("d2", 5); spawn("d3", 10);
    sc_core::wait(20, sc_core::SC_NS);
    status("barrier stays open");

    barrier->print();

    phase.drop_objection(this);
  }
};

int sc_main(int, char*[])
{
  run_test("test");
  return 0;
}
