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

#ifndef UVM_REG_MEM_SHARED_ACCESS_SEQ_H_
#define UVM_REG_MEM_SHARED_ACCESS_SEQ_H_

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
#include "uvmsc/reg/uvm_mem.h"
#include "uvmsc/reg/sequences/uvm_reg_randval.h"

namespace uvm
{

//----------------------------------------------------------------------
// Title: Shared Register and Memory Access Test Sequences
//----------------------------------------------------------------------
// This section defines sequences for testing registers and memories that are
// shared between two or more physical interfaces, i.e. are associated with
// more than one #uvm_reg_map instance.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Class: uvm_reg_shared_access_seq
//
//! Verify the accessibility of a shared register
//! by writing through each address map
//! then reading it via every other address maps
//! in which the register is readable and the backdoor,
//! making sure that the resulting value matches the mirrored value.
//!
//! If bit-type resource named
//! "NO_REG_TESTS" or "NO_REG_SHARED_ACCESS_TEST"
//! in the "REG::" namespace
//! matches the full name of the register,
//! the register is not tested.
//!
//!| uvm_resource_db<bool>::set("REG::" + regmodel->blk->r0->get_full_name(),
//!|                            "NO_REG_TESTS", true, this);
//!
//! Registers that contain fields with unknown access policies
//! cannot be tested.
//!
//! The DUT should be idle and not modify any register during this test.
//----------------------------------------------------------------------

class uvm_reg_shared_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:

  // Data member: rg
  // The register to be tested
  uvm_reg* rg;

  // Data member: rand_reg_val
  // Generator of the values written to the register
  uvm_reg_randval rand_reg_val;

  UVM_OBJECT_UTILS(uvm_reg_shared_access_seq);

