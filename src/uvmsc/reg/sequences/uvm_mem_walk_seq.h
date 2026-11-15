// -------------------------------------------------------------
//    Copyright 2010 AMD
//    Copyright 2010-2018 Cadence Design Systems, Inc.
//    Copyright 2010-2011 Mentor Graphics Corporation
//    Copyright 2014-2024 NVIDIA Corporation
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

#ifndef UVM_MEM_WALK_SEQ_H_
#define UVM_MEM_WALK_SEQ_H_

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

namespace uvm
{

//----------------------------------------------------------------------
// Title: Memory Walking-Ones Test Sequences
//----------------------------------------------------------------------
// This section defines sequences for applying a "walking-ones"
// algorithm on one or more memories.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Class: uvm_mem_single_walk_seq
//
//! Runs the walking-ones algorithm on the memory given by the mem
//! property, which must be assigned prior to starting this sequence.
//!
//! If bit-type resource named
//! "NO_REG_TESTS", "NO_MEM_TESTS", or "NO_MEM_WALK_TEST"
//! in the "REG::" namespace
//! matches the full name of the memory,
//! the memory is not tested.
//!
//!| uvm_resource_db<bool>::set("REG::" + regmodel->blk->mem0->get_full_name(),
//!|                            "NO_MEM_TESTS", true, this);
//!
//! The walking ones algorithm is performed for each map in which the
//! memory is defined.
//!
//!| for (k = 0 thru memsize-1)
//!|   write addr=k data=~k
//!|   if (k > 0) {
//!|     read addr=k-1, expect data=~(k-1)
//!|     write addr=k-1 data=k-1
//!|   if (k == last addr)
//!|     read addr=k, expect data=~k
//----------------------------------------------------------------------

class uvm_mem_single_walk_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:

  // Data member: mem
  // The memory to test; must be assigned prior to starting sequence.
  uvm_mem* mem;

  UVM_OBJECT_UTILS(uvm_mem_single_walk_seq);

  uvm_mem_single_walk_seq( const std::string& name = "uvm_mem_walk_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), mem(nullptr)
  {}

  // Member function: body
  //
  // Performs the walking-ones algorithm on each map of the memory
  // specified in mem.
  //
  virtual void body()
  {
    std::vector<uvm_reg_map*> maps;
    uvm_reg_data_t mask;

    if (mem == nullptr)
    {
      UVM_ERROR("uvm_mem_walk_seq", "No memory specified to run sequence on");
      return;
    }

    // Memories with some attributes are not to be tested
    if (uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_MEM_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_MEM_WALK_TEST", false) != nullptr)
      return;

    mask = uvm_mask_size(mem->get_n_bits());

    // Memories may be accessible from multiple physical interfaces (maps)
    mem->get_maps(maps);

    // Walk the memory via each map
    for (unsigned int j = 0; j < maps.size(); j++)
    {
      uvm_status_e status;
      uvm_reg_data_t val, exp;

      // Only deal with RW memories
      if (mem->get_access(maps[j]) != "RW")
        continue;

      UVM_INFO("uvm_mem_walk_seq", "Walking memory " + mem->get_full_name() +
        " in map \"" + maps[j]->get_full_name() + "\"...", UVM_LOW);

      // The walking process is, for address k:
      // - Write ~k
      // - Read k-1 and expect ~(k-1) if k > 0
      // - Write k-1 at k-1
      // - Read k and expect ~k if k == last address
      for (unsigned long k = 0; k < mem->get_size(); k++)
      {
        mem->write(status, k, ~uvm_reg_data_t(k) & mask, UVM_FRONTDOOR, maps[j], this);

        if (status != UVM_IS_OK)
        {
          UVM_ERROR("uvm_mem_walk_seq", m_status_msg(status, "writing", k, maps[j]));
        }

        if (k > 0)
        {
          mem->read(status, k-1, val, UVM_FRONTDOOR, maps[j], this);

          if (status != UVM_IS_OK)
          {
            UVM_ERROR("uvm_mem_walk_seq", m_status_msg(status, "reading", k-1, maps[j]));
          }
          else
          {
            exp = ~uvm_reg_data_t(k-1) & mask;
            if (val != exp)
            {
              UVM_ERROR("uvm_mem_walk_seq", m_value_msg(k-1, val, exp));
            }
          }

          mem->write(status, k-1, uvm_reg_data_t(k-1) & mask, UVM_FRONTDOOR, maps[j], this);

          if (status != UVM_IS_OK)
          {
            UVM_ERROR("uvm_mem_walk_seq", m_status_msg(status, "writing", k-1, maps[j]));
          }
        }

        if (k == mem->get_size() - 1)
        {
          mem->read(status, k, val, UVM_FRONTDOOR, maps[j], this);

          if (status != UVM_IS_OK)
          {
            UVM_ERROR("uvm_mem_walk_seq", m_status_msg(status, "reading", k, maps[j]));
          }
          else
          {
            exp = ~uvm_reg_data_t(k) & mask;
            if (val != exp)
            {
              UVM_ERROR("uvm_mem_walk_seq", m_value_msg(k, val, exp));
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
                            uvm_reg_map* map ) const
  {
    std::ostringstream str;
    str << "Status was " << uvm_status_name[status] << " when " << action
        << " \"" << mem->get_full_name() << "[" << k << "]\" through map \""
        << map->get_full_name() << "\".";
    return str.str();
  }

  std::string m_value_msg( unsigned long k,
                           const uvm_reg_data_t& val,
                           const uvm_reg_data_t& exp ) const
  {
    std::ostringstream str;
    str << "\"" << mem->get_full_name() << "[" << k << "]\" read back as 0x"
        << std::hex << val.to_uint64() << " instead of 0x" << exp.to_uint64() << ".";
    return str.str();
  }

}; // class uvm_mem_single_walk_seq


//----------------------------------------------------------------------
// Class: uvm_mem_walk_seq
//
//! Verifies all memories in a block
//! by executing the #uvm_mem_single_walk_seq sequence on
//! every memory within it.
//!
//! If bit-type resource named
//! "NO_REG_TESTS", "NO_MEM_TESTS", or "NO_MEM_WALK_TEST"
//! in the "REG::" namespace
//! matches the full name of the block,
//! the block is not tested.
//----------------------------------------------------------------------

class uvm_mem_walk_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:
  UVM_OBJECT_UTILS(uvm_mem_walk_seq);

  uvm_mem_walk_seq( const std::string& name = "uvm_mem_walk_seq" )
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
      UVM_ERROR("uvm_mem_walk_seq", "No register model specified to run sequence on");
      return;
    }

    uvm_report_info("STARTING_SEQ", "\n\nStarting " + get_name() + " sequence...\n", UVM_LOW);

    mem_seq = uvm_mem_single_walk_seq::type_id::create("single_mem_walk_seq");

    this->reset_blk(model);
    model->reset();

    do_block(model);

    uvm_mem_single_walk_seq::type_id::destroy(mem_seq);
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
          "NO_MEM_WALK_TEST", false) != nullptr)
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
          "NO_MEM_WALK_TEST", false) != nullptr)
        continue;

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
  uvm_mem_single_walk_seq* mem_seq;

}; // class uvm_mem_walk_seq

} // namespace uvm

#endif // UVM_MEM_WALK_SEQ_H_
