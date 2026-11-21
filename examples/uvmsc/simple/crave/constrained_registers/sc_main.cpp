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

#include <iostream>
#include <set>

#include <systemc>
#include <uvm>
#include <crave2uvm.h>

#include "timer_reg_model.h"

#define NUM_RANDOMIZATIONS 200

// Randomizes the register model and checks that the desired values of
// the registers meet the constraints each time.

class test : public uvm::uvm_test
{
 public:
  timer_reg_block* model;

  unsigned int num_violations;
  std::set<unsigned int> prescales, periods, duties;

  UVM_COMPONENT_UTILS(test);

  test( uvm::uvm_component_name name )
  : uvm::uvm_test(name), model(nullptr), num_violations(0)
  {
    // CRAVE follows the global seed of UVM, so the values are the same
    // in every run and change with the command line argument +UVM_SEED
    crave::set_global_seed(uvm::uvm_get_random_seed());
  }

  void build_phase( uvm::uvm_phase& phase )
  {
    uvm::uvm_test::build_phase(phase);

    model = new timer_reg_block("model");
    model->build();
    model->reset();
  }

  void run_phase( uvm::uvm_phase& phase )
  {
    phase.raise_objection(this);

    for (int i = 0; i < NUM_RANDOMIZATIONS; i++)
    {
      if (!model->randomize())
        UVM_ERROR(this->get_name(), "Randomization of the register model failed");

      unsigned int prescale = model->prescale->get().to_uint();
      unsigned int period   = model->period->get().to_uint();
      unsigned int duty     = model->duty->get().to_uint();

      if (timer_constraints::valid_prescales().count(prescale) == 0 ||
          period < 16 || period > 1000 || duty == 0 || duty >= period)
      {
        num_violations++;
        UVM_ERROR(this->get_name(), "The register values violate the constraints");
      }

      prescales.insert(prescale);
      periods.insert(period);
      duties.insert(duty);
    }

    phase.drop_objection(this);
  }

  void check_phase( uvm::uvm_phase& phase )
  {
    // Constraints which hold do not make the values random. The values
    // have to be spread over the solutions of the constraints as well.
    // The solver does not give a uniform distribution, so the limits
    // are far below the number of randomizations: they only detect a
    // generator which returns nearly the same solution each time. Over
    // 2000 seeds the fewest different values were 80 periods and 8 duty
    // values.
    if (prescales.size() != timer_constraints::valid_prescales().size())
      UVM_ERROR(this->get_name(), "Not all prescaler values have been generated");

    if (periods.size() < NUM_RANDOMIZATIONS / 8)
      UVM_ERROR(this->get_name(), "Too few different periods have been generated");

    if (duties.size() < NUM_RANDOMIZATIONS / 50)
      UVM_ERROR(this->get_name(), "Too few different duty values have been generated");

    // the new values are desired values, which update would write
    if (!model->needs_update())
      UVM_ERROR(this->get_name(), "The register model does not need an update");
  }

  // The generated values depend on the seed and on the solver, so only
  // the part between the tags is compared with the golden log file.
  void report_phase( uvm::uvm_phase& phase )
  {
    unsigned int errors =
      uvm::uvm_report_server::get_server()->get_severity_count(uvm::UVM_ERROR);

    std::cout << "different values: " << prescales.size() << " prescales, "
              << periods.size() << " periods, " << duties.size() << " duties" << std::endl;
    std::cout << "test started" << std::endl;
    std::cout << "randomizations: " << NUM_RANDOMIZATIONS << std::endl;
    std::cout << "constraint violations: " << num_violations << std::endl;
    std::cout << "errors: " << errors << std::endl;
    std::cout << "test ended" << std::endl;
  }
};

int sc_main(int, char*[])
{
  // read the settings of CRAVE; the file is created if it does not exist
  crave::init("crave.cfg");

  uvm::run_test("test");

  return 0;
}