  uvm_reg_shared_access_seq( const std::string& name = "uvm_reg_shared_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), rg(nullptr)
  {}

  virtual void body()
  {
    uvm_reg_data_t other_mask;
    std::vector<uvm_reg_data_t> wo_mask;
    std::vector<uvm_reg_field*> fields;
    std::vector<uvm_reg_map*> maps;

    if (rg == nullptr)
    {
      UVM_ERROR("uvm_reg_shared_access_seq", "No register specified to run sequence on");
      return;
    }

    // Registers with some attributes are not to be tested
    if (uvm_resource_db<bool>::get_by_name("REG::" + rg->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + rg->get_full_name(),
          "NO_REG_SHARED_ACCESS_TEST", false) != nullptr)
      return;

    // Only look at shared registers
    if (rg->get_n_maps() < 2)
      return;

    rg->get_maps(maps);

    // Let's see what kind of bits we have...
    rg->get_fields(fields);

    // Identify unpredictable bits and the ones we shouldn't change
    other_mask = 0;
    for (unsigned int k = 0; k < fields.size(); k++)
    {
      int lsb = fields[k]->get_lsb_pos();
      int w = fields[k]->get_n_bits();

      if (!fields[k]->is_known_access(maps[0]))
        for (int r = 0; r < w; r++)
          other_mask[lsb++] = 1;
    }

    // WO bits will always read back as 0's but the mirror
    // with return what is supposed to have been written
    // so we cannot use the mirror-check function
    for (unsigned int j = 0; j < maps.size(); j++)
    {
      uvm_reg_data_t wo = 0;

      for (unsigned int k = 0; k < fields.size(); k++)
      {
        int lsb = fields[k]->get_lsb_pos();
        int w = fields[k]->get_n_bits();

        if (fields[k]->get_access(maps[j]) == "WO")
          for (int r = 0; r < w; r++)
            wo[lsb++] = 1;
      }
      wo_mask.push_back(wo);
    }

    // Try to write through all address maps
    for (unsigned int j = 0; j < maps.size(); j++)
    {
      uvm_status_e status;
      uvm_reg_data_t prev, v;

      // The mirror should contain the initial value
      prev = rg->get();

      // Write a random value, except in those "don't touch" fields
      rand_reg_val.randomize();
      v = (rand_reg_val.randval & ~other_mask) | (prev & other_mask);

      UVM_INFO("uvm_reg_shared_access_seq", "Writing register " +
        rg->get_full_name() + " via map \"" + maps[j]->get_full_name() +
        "\"...", UVM_LOW);

      {
        std::ostringstream str;
        str << "Writing 0x" << std::hex << v.to_uint64() << " over 0x" << prev.to_uint64();
        UVM_INFO("uvm_reg_shared_access_seq", str.str(), UVM_DEBUG);
      }

      rg->write(status, v, UVM_FRONTDOOR, maps[j], this);

      if (status != UVM_IS_OK)
      {
        UVM_ERROR("uvm_reg_shared_access_seq", "Status was " +
          std::string(uvm_status_name[status]) + " when writing register \"" +
          rg->get_full_name() + "\" through map \"" +
          maps[j]->get_full_name() + "\".");
      }

      for (unsigned int k = 0; k < maps.size(); k++)
      {
        uvm_reg_data_t actual, exp;

        UVM_INFO("uvm_reg_shared_access_seq", "Reading register " +
          rg->get_full_name() + " via map \"" + maps[k]->get_full_name() +
          "\"...", UVM_LOW);

        // Was it what we expected?
        exp = rg->get() & ~wo_mask[k];

        rg->read(status, actual, UVM_FRONTDOOR, maps[k], this);

        if (status != UVM_IS_OK)
        {
          UVM_ERROR("uvm_reg_shared_access_seq", "Status was " +
            std::string(uvm_status_name[status]) + " when reading register \"" +
            rg->get_full_name() + "\" through map \"" +
            maps[k]->get_full_name() + "\".");
        }

        {
          std::ostringstream str;
          str << "Read 0x" << std::hex << actual.to_uint64() << ", expecting 0x" << exp.to_uint64();
          UVM_INFO("uvm_reg_shared_access_seq", str.str(), UVM_DEBUG);
        }

        if (actual != exp)
        {
          std::ostringstream str;
          str << "Register \"" << rg->get_full_name() << "\" through map \""
              << maps[k]->get_full_name() << "\" is 0x" << std::hex << actual.to_uint64()
              << " instead of 0x" << exp.to_uint64() << " after writing 0x" << v.to_uint64()
              << " via map \"" << maps[j]->get_full_name() << "\" over 0x"
              << prev.to_uint64() << ".";
          UVM_ERROR("uvm_reg_shared_access_seq", str.str());
        }
      }
    }
  } // body

}; // class uvm_reg_shared_access_seq


//----------------------------------------------------------------------
// Class: uvm_mem_shared_access_seq
//
//! Verify the accessibility of a shared memory
//! by writing through each address map
//! then reading it via every other address maps
//! in which the memory is readable and the backdoor,
//! making sure that the resulting value matches the written value.
//!
//! If bit-type resource named
//! "NO_REG_TESTS", "NO_MEM_TESTS",
//! "NO_REG_SHARED_ACCESS_TEST" or "NO_MEM_SHARED_ACCESS_TEST"
//! in the "REG::" namespace
//! matches the full name of the memory,
//! the memory is not tested.
//!
//!| uvm_resource_db<bool>::set("REG::" + regmodel->blk->mem0->get_full_name(),
//!|                            "NO_MEM_TESTS", true, this);
//!
//! The DUT should be idle and not modify the memory during this test.
//----------------------------------------------------------------------

class uvm_mem_shared_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:

  // Data member: mem
  // The memory to be tested
  uvm_mem* mem;

  // Data member: rand_reg_val
  // Generator of the values written to the memory
  uvm_reg_randval rand_reg_val;

  UVM_OBJECT_UTILS(uvm_mem_shared_access_seq);

