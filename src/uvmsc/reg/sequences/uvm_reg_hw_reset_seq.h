// -------------------------------------------------------------
//    Copyright 2010 AMD
//    Copyright 2010-2018 Cadence Design Systems, Inc.
//    Copyright 2021 Marvell International Ltd.
//    Copyright 2010-2011 Mentor Graphics Corporation
//    Copyright 2014-2024 NVIDIA Corporation
//    Copyright 2018 Qualcomm, Inc.
//    Copyright 2012-2020 Semifore
//    Copyright 2004-2013 Synopsys, Inc.
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

#ifndef UVM_REG_HW_RESET_SEQ_H_
#define UVM_REG_HW_RESET_SEQ_H_

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
// Class: uvm_reg_hw_reset_seq
//
//! Test the hard reset values of registers
//!
//! The test sequence performs the following steps
//!
//! 1. resets the DUT and the
//! block abstraction class associated with this sequence.
//!
//! 2. reads all of the registers in the block,
//! via all of the available address maps,
//! comparing the value read with the expected reset value.
//!
//! If bit-type resource named
//! "NO_REG_TESTS" or "NO_REG_HW_RESET_TEST"
//! in the "REG::" namespace
//! matches the full name of the block or register,
//! the block or register is not tested.
//!
//!| uvm_resource_db<bool>::set("REG::" + regmodel->blk->get_full_name() + ".*",
//!|                            "NO_REG_TESTS", true, this);
//!
//! This is usually the first test executed on any DUT.
//----------------------------------------------------------------------

class uvm_reg_hw_reset_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:
  UVM_OBJECT_UTILS(uvm_reg_hw_reset_seq);

  uvm_reg_hw_reset_seq( const std::string& name = "uvm_reg_hw_reset_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name)
  {}

  // Variable: model
  //
  // The block to be tested. Declared in the base class.
  //
  //| uvm_reg_block* model;

  // Member function: body
  //
  // Executes the Hardware Reset sequence.
  // Do not call directly. Use seq.start() instead.
  //
  virtual void body()
  {
    if (model == nullptr)
    {
      UVM_ERROR("uvm_reg_hw_reset_seq", "No block or system specified to run sequence on");
      return;
    }

    uvm_report_info("STARTING_SEQ", "\n\nStarting " + get_name() + " sequence...\n", UVM_LOW);

    this->reset_blk(model);
    model->reset();

    do_block(model);
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
          "NO_REG_HW_RESET_TEST", false) != nullptr)
      return;

    blk->get_registers(regs, UVM_NO_HIER);

    for (unsigned int i = 0; i < regs.size(); i++)
    {
      if (uvm_resource_db<bool>::get_by_name("REG::" + regs[i]->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
          !regs[i]->has_reset() ||
          uvm_resource_db<bool>::get_by_name("REG::" + regs[i]->get_full_name(),
          "NO_REG_HW_RESET_TEST", false) != nullptr)
        continue;

      std::vector<uvm_reg_map*> maps;
      std::vector<uvm_reg_field*> fields;
      std::vector<uvm_reg_field*> restore_fields;
      std::vector<uvm_check_e> restore_checks;
      uvm_status_e status;

      regs[i]->get_maps(maps);
      regs[i]->get_fields(fields);

      // Fields which cannot be checked are not compared
      for (unsigned int k = 0; k < fields.size(); k++)
      {
        if (!fields[k]->has_reset() ||
            fields[k]->get_compare() == UVM_NO_CHECK ||
            uvm_resource_db<bool>::get_by_name("REG::" + fields[k]->get_full_name(),
            "NO_REG_HW_RESET_TEST", false) != nullptr)
        {
          restore_fields.push_back(fields[k]);
          restore_checks.push_back(fields[k]->get_compare());
          fields[k]->set_compare(UVM_NO_CHECK);
        }
      }

      // if there are some fields to check
      if (fields.size() != restore_fields.size())
      {
        for (unsigned int j = 0; j < maps.size(); j++)
        {
          UVM_INFO(get_type_name(), "Verifying reset value of register " +
            regs[i]->get_full_name() + " in map \"" +
            maps[j]->get_full_name() + "\"...", UVM_LOW);

          regs[i]->mirror(status, UVM_CHECK, UVM_FRONTDOOR, maps[j], this);

          if (status != UVM_IS_OK)
          {
            UVM_ERROR(get_type_name(), "Status was " +
              std::string(uvm_status_name[status]) +
              " when reading reset value of register \"" +
              regs[i]->get_full_name() + "\" through map \"" +
              maps[j]->get_full_name() + "\".");
          }
        }
      }

      // restore compare setting
      for (unsigned int k = 0; k < restore_fields.size(); k++)
        restore_fields[k]->set_compare(restore_checks[k]);
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

}; // class uvm_reg_hw_reset_seq

} // namespace uvm

#endif // UVM_REG_HW_RESET_SEQ_H_
