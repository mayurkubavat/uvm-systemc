//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2014-2024 NVIDIA Corporation
//   Copyright 2014 Semifore
//   Copyright 2014-2018 Synopsys, Inc.
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

#ifndef UVM_ALGORITHMIC_COMPARATOR_H_
#define UVM_ALGORITHMIC_COMPARATOR_H_

#include <string>

#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_component_name.h"
#include "uvmsc/comps/uvm_in_order_comparator.h"
#include "uvmsc/tlm1/uvm_analysis_export.h"
#include "uvmsc/tlm1/uvm_analysis_imp.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_algorithmic_comparator
//
//! Compares two streams of data objects of different types, BEFORE and
//! AFTER.
//!
//! The algorithmic comparator is a wrapper around
//! uvm_in_order_class_comparator. Like the in-order comparator, the
//! algorithmic comparator compares two streams of transactions, the
//! BEFORE stream and the AFTER stream. It is often the case when two
//! streams of transactions need to be compared that the two streams are
//! in different forms. That is, the type of the BEFORE transaction
//! stream is different than the type of the AFTER transaction stream.
//!
//! The comparator's transformer object provides the member function
//!
//!   AFTER transform( const BEFORE& b );
//!
//! which is called for each transaction of the BEFORE stream. Its
//! result is compared in order with the transactions of the AFTER
//! stream. AFTER shall be a class derived from uvm_object which has a
//! default constructor, as it is compared, copied and converted to a
//! string by the in-order comparator.
//!
//! Matches and mismatches are reported in terms of the AFTER
//! transactions.
//----------------------------------------------------------------------

template < typename BEFORE = int,
           typename AFTER = int,
           typename TRANSFORMER = int >
class uvm_algorithmic_comparator : public uvm_component
{
 public:
  typedef uvm_algorithmic_comparator<BEFORE, AFTER, TRANSFORMER> this_type;

  //! The export to which the stream of BEFORE transactions is written.
  //! Each transaction is transformed, and the result is compared in
  //! order with the transactions of the AFTER stream.
  uvm_analysis_imp<BEFORE, this_type> before_export;

  //! The export to which the stream of AFTER transactions is written.
  uvm_analysis_export<AFTER> after_export;

  //! Creates an instance of the comparator. The \p transformer is the
  //! object which transforms the BEFORE transactions. The comparator
  //! does not take ownership of it.
  explicit uvm_algorithmic_comparator( uvm_component_name name,
                                       TRANSFORMER* transformer = nullptr )
  : uvm_component(name),
    before_export("before_analysis_export", this),
    after_export("after_analysis_export"),
    m_transformer(transformer),
    comp("comp")
  {
    after_export.bind(comp.after_export);
  }

  virtual const std::string get_type_name() const
  {
    return "uvm_algorithmic_comparator #(BEFORE,AFTER,TRANSFORMER)";
  }

  //! Transforms a transaction of the BEFORE stream and passes the
  //! result on to the in-order comparator.
  void write( const BEFORE& b )
  {
    if (m_transformer == nullptr)
    {
      uvm_report_fatal("NOTRANSFORMER",
        "A transaction was written to the comparator, but it has no transformer");
      return;
    }

    comp.before_export->write(m_transformer->transform(b));
  }

 private:
  TRANSFORMER* m_transformer;
  uvm_in_order_class_comparator<AFTER> comp;

}; // class uvm_algorithmic_comparator

} // namespace uvm

#endif // UVM_ALGORITHMIC_COMPARATOR_H_
