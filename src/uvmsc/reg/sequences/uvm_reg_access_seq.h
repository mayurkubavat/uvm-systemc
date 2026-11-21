// -------------------------------------------------------------
//    Copyright 2010 AMD
//    Copyright 2010-2018 Cadence Design Systems, Inc.
//    Copyright 2010-2011 Mentor Graphics Corporation
//    Copyright 2015-2024 NVIDIA Corporation
//    Copyright 2020 Semifore
//    Copyright 2004-2010 Synopsys, Inc.
//    Copyright 2026 Mayur Kubavat
//    All Rights Reserved Worldwide
//
//    Licensed under the Apache License, Version 2.0 (the
//    "License"); you may not use this file except in
//    compliance with the License.  You may obtain a copy of
//    the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in
//    writing, software distributed under the License is
//    distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.  See
//    the License for the specific language governing
//    permissions and limitations under the License.
// -------------------------------------------------------------

#ifndef UVM_REG_ACCESS_SEQ_H_
#define UVM_REG_ACCESS_SEQ_H_

#include <systemc>
#include <string>
#include <sstream>
#include <vector>

#include "uvmsc/conf/uvm_resource_db.h"
#include "uvmsc/reg/uvm_reg_sequence.h"
#include "uvmsc/reg/uvm_reg_block.h"
#include "uvmsc/reg/uvm_reg_map.h"
#include "uvmsc/reg/uvm_reg_model.h"
#include "uvmsc/reg/uvm_reg.h"
#include "uvmsc/reg/uvm_reg_field.h"

namespace uvm
{

//----------------------------------------------------------------------
// Title: Register Access Test Sequences
//----------------------------------------------------------------------
// This section defines sequences that test DUT register access via the
// available frontdoor and backdoor paths defined in the provided register
// model.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Class: uvm_reg_single_access_seq
//
//! Verify the accessibility of a register
//! by writing through its default address map
//! then reading it via the backdoor, then reversing the process,
//! making sure that the resulting value matches the mirrored value.
//!
//! If bit-type resource named
//! "NO_REG_TESTS" or "NO_REG_ACCESS_TEST"
//! in the "REG::" namespace
//! matches the full name of the register,
//! the register is not tested.
//!
//!| uvm_resource_db<bool>::set("REG::" + regmodel->blk->r0->get_full_name(),
//!|                            "NO_REG_TESTS", true, this);
//!
//! Registers without an available backdoor or
//! that contain read-only fields only,
//! or fields with unknown access policies
//! cannot be tested.
//!
//! The DUT should be idle and not modify any register during this test.
//----------------------------------------------------------------------

class uvm_reg_single_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:

  // Data member: rg
  // The register to be tested
  uvm_reg* rg;

  UVM_OBJECT_UTILS(uvm_reg_single_access_seq);

