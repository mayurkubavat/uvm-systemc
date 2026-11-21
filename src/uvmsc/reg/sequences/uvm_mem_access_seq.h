// -------------------------------------------------------------
//    Copyright 2010 AMD
//    Copyright 2010-2018 Cadence Design Systems, Inc.
//    Copyright 2010-2020 Mentor Graphics Corporation
//    Copyright 2015-2024 NVIDIA Corporation
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

#ifndef UVM_MEM_ACCESS_SEQ_H_
#define UVM_MEM_ACCESS_SEQ_H_

#include <systemc>
#include <string>
#include <sstream>
#include <vector>

#include "uvmsc/conf/uvm_resource_db.h"
#include "uvmsc/reg/uvm_reg_sequence.h"
#include "uvmsc/reg/uvm_reg_block.h"
#include "uvmsc/reg/uvm_reg_map.h"
#include "uvmsc/reg/uvm_reg_model.h"
#include "uvmsc/reg/uvm_mem.h"
#include "uvmsc/reg/sequences/uvm_reg_randval.h"
#include "uvmsc/reg/sequences/uvm_reg_access_seq.h"

namespace uvm
{

//----------------------------------------------------------------------
// Title: Memory Access Test Sequence
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Class: uvm_mem_single_access_seq
//
//! Verify the accessibility of a memory
//! by writing through its default address map
//! then reading it via the backdoor, then reversing the process,
//! making sure that the resulting value matches the written value.
//!
//! If bit-type resource named
//! "NO_REG_TESTS", "NO_MEM_TESTS", or "NO_MEM_ACCESS_TEST"
//! in the "REG::" namespace
//! matches the full name of the memory,
//! the memory is not tested.
//!
//!| uvm_resource_db<bool>::set("REG::" + regmodel->blk->mem0->get_full_name(),
//!|                            "NO_MEM_TESTS", true, this);
//!
//! Memories without an available backdoor
//! cannot be tested.
//!
//! The DUT should be idle and not modify the memory during this test.
//----------------------------------------------------------------------

class uvm_mem_single_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:

  // Data member: mem
  // The memory to be tested
  uvm_mem* mem;

  // Data member: rand_reg_val
  // Generator of the values written to the memory
  uvm_reg_randval rand_reg_val;

  UVM_OBJECT_UTILS(uvm_mem_single_access_seq);

