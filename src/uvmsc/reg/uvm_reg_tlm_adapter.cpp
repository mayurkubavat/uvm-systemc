//----------------------------------------------------------------------
//   Copyright 2013-2014 NXP B.V.
//   Copyright 2004-2009 Synopsys, Inc.
//   Copyright 2010 Mentor Graphics Corporation
//   Copyright 2014 Fraunhofer-Gesellschaft zur Foerderung
//					der angewandten Forschung e.V.
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

#include <algorithm>
#include <vector>

#include "uvmsc/reg/uvm_reg_tlm_adapter.h"
#include "uvmsc/reg/uvm_reg_item.h"
#include "uvmsc/tlm2/uvm_tlm_gp.h"

namespace uvm {

//----------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------

uvm_reg_tlm_adapter::uvm_reg_tlm_adapter( const std::string& name )
  : uvm_reg_adapter(name)
{
  supports_byte_enable = true;
  provides_responses = true;
}

//----------------------------------------------------------------------
// member function: reg2bus
//
//! Converts a #uvm_reg_bus_op struct to a #uvm_tlm_gp item.
//----------------------------------------------------------------------

uvm_sequence_item* uvm_reg_tlm_adapter::reg2bus( const uvm_reg_bus_op& rw )
{
  uvm_tlm_gp* gp = uvm_tlm_gp::type_id::create("tlm_gp", nullptr, this->get_full_name());

  if (rw.kind == UVM_WRITE || rw.kind == UVM_BURST_WRITE)
    gp->set_command(UVM_TLM_WRITE_COMMAND);
  else if (rw.kind == UVM_READ || rw.kind == UVM_BURST_READ)
    gp->set_command(UVM_TLM_READ_COMMAND);
  else
    gp->set_command(UVM_TLM_IGNORE_COMMAND);

  gp->set_address(rw.addr.to_uint64());

  unsigned int nbytes = (rw.n_bits > 0) ? ((rw.n_bits - 1u) / 8u + 1u) : 0u;

  uvm_reg_item* item = get_item();
  if (item != nullptr && item->element_kind == UVM_FIELD)
  {
    unsigned int highest_be_bytes = 0;
    for (unsigned int z = 0; z < UVM_REG_BYTENABLE_WIDTH; ++z)
    {
      if (rw.byte_en[z].to_bool())
        highest_be_bytes = z + 1u;
    }
    if (highest_be_bytes > nbytes)
      nbytes = highest_be_bytes;
  }

  std::vector<unsigned char> data(nbytes, 0);
  uvm_reg_data_t tmp_data = rw.data;
  for (unsigned int i = 0; i < nbytes; ++i)
  {
    data[i] = static_cast<unsigned char>(tmp_data.to_uint() & 0xFFu);
    tmp_data >>= 8;
  }

  std::vector<unsigned char> byte_en(nbytes, UVM_TLM_BYTE_DISABLED);
  for (unsigned int i = 0; i < nbytes; ++i)
  {
    if (i < UVM_REG_BYTENABLE_WIDTH && rw.byte_en[i].to_bool())
      byte_en[i] = UVM_TLM_BYTE_ENABLED;
  }

  gp->set_data_length(nbytes);
  gp->set_streaming_width(nbytes);
  gp->set_data(data);
  gp->set_byte_enable_length(nbytes);
  gp->set_byte_enable(byte_en);
  gp->set_response_status(UVM_TLM_INCOMPLETE_RESPONSE);

  return gp;
}

//----------------------------------------------------------------------
// member function: bus2reg
//
//! Converts a #uvm_tlm_gp item to a #uvm_reg_bus_op
//! into the provided \p rw transaction.
//----------------------------------------------------------------------

void uvm_reg_tlm_adapter::bus2reg( const uvm_sequence_item* bus_item,
                                   uvm_reg_bus_op& rw )
{
  if (bus_item == nullptr)
  {
    UVM_FATAL("REG/NULL_ITEM", "bus2reg: bus_item argument is nullptr");
    rw.status = UVM_NOT_OK;
    return;
  }

  const uvm_tlm_gp* gp = dynamic_cast<const uvm_tlm_gp*>(bus_item);
  if (gp == nullptr)
  {
    UVM_ERROR("WRONG_TYPE", "Provided bus_item is not of type uvm_tlm_gp");
    rw.status = UVM_NOT_OK;
    return;
  }

  if (gp->get_command() == UVM_TLM_WRITE_COMMAND)
    rw.kind = UVM_WRITE;
  else
    rw.kind = UVM_READ;

  rw.addr = gp->get_address();

  std::vector<unsigned char> data;
  gp->get_data(data);
  const unsigned int data_len = gp->get_data_length();
  const unsigned int avail_data = std::min(data_len, static_cast<unsigned int>(data.size()));
  const unsigned int max_data_bytes = (UVM_REG_DATA_WIDTH - 1u) / 8u + 1u;
  const unsigned int copy_data_bytes = std::min(avail_data, max_data_bytes);

  rw.data = 0;
  for (unsigned int i = copy_data_bytes; i > 0; --i)
  {
    rw.data <<= 8;
    rw.data |= static_cast<unsigned int>(data[i - 1u]);
  }
  rw.n_bits = copy_data_bytes * 8u;

  std::vector<unsigned char> byte_en;
  gp->get_byte_enable(byte_en);
  const unsigned int be_len = gp->get_byte_enable_length();
  const unsigned int avail_be = std::min(be_len, static_cast<unsigned int>(byte_en.size()));

  if (avail_be == 0)
  {
    rw.byte_en = -1;
  }
  else
  {
    rw.byte_en = 0;
    const unsigned int be_lanes = (copy_data_bytes > 0) ? copy_data_bytes : avail_be;
    const unsigned int max_be_lanes = std::min(be_lanes, static_cast<unsigned int>(UVM_REG_BYTENABLE_WIDTH));
    for (unsigned int i = 0; i < max_be_lanes; ++i)
    {
      rw.byte_en[i] = (byte_en[i % avail_be] != UVM_TLM_BYTE_DISABLED);
    }
  }

  rw.status = gp->is_response_ok() ? UVM_IS_OK : UVM_NOT_OK;
}

} // namespace uvm