  uvm_mem_shared_access_seq( const std::string& name = "uvm_mem_shared_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), mem(nullptr)
  {}

  virtual void body()
  {
    int read_from;
    std::vector<uvm_reg_map*> maps;
    uvm_reg_data_t mask;

    if (mem == nullptr)
    {
      UVM_ERROR("uvm_mem_shared_access_seq", "No memory specified to run sequence on");
      return;
    }

    // Memories with some attributes are not to be tested
    if (uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_MEM_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_REG_SHARED_ACCESS_TEST", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + mem->get_full_name(),
          "NO_MEM_SHARED_ACCESS_TEST", false) != nullptr)
      return;

    // Only look at shared memories
    if (mem->get_n_maps() < 2)
      return;

    mem->get_maps(maps);

    mask = uvm_mask_size(mem->get_n_bits());

    // We need at least a backdoor or a map that can read
    // the shared memory
    read_from = -1;
    if (mem->get_backdoor() == nullptr)
    {
      for (unsigned int j = 0; j < maps.size(); j++)
      {
        std::string right = mem->get_access(maps[j]);

        if (right == "RW" || right == "RO")
        {
          read_from = j;
          break;
        }
      }

      if (read_from < 0)
      {
        UVM_WARNING("uvm_mem_shared_access_seq", "Memory \"" + mem->get_full_name() +
          "\" cannot be read from any maps or backdoor. Shared access not verified.");
        return;
      }
    }

    // Try to write through all address maps
    for (unsigned int j = 0; j < maps.size(); j++)
    {
      UVM_INFO("uvm_mem_shared_access_seq", "Writing shared memory \"" +
        mem->get_full_name() + "\" via map \"" + maps[j]->get_full_name() +
        "\".", UVM_LOW);

      // All addresses
      for (unsigned long offset = 0; offset < mem->get_size(); offset++)
      {
        uvm_status_e status;
        uvm_reg_data_t prev, v;

        // Read the initial value
        if (mem->get_backdoor() != nullptr)
        {
          mem->peek(status, offset, prev);

          if (status != UVM_IS_OK)
          {
            std::ostringstream str;
            str << "Status was " << uvm_status_name[status]
                << " when reading initial value of \"" << mem->get_full_name()
                << "\"[" << offset << "] through backdoor.";
            UVM_ERROR("uvm_mem_shared_access_seq", str.str());
          }
        }
        else
        {
          mem->read(status, offset, prev, UVM_FRONTDOOR, maps[read_from], this);

          if (status != UVM_IS_OK)
          {
            std::ostringstream str;
            str << "Status was " << uvm_status_name[status]
                << " when reading initial value of \"" << mem->get_full_name()
                << "\"[" << offset << "] through map \""
                << maps[read_from]->get_full_name() << "\".";
            UVM_ERROR("uvm_mem_shared_access_seq", str.str());
          }
        }

        // Write a random value,
        rand_reg_val.randomize();
        v = rand_reg_val.randval & mask;

        mem->write(status, offset, v, UVM_FRONTDOOR, maps[j], this);

        if (status != UVM_IS_OK)
        {
          std::ostringstream str;
          str << "Status was " << uvm_status_name[status]
              << " when writing \"" << mem->get_full_name()
              << "\"[" << offset << "] through map \""
              << maps[j]->get_full_name() << "\".";
          UVM_ERROR("uvm_mem_shared_access_seq", str.str());
        }

        // Read back from all other address maps
        for (unsigned int k = 0; k < maps.size(); k++)
        {
          uvm_reg_data_t actual, exp;

          mem->read(status, offset, actual, UVM_FRONTDOOR, maps[k], this);

          if (status != UVM_IS_OK)
          {
            std::ostringstream str;
            str << "Status was " << uvm_status_name[status]
                << " when reading " << mem->get_full_name()
                << "[" << offset << "] through map \""
                << maps[k]->get_full_name() << "\".";
            UVM_ERROR("uvm_mem_shared_access_seq", str.str());
          }

          // Was it what we expected?
          exp = v;
          if (mem->get_access(maps[j]) == "RO")
            exp = prev;
          if (mem->get_access(maps[k]) == "WO")
            exp = 0;

          // Trim to number of bits
          exp &= mask;

          if (actual != exp)
          {
            std::ostringstream str;
            str << mem->get_full_name() << "[" << offset << "] through map \""
                << maps[k]->get_full_name() << "\" is 0x" << std::hex << actual.to_uint64()
                << " instead of 0x" << exp.to_uint64() << " after writing 0x" << v.to_uint64()
                << " via map \"" << maps[j]->get_full_name() << "\" over 0x"
                << prev.to_uint64() << ".";
            UVM_ERROR("uvm_mem_shared_access_seq", str.str());
          }
        }
      }
    }
  } // body

}; // class uvm_mem_shared_access_seq