  uvm_mem_single_access_seq( const std::string& name = "uvm_mem_single_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), mem(nullptr)
  {}

  virtual void body()
  {
    // The values depend on the global seed and on this sequence only,
    // unless the user has set a seed.
    if (!rand_reg_val.has_seed())
      rand_reg_val.set_seed(uvm_create_random_seed(get_type_name(), get_full_name()));

    std::string mode;
    std::vector<uvm_reg_map*> maps;
    uvm_reg_data_t mask;

    if (mem == nullptr)
    {
      UVM_ERROR("uvm_mem_access_seq", "No memory specified to run sequence on");
      return;
    }

    // Memories with some attributes are not to be tested
    if (uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_MEM_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_MEM_ACCESS_TEST", false) != nullptr)
      return;

    // Can only deal with memories with backdoor access
    if (mem->get_backdoor() == nullptr && !mem->has_hdl_path())
    {
      UVM_ERROR("uvm_mem_access_seq", "Memory '" + mem->get_full_name() +
        "' does not have a backdoor mechanism available");
      return;
    }

    mask = uvm_mask_size(mem->get_n_bits());

    // Memories may be accessible from multiple physical interfaces (maps)
    mem->get_maps(maps);

    // Walk the memory via each map
    for (unsigned int j = 0; j < maps.size(); j++)
    {
      uvm_status_e status;
      uvm_reg_data_t val, exp;

      UVM_INFO("uvm_mem_access_seq", "Verifying access of memory '" +
        mem->get_full_name() + "' in map '" + maps[j]->get_full_name() +
        "' ...", UVM_LOW);

      mode = mem->get_access(maps[j]);

      // The access process is, for address k:
      // - Write random value via front door
      // - Read via backdoor and expect same random value if RW
      // - Write complement of random value via back door
      // - Read via front door and expect inverted random value
      for (unsigned long k = 0; k < mem->get_size(); k++)
      {
        rand_reg_val.randomize();
        val = rand_reg_val.randval & mask;

        if (mode == "RO")
        {
          mem->peek(status, k, exp);

          if (status != UVM_IS_OK)
          {
            UVM_ERROR("uvm_mem_access_seq", m_status_msg(status, "reading", k, "backdoor"));
          }
        }
        else
          exp = val;

        mem->write(status, k, val, UVM_FRONTDOOR, maps[j], this);

        if (status != UVM_IS_OK)
        {
          UVM_ERROR("uvm_mem_access_seq", m_status_msg(status, "writing", k,
            "map \"" + maps[j]->get_full_name() + "\""));
        }

        // let the DUT settle before the value is read via the backdoor
        sc_core::wait(sc_core::sc_get_time_resolution());

        val = 0;
        mem->peek(status, k, val);

        if (status != UVM_IS_OK)
        {
          UVM_ERROR("uvm_mem_access_seq", m_status_msg(status, "reading", k, "backdoor"));
        }
        else if (val != exp)
        {
          UVM_ERROR("uvm_mem_access_seq", m_value_msg("Backdoor", k, val, exp));
        }

        exp = ~exp & mask;
        mem->poke(status, k, exp);

        if (status != UVM_IS_OK)
        {
          UVM_ERROR("uvm_mem_access_seq", m_status_msg(status, "writing", k, "backdoor"));
        }

        mem->read(status, k, val, UVM_FRONTDOOR, maps[j], this);

        if (status != UVM_IS_OK)
        {
          UVM_ERROR("uvm_mem_access_seq", m_status_msg(status, "reading", k,
            "map \"" + maps[j]->get_full_name() + "\""));
        }
        else
        {
          if (mode == "WO")
          {
            if (val != 0)
            {
              UVM_ERROR("uvm_mem_access_seq", m_value_msg("Front door", k, val, 0));
            }
          }
          else
          {
            if (val != exp)
            {
              UVM_ERROR("uvm_mem_access_seq", m_value_msg("Front door", k, val, exp));
            }
          }
        }
      }
    }
  } // body

 private:

  std::string m_status_msg( uvm_status_e status,
                            const std::string& action,
                            unsigned long k,
                            const std::string& path ) const
  {
    std::ostringstream str;
    str << "Status was " << uvm_status_name[status] << " when " << action
        << " \"" << mem->get_full_name() << "[" << k << "]\" through "
        << path << ".";
    return str.str();
  }

  std::string m_value_msg( const std::string& path,
                           unsigned long k,
                           const uvm_reg_data_t& val,
                           const uvm_reg_data_t& exp ) const
  {
    std::ostringstream str;
    str << path << " \"" << mem->get_full_name() << "[" << k
        << "]\" read back as 0x" << std::hex << val.to_uint64()
        << " instead of 0x" << exp.to_uint64() << ".";
    return str.str();
  }

}; // class uvm_mem_single_access_seq


//----------------------------------------------------------------------
// Class: uvm_mem_access_seq
//
//! Verify the accessibility of all memories in a block
//! by executing the #uvm_mem_single_access_seq sequence on
//! every memory within it.
//!
//! If bit-type resource named
//! "NO_REG_TESTS", "NO_MEM_TESTS", or "NO_MEM_ACCESS_TEST"
//! in the "REG::" namespace
//! matches the full name of the block,
//! the block is not tested.
//----------------------------------------------------------------------

class uvm_mem_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:
  UVM_OBJECT_UTILS(uvm_mem_access_seq);

  uvm_mem_access_seq( const std::string& name = "uvm_mem_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), mem_seq(nullptr)
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
      UVM_ERROR("uvm_mem_access_seq", "No register model specified to run sequence on");
      return;
    }

    uvm_report_info("STARTING_SEQ", "\n\nStarting " + get_name() + " sequence...\n", UVM_LOW);

    mem_seq = uvm_mem_single_access_seq::type_id::create("single_mem_access_seq");

    this->reset_blk(model);
    model->reset();

    do_block(model);

    uvm_mem_single_access_seq::type_id::destroy(mem_seq);
    mem_seq = nullptr;
  }

