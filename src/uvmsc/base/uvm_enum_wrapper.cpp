//----------------------------------------------------------------------
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2010-2018 Synopsys, Inc.
//   Copyright 2013-2024 NVIDIA Corporation
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


#include "uvmsc/base/uvm_enum_wrapper.h"
#include "uvmsc/base/uvm_object_globals.h"

namespace uvm {

//----------------------------------------------------------------------
// member function: m_init_map (static)
//
//! Implementation defined
//! Registers the names of the values of the UVM enumeration types
//----------------------------------------------------------------------

template <>
void uvm_enum_wrapper<uvm_active_passive_enum>::m_init_map()
{
  m_add_names("UVM_PASSIVE, UVM_ACTIVE", { UVM_PASSIVE, UVM_ACTIVE });
}

template <>
void uvm_enum_wrapper<uvm_verbosity>::m_init_map()
{
  m_add_names("UVM_NONE, UVM_LOW, UVM_MEDIUM, UVM_HIGH, UVM_FULL, UVM_DEBUG",
              { UVM_NONE, UVM_LOW, UVM_MEDIUM, UVM_HIGH, UVM_FULL, UVM_DEBUG });
}

template <>
void uvm_enum_wrapper<uvm_severity>::m_init_map()
{
  m_add_names("UVM_INFO, UVM_WARNING, UVM_ERROR, UVM_FATAL",
              { UVM_INFO, UVM_WARNING, UVM_ERROR, UVM_FATAL });
}

template <>
void uvm_enum_wrapper<uvm_sequencer_arb_mode>::m_init_map()
{
  m_add_names("UVM_SEQ_ARB_FIFO, UVM_SEQ_ARB_WEIGHTED, UVM_SEQ_ARB_RANDOM, "
              "UVM_SEQ_ARB_STRICT_FIFO, UVM_SEQ_ARB_STRICT_RANDOM, UVM_SEQ_ARB_USER",
              { SEQ_ARB_FIFO, SEQ_ARB_WEIGHTED, SEQ_ARB_RANDOM,
                SEQ_ARB_STRICT_FIFO, SEQ_ARB_STRICT_RANDOM, SEQ_ARB_USER });
  // the names of the UVM-SystemC enumerators
  m_add_names("SEQ_ARB_FIFO, SEQ_ARB_WEIGHTED, SEQ_ARB_RANDOM, "
              "SEQ_ARB_STRICT_FIFO, SEQ_ARB_STRICT_RANDOM, SEQ_ARB_USER",
              { SEQ_ARB_FIFO, SEQ_ARB_WEIGHTED, SEQ_ARB_RANDOM,
                SEQ_ARB_STRICT_FIFO, SEQ_ARB_STRICT_RANDOM, SEQ_ARB_USER });
}

template <>
void uvm_enum_wrapper<uvm_phase_state>::m_init_map()
{
  m_add_names("UVM_PHASE_DORMANT, UVM_PHASE_SCHEDULED, UVM_PHASE_SYNCING, "
              "UVM_PHASE_STARTED, UVM_PHASE_EXECUTING, UVM_PHASE_READY_TO_END, "
              "UVM_PHASE_ENDED, UVM_PHASE_CLEANUP, UVM_PHASE_DONE, UVM_PHASE_JUMPING",
              { UVM_PHASE_DORMANT, UVM_PHASE_SCHEDULED, UVM_PHASE_SYNCING,
                UVM_PHASE_STARTED, UVM_PHASE_EXECUTING, UVM_PHASE_READY_TO_END,
                UVM_PHASE_ENDED, UVM_PHASE_CLEANUP, UVM_PHASE_DONE, UVM_PHASE_JUMPING });
}

template <>
void uvm_enum_wrapper<uvm_radix_enum>::m_init_map()
{
  m_add_names("UVM_BIN, UVM_DEC, UVM_UNSIGNED, UVM_UNFORMAT2, UVM_UNFORMAT4, "
              "UVM_OCT, UVM_HEX, UVM_STRING, UVM_TIME, UVM_ENUM, UVM_REAL, "
              "UVM_REAL_DEC, UVM_REAL_EXP, UVM_NORADIX",
              { UVM_BIN, UVM_DEC, UVM_UNSIGNED, UVM_UNFORMAT2, UVM_UNFORMAT4,
                UVM_OCT, UVM_HEX, UVM_STRING, UVM_TIME, UVM_ENUM, UVM_REAL,
                UVM_REAL_DEC, UVM_REAL_EXP, UVM_NORADIX });
}

} // namespace uvm
