//----------------------------------------------------------------------
//   Copyright 2004-2011 Synopsys, Inc.
//   Copyright 2010-2011 Mentor Graphics Corporation
//   Copyright 2010-2011 Cadence Design Systems, Inc.
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

#ifndef UVM_REG_TRANSACTION_ORDER_POLICY_H_
#define UVM_REG_TRANSACTION_ORDER_POLICY_H_

#include <string>
#include <vector>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/reg/uvm_reg_bus_op.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_reg_transaction_order_policy
//
//! Reorders the bus transactions which one register access produces,
//! when the register is wider than the bus. Set it on a map with
//! uvm_reg_map::set_transaction_order_policy().
//----------------------------------------------------------------------

class uvm_reg_transaction_order_policy : public uvm_object
{
 public:
  explicit uvm_reg_transaction_order_policy( const std::string& name = "policy" )
  : uvm_object(name)
  {}

  //! May reorder the bus transactions in \p q, which come in the order of
  //! their addresses. The first item of the queue is the first bus
  //! transaction, the last item the final one.
  virtual void order( std::vector<uvm_reg_bus_op>& q ) = 0;

  virtual const std::string get_type_name() const
  {
    return "uvm_reg_transaction_order_policy";
  }
};

} // namespace uvm

#endif // UVM_REG_TRANSACTION_ORDER_POLICY_H_
