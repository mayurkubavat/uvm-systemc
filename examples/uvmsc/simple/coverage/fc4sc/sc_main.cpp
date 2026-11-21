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

#include <systemc>
#include <uvm>

#include "coverage_subscriber.h"
#include "monitor.h"

class testbench : public uvm::uvm_env
{
 public:
  monitor* mon;
  coverage_subscriber* cov;

  testbench( uvm::uvm_component_name name )
  : uvm::uvm_env(name),
    mon(nullptr),
    cov(nullptr)
  {}

  UVM_COMPONENT_UTILS(testbench);

  virtual void build_phase( uvm::uvm_phase& phase )
  {
    uvm::uvm_env::build_phase(phase);

    mon = monitor::type_id::create("mon", this);
    cov = coverage_subscriber::type_id::create("cov", this);
  }

  virtual void connect_phase( uvm::uvm_phase& phase )
  {
    mon->item_collected_port.connect(cov->analysis_export);
  }
};

int sc_main(int, char*[])
{
  testbench tb("tb");

  uvm::run_test();

  return 0;
}
