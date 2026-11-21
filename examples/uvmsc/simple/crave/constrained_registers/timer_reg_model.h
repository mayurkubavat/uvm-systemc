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

#ifndef TIMER_REG_MODEL_H_
#define TIMER_REG_MODEL_H_

#include <set>
#include <string>

#include <systemc>
#include <uvm>
#include <crave2uvm.h>

// Register model of a timer whose registers depend on each other: the
// duty register has to be smaller than the period register. The model
// states this as CRAVE constraints and solves them when it is
// randomized.

//----------------------------------------------------------------------
// The constraints between the values of the registers
//----------------------------------------------------------------------

class timer_constraints : public crave::crv_sequence_item
{
 public:
  crave::crv_variable<unsigned int> prescale;
  crave::crv_variable<unsigned int> period;
  crave::crv_variable<unsigned int> duty;

  // only powers of two are valid prescaler values
  crave::crv_constraint c_prescale {
    crave::inside(prescale(), valid_prescales())
  };

  crave::crv_constraint c_period { period() >= 16u, period() <= 1000u };

  // the output is never constant
  crave::crv_constraint c_duty { duty() > 0u, duty() < period() };

  timer_constraints( crave::crv_object_name = "timer_constraints" )
  {}

  static const std::set<unsigned int>& valid_prescales()
  {
    static const std::set<unsigned int> prescales = { 1, 2, 4, 8 };
    return prescales;
  }
};

//----------------------------------------------------------------------
// The registers
//----------------------------------------------------------------------

class timer_reg : public uvm::uvm_reg
{
 public:
  uvm::uvm_reg_field* value;

  timer_reg( const std::string& name, unsigned int size )
  : uvm::uvm_reg(name, 32, uvm::UVM_NO_COVERAGE), value(nullptr), m_size(size)
  {}

  virtual void build()
  {
    value = uvm::uvm_reg_field::type_id::create("value");
    //                     size  lsb access volatile reset has_reset is_rand indiv
    value->configure(this, m_size, 0, "RW", false,   1,    true,     true,   false);
  }

 private:
  unsigned int m_size;
};

//----------------------------------------------------------------------
// The register block
//----------------------------------------------------------------------

class timer_reg_block : public uvm::uvm_reg_block
{
 public:
  timer_reg* prescale;
  timer_reg* period;
  timer_reg* duty;

  timer_constraints constraints;

  timer_reg_block( const std::string& name = "timer_reg_block" )
  : uvm::uvm_reg_block(name, uvm::UVM_NO_COVERAGE),
    prescale(nullptr), period(nullptr), duty(nullptr)
  {}

  virtual void build()
  {
    prescale = add("prescale", 4, 0x0);
    period   = add("period", 16, 0x4);
    duty     = add("duty", 16, 0x8);

    lock_model();
  }

  // Called by randomize after the registers got unconstrained random
  // values. Replace them by a solution of the constraints.
  virtual void post_randomize()
  {
    if (!constraints.randomize())
      UVM_ERROR("TIMER_REGS", "The constraints of the registers have no solution");

    prescale->value->set(static_cast<unsigned int>(constraints.prescale));
    period->value->set(static_cast<unsigned int>(constraints.period));
    duty->value->set(static_cast<unsigned int>(constraints.duty));
  }

 private:
  timer_reg* add( const std::string& name, unsigned int size, unsigned int offset )
  {
    if (default_map == nullptr)
      default_map = create_map("default_map", 0x0, 4, uvm::UVM_LITTLE_ENDIAN);

    timer_reg* rg = new timer_reg(name, size);
    rg->configure(this);
    rg->build();
    default_map->add_reg(rg, offset, "RW");
    return rg;
  }
};

#endif /* TIMER_REG_MODEL_H_ */