  uvm_reg_single_access_seq( const std::string& name = "uvm_reg_single_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), rg(nullptr)
  {}

  virtual void body()
  {
    std::vector<uvm_reg_map*> maps;
    std::vector<uvm_reg_field*> fields;

    if (rg == nullptr)
    {
      UVM_ERROR("uvm_reg_access_seq", "No register specified to run sequence on");
      return;
    }

    // Registers with some attributes are not to be tested
    if (uvm_resource_db<bool>::get_by_name("REG::" + rg->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + rg->get_full_name(),
          "NO_REG_ACCESS_TEST", false) != nullptr)
      return;

    // Can only deal with registers with backdoor access
    if (rg->get_backdoor() == nullptr && !rg->has_hdl_path())
    {
      UVM_ERROR("uvm_reg_access_seq", "Register '" + rg->get_full_name() +
        "' does not have a backdoor mechanism available");
      return;
    }

    // Registers may be accessible from multiple physical interfaces (maps)
    rg->get_maps(maps);

    // Cannot test access if register contains RO or OTHER fields
    rg->get_fields(fields);

    for (unsigned int k = 0; k < maps.size(); k++)
    {
      unsigned int ro = 0;

      for (unsigned int j = 0; j < fields.size(); j++)
      {
        if (fields[j]->get_access(maps[k]) == "RO")
          ro++;

        if (!fields[j]->is_known_access(maps[k]))
        {
          UVM_WARNING("uvm_reg_access_seq", "Register '" + rg->get_full_name() +
            "' has field with unknown access type '" +
            fields[j]->get_access(maps[k]) + "', skipping");
          return;
        }
      }

      if (ro == fields.size())
      {
        UVM_WARNING("uvm_reg_access_seq", "Register '" + rg->get_full_name() +
          "' has only RO fields in map " + maps[k]->get_full_name() + ", skipping");
        return;
      }
    }

    // Access each register:
    // - Write complement of reset value via front door
    // - Read value via backdoor and compare against mirror
    // - Write reset value via backdoor
    // - Read via front door and compare against mirror
    for (unsigned int j = 0; j < maps.size(); j++)
    {
      uvm_status_e status;
      uvm_reg_data_t v;

      UVM_INFO("uvm_reg_access_seq", "Verifying access of register '" +
        rg->get_full_name() + "' in map '" + maps[j]->get_full_name() +
        "' ...", UVM_LOW);

      v = rg->get();

      rg->write(status, ~v, UVM_FRONTDOOR, maps[j], this);

      if (status != UVM_IS_OK)
      {
        UVM_ERROR("uvm_reg_access_seq", "Status was '" +
          std::string(uvm_status_name[status]) + "' when writing '" +
          rg->get_full_name() + "' through map '" +
          maps[j]->get_full_name() + "'");
      }

      // let the DUT settle before the value is read via the backdoor
      sc_core::wait(sc_core::sc_get_time_resolution());

      rg->mirror(status, UVM_CHECK, UVM_BACKDOOR, nullptr, this);

      if (status != UVM_IS_OK)
      {
        UVM_ERROR("uvm_reg_access_seq", "Status was '" +
          std::string(uvm_status_name[status]) +
          "' when reading reset value of register '" +
          rg->get_full_name() + "' through backdoor");
      }

      rg->write(status, v, UVM_BACKDOOR, maps[j], this);

      if (status != UVM_IS_OK)
      {
        UVM_ERROR("uvm_reg_access_seq", "Status was '" +
          std::string(uvm_status_name[status]) + "' when writing '" +
          rg->get_full_name() + "' through backdoor");
      }

      rg->mirror(status, UVM_CHECK, UVM_FRONTDOOR, maps[j], this);

      if (status != UVM_IS_OK)
      {
        UVM_ERROR("uvm_reg_access_seq", "Status was '" +
          std::string(uvm_status_name[status]) +
          "' when reading reset value of register '" +
          rg->get_full_name() + "' through map '" +
          maps[j]->get_full_name() + "'");
      }
    }
  } // body

}; // class uvm_reg_single_access_seq


//----------------------------------------------------------------------
// Class: uvm_reg_access_seq
//
//! Verify the accessibility of all registers in a block
//! by executing the #uvm_reg_single_access_seq sequence on
//! every register within it.
//!
//! If bit-type resource named
//! "NO_REG_TESTS" or "NO_REG_ACCESS_TEST"
//! in the "REG::" namespace
//! matches the full name of the block,
//! the block is not tested.
//----------------------------------------------------------------------

class uvm_reg_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:
  UVM_OBJECT_UTILS(uvm_reg_access_seq);

  uvm_reg_access_seq( const std::string& name = "uvm_reg_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), reg_seq(nullptr)
  {}

  // Member function: body
  //
  // Executes the sequence.
  // Do not call directly. Use seq.start() instead.
  //
  virtual void body()
  {
    if (model == nullptr)
    {
      UVM_ERROR("uvm_reg_access_seq", "No register model specified to run sequence on");
      return;
    }

    uvm_report_info("STARTING_SEQ", "\n\nStarting " + get_name() + " sequence...\n", UVM_LOW);

    reg_seq = uvm_reg_single_access_seq::type_id::create("single_reg_access_seq");

    this->reset_blk(model);
    model->reset();

    do_block(model);

    uvm_reg_single_access_seq::type_id::destroy(reg_seq);
    reg_seq = nullptr;
  }

 protected:

  // Member function: do_block
  //
  // Test all of the registers in a given block
  //
  virtual void do_block( uvm_reg_block* blk )
  {
    std::vector<uvm_reg*> regs;

    if (uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_REG_ACCESS_TEST", false) != nullptr)
      return;

    blk->get_registers(regs, UVM_NO_HIER);

    for (unsigned int i = 0; i < regs.size(); i++)
    {
      // Registers with some attributes are not to be tested
      if (uvm_resource_db<bool>::get_by_name("REG::" + regs[i]->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
          uvm_resource_db<bool>::get_by_name("REG::" + regs[i]->get_full_name(),
          "NO_REG_ACCESS_TEST", false) != nullptr)
        continue;

      // Can only deal with registers with backdoor access
      if (regs[i]->get_backdoor() == nullptr && !regs[i]->has_hdl_path())
      {
        UVM_WARNING("uvm_reg_access_seq", "Register '" + regs[i]->get_full_name() +
          "' does not have a backdoor mechanism available");
        continue;
      }

      reg_seq->rg = regs[i];
      reg_seq->start(nullptr, this);
    }

    std::vector<uvm_reg_block*> blks;
    blk->get_blocks(blks, UVM_NO_HIER);

    for (unsigned int i = 0; i < blks.size(); i++)
      do_block(blks[i]);
  }

 public:

  // Member function: reset_blk
  //
  // Reset the DUT that corresponds to the specified block abstraction class.
  //
  // Currently empty. The DUT should be reset before executing this
  // test sequence or this member function should be implemented
  // in an extension to reset the DUT.
  //
  virtual void reset_blk( uvm_reg_block* blk ) {}

 protected:

  // Variable: reg_seq
  //
  // The sequence used to test one register
  //
  uvm_reg_single_access_seq* reg_seq;

}; // class uvm_reg_access_seq

} // namespace uvm

#endif // UVM_REG_ACCESS_SEQ_H_