//----------------------------------------------------------------------
// Class: uvm_reg_mem_shared_access_seq
//
//! Verify the accessibility of all shared registers
//! and memories in a block
//! by executing the #uvm_reg_shared_access_seq
//! and #uvm_mem_shared_access_seq
//! sequence respectively on every register and memory within it.
//!
//! If bit-type resource named
//! "NO_REG_TESTS", "NO_MEM_TESTS",
//! "NO_REG_SHARED_ACCESS_TEST" or "NO_MEM_SHARED_ACCESS_TEST"
//! in the "REG::" namespace
//! matches the full name of the block,
//! the block is not tested.
//----------------------------------------------------------------------

class uvm_reg_mem_shared_access_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:
  UVM_OBJECT_UTILS(uvm_reg_mem_shared_access_seq);

  uvm_reg_mem_shared_access_seq( const std::string& name = "uvm_reg_mem_shared_access_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), reg_seq(nullptr), mem_seq(nullptr)
  {}

  // Member function: body
  //
  // Executes the Shared Register and Memory sequence.
  // Do not call directly. Use seq.start() instead.
  //
  virtual void body()
  {
    if (model == nullptr)
    {
      UVM_ERROR("uvm_reg_mem_shared_access_seq", "No register model specified to run sequence on");
      return;
    }

    uvm_report_info("STARTING_SEQ", "\n\nStarting " + get_name() + " sequence...\n", UVM_LOW);

    reg_seq = uvm_reg_shared_access_seq::type_id::create("reg_shared_access_seq");
    mem_seq = uvm_mem_shared_access_seq::type_id::create("mem_shared_access_seq");

    this->reset_blk(model);
    model->reset();

    do_block(model);

    uvm_reg_shared_access_seq::type_id::destroy(reg_seq);
    uvm_mem_shared_access_seq::type_id::destroy(mem_seq);
    reg_seq = nullptr;
    mem_seq = nullptr;
  }

 protected:

  // Member function: do_block
  //
  // Test all of the registers and memories in a block
  //
  virtual void do_block( uvm_reg_block* blk )
  {
    std::vector<uvm_reg*> regs;
    std::vector<uvm_mem*> mems;

    if (uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_MEM_TESTS", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_REG_SHARED_ACCESS_TEST", false) != nullptr ||
        uvm_resource_db<bool>::get_by_name("REG::" + blk->get_full_name(),
          "NO_MEM_SHARED_ACCESS_TEST", false) != nullptr)
      return;

    this->reset_blk(model);
    model->reset();

    // Iterate over all registers, checking accesses
    blk->get_registers(regs, UVM_NO_HIER);

    for (unsigned int i = 0; i < regs.size(); i++)
    {
      // Registers with some attributes are not to be tested
      if (uvm_resource_db<bool>::get_by_name("REG::" + regs[i]->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
          uvm_resource_db<bool>::get_by_name("REG::" + regs[i]->get_full_name(),
          "NO_REG_SHARED_ACCESS_TEST", false) != nullptr)
        continue;

      reg_seq->rg = regs[i];
      reg_seq->start(this->get_sequencer(), this);
    }

    // Iterate over all memories, checking accesses
    blk->get_memories(mems, UVM_NO_HIER);

    for (unsigned int i = 0; i < mems.size(); i++)
    {
      // Memories with some attributes are not to be tested
      if (uvm_resource_db<bool>::get_by_name("REG::" + mems[i]->get_full_name(),
          "NO_REG_TESTS", false) != nullptr ||
          uvm_resource_db<bool>::get_by_name("REG::" + mems[i]->get_full_name(),
          "NO_MEM_TESTS", false) != nullptr ||
          uvm_resource_db<bool>::get_by_name("REG::" + mems[i]->get_full_name(),
          "NO_REG_SHARED_ACCESS_TEST", false) != nullptr ||
          uvm_resource_db<bool>::get_by_name("REG::" + mems[i]->get_full_name(),
          "NO_MEM_SHARED_ACCESS_TEST", false) != nullptr)
        continue;

      mem_seq->mem = mems[i];
      mem_seq->start(this->get_sequencer(), this);
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
  uvm_reg_shared_access_seq* reg_seq;

  // Variable: mem_seq
  //
  // The sequence used to test one memory
  //
  uvm_mem_shared_access_seq* mem_seq;

}; // class uvm_reg_mem_shared_access_seq

} // namespace uvm

#endif // UVM_REG_MEM_SHARED_ACCESS_SEQ_H_