 protected:

  // Member function: do_block
  //
  // Test all of the memories in a given block
  //
  virtual void do_block( uvm_reg_block* blk )
  {
    std::vector<uvm_mem*> mems;

    if (uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_MEM_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_MEM_ACCESS_TEST", false) != nullptr)
      return;

    blk->get_memories(mems, UVM_NO_HIER);

    for (unsigned int i = 0; i < mems.size(); i++)
    {
      // Memories with some attributes are not to be tested
      if (uvm_resource_db<bool>::get_by_name("REG::" + mems[i]->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
          uvm_resource_db<bool>::get_by_name("REG::" + mems[i]->get_full_name(),
          "NO_MEM_TESTS", false) != nullptr ||
          uvm_resource_db<bool>::get_by_name("REG::" + mems[i]->get_full_name(),
          "NO_MEM_ACCESS_TEST", false) != nullptr)
        continue;

      // Can only deal with memories with backdoor access
      if (mems[i]->get_backdoor() == nullptr && !mems[i]->has_hdl_path())
      {
        UVM_WARNING("uvm_mem_access_seq", "Memory \"" + mems[i]->get_full_name() +
          "\" does not have a backdoor mechanism available");
        continue;
      }

      mem_seq->mem = mems[i];
      mem_seq->start(nullptr, this);
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

  // Variable: mem_seq
  //
  // The sequence used to test one memory
  //
  uvm_mem_single_access_seq* mem_seq;

}; // class uvm_mem_access_seq


//----------------------------------------------------------------------
// Class: uvm_reg_mem_access_seq
//
//! Verify the accessibility of all registers and memories in a block
//! by executing the #uvm_reg_access_seq and
//! #uvm_mem_access_seq sequence respectively on every register
//! and memory within it.
//!
//! Blocks and registers with the NO_REG_TESTS or
//! the NO_REG_ACCESS_TEST attribute are not verified.
//----------------------------------------------------------------------

class uvm_reg_mem_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:
  UVM_OBJECT_UTILS(uvm_reg_mem_access_seq);

  uvm_reg_mem_access_seq( const std::string& name = "uvm_reg_mem_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name)
  {}

  virtual void body()
  {
    if (model == nullptr)
    {
      UVM_ERROR("uvm_reg_mem_access_seq", "Register model handle is null");
      return;
    }

    uvm_report_info("STARTING_SEQ", "\n\nStarting " + get_name() + " sequence...\n", UVM_LOW);

    if (uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_TESTS", false) != nullptr)
      return;

    if (uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_ACCESS_TEST", false) == nullptr)
    {
      uvm_reg_access_seq* sub_seq = uvm_reg_access_seq::type_id::create("reg_access_seq");
      this->reset_blk(model);
      model->reset();
      sub_seq->model = model;
      sub_seq->start(nullptr, this);
      uvm_reg_access_seq::type_id::destroy(sub_seq);
    }

    if (uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_MEM_ACCESS_TEST", false) == nullptr)
    {
      uvm_mem_access_seq* sub_seq = uvm_mem_access_seq::type_id::create("mem_access_seq");
      this->reset_blk(model);
      model->reset();
      sub_seq->model = model;
      sub_seq->start(nullptr, this);
      uvm_mem_access_seq::type_id::destroy(sub_seq);
    }
  }

  // Member function: reset_blk
  //
  // Reset the DUT that corresponds to the specified block abstraction class.
  //
  // Currently empty. The DUT should be reset before executing this
  // test sequence or this member function should be implemented
  // in an extension to reset the DUT.
  //
  virtual void reset_blk( uvm_reg_block* blk ) {}

}; // class uvm_reg_mem_access_seq

} // namespace uvm

#endif // UVM_MEM_ACCESS_SEQ_H_
