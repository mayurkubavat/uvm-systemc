// -------------------------------------------------------------
//    Copyright 2010 AMD
//    Copyright 2010-2018 Cadence Design Systems, Inc.
//    Copyright 2010-2011 Mentor Graphics Corporation
//    Copyright 2015-2024 NVIDIA Corporation
//    Copyright 2010 Synopsys, Inc.
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

#ifndef UVM_REG_MEM_BUILT_IN_SEQ_H_
#define UVM_REG_MEM_BUILT_IN_SEQ_H_

#include <systemc>
#include <string>
#include <sstream>
#include <vector>

#include "uvmsc/conf/uvm_resource_db.h"
#include "uvmsc/reg/uvm_reg_sequence.h"
#include "uvmsc/reg/uvm_reg_block.h"
#include "uvmsc/reg/uvm_reg_map.h"
#include "uvmsc/reg/uvm_reg_model.h"
#include "uvmsc/reg/sequences/uvm_reg_hw_reset_seq.h"
#include "uvmsc/reg/sequences/uvm_reg_bit_bash_seq.h"
#include "uvmsc/reg/sequences/uvm_reg_access_seq.h"
#include "uvmsc/reg/sequences/uvm_mem_access_seq.h"
#include "uvmsc/reg/sequences/uvm_reg_mem_shared_access_seq.h"
#include "uvmsc/reg/sequences/uvm_mem_walk_seq.h"

namespace uvm
{

//----------------------------------------------------------------------
// Class: uvm_reg_mem_built_in_seq
//
//! Sequence that executes a user-defined selection
//! of pre-defined register and memory test sequences.
//----------------------------------------------------------------------

class uvm_reg_mem_built_in_seq
: public uvm_reg_sequence<uvm_sequence<uvm_reg_item> >
{
 public:
  UVM_OBJECT_UTILS(uvm_reg_mem_built_in_seq);

  uvm_reg_mem_built_in_seq( const std::string& name = "uvm_reg_mem_built_in_seq" )
  : uvm_reg_sequence<uvm_sequence<uvm_reg_item> >(name), tests(UVM_DO_ALL_REG_MEM_TESTS)
  {}

  // Variable: model
  //
  // The block to be tested. Declared in the base class.
  //
  //| uvm_reg_block* model;

  // Variable: tests
  //
  // The pre-defined test sequences to be executed,
  // a bitwise OR of #uvm_reg_mem_tests_e values.
  //
  sc_dt::uint64 tests;

  // Member function: body
  //
  // Executes any or all the built-in register and memory sequences.
  // Do not call directly. Use seq.start() instead.
  //
  virtual void body()
  {
    if (model == nullptr)
    {
      UVM_ERROR("uvm_reg_mem_built_in_seq", "Not block or system specified to run sequence on");
      return;
    }

    uvm_report_info("START_SEQ", "\n\nStarting " + get_name() + " sequence...\n", UVM_LOW);

    if ((tests & UVM_DO_REG_HW_RESET) &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_TESTS", false) == nullptr &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_HW_RESET_TEST", false) == nullptr)
    {
      uvm_reg_hw_reset_seq* seq = uvm_reg_hw_reset_seq::type_id::create("reg_hw_reset_seq");
      seq->model = model;
      seq->start(nullptr, this);
      UVM_INFO("FINISH_SEQ", "Finished " + seq->get_name() + " sequence.", UVM_LOW);
      uvm_reg_hw_reset_seq::type_id::destroy(seq);
    }

    if ((tests & UVM_DO_REG_BIT_BASH) &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_TESTS", false) == nullptr &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_BIT_BASH_TEST", false) == nullptr)
    {
      uvm_reg_bit_bash_seq* seq = uvm_reg_bit_bash_seq::type_id::create("reg_bit_bash_seq");
      seq->model = model;
      seq->start(nullptr, this);
      UVM_INFO("FINISH_SEQ", "Finished " + seq->get_name() + " sequence.", UVM_LOW);
      uvm_reg_bit_bash_seq::type_id::destroy(seq);
    }

    if ((tests & UVM_DO_REG_ACCESS) &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_TESTS", false) == nullptr &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_ACCESS_TEST", false) == nullptr)
    {
      uvm_reg_access_seq* seq = uvm_reg_access_seq::type_id::create("reg_access_seq");
      seq->model = model;
      seq->start(nullptr, this);
      UVM_INFO("FINISH_SEQ", "Finished " + seq->get_name() + " sequence.", UVM_LOW);
      uvm_reg_access_seq::type_id::destroy(seq);
    }

    if ((tests & UVM_DO_MEM_ACCESS) &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_TESTS", false) == nullptr &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_MEM_TESTS", false) == nullptr &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_MEM_ACCESS_TEST", false) == nullptr)
    {
      uvm_mem_access_seq* seq = uvm_mem_access_seq::type_id::create("mem_access_seq");
      seq->model = model;
      seq->start(nullptr, this);
      UVM_INFO("FINISH_SEQ", "Finished " + seq->get_name() + " sequence.", UVM_LOW);
      uvm_mem_access_seq::type_id::destroy(seq);
    }

    if ((tests & UVM_DO_SHARED_ACCESS) &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_TESTS", false) == nullptr &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_SHARED_ACCESS_TEST", false) == nullptr)
    {
      uvm_reg_mem_shared_access_seq* seq = uvm_reg_mem_shared_access_seq::type_id::create("shared_access_seq");
      seq->model = model;
      seq->start(nullptr, this);
      UVM_INFO("FINISH_SEQ", "Finished " + seq->get_name() + " sequence.", UVM_LOW);
      uvm_reg_mem_shared_access_seq::type_id::destroy(seq);
    }

    if ((tests & UVM_DO_MEM_WALK) &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_REG_TESTS", false) == nullptr &&
        uvm_resource_db<bool>::get_by_name("REG::" + model->get_full_name(),
          "NO_MEM_WALK_TEST", false) == nullptr)
    {
      uvm_mem_walk_seq* seq = uvm_mem_walk_seq::type_id::create("mem_walk_seq");
      seq->model = model;
      seq->start(nullptr, this);
      UVM_INFO("FINISH_SEQ", "Finished " + seq->get_name() + " sequence.", UVM_LOW);
      uvm_mem_walk_seq::type_id::destroy(seq);
    }
  }

}; // class uvm_reg_mem_built_in_seq

} // namespace uvm

#endif // UVM_REG_MEM_BUILT_IN_SEQ_H_
