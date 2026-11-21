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

#ifndef UVM_IN_ORDER_COMPARATOR_H_
#define UVM_IN_ORDER_COMPARATOR_H_

#include <string>

#include "uvmsc/base/uvm_component.h"
#include "uvmsc/base/uvm_component_name.h"
#include "uvmsc/comps/uvm_pair.h"
#include "uvmsc/comps/uvm_policies.h"
#include "uvmsc/phasing/uvm_phase.h"
#include "uvmsc/tlm1/uvm_analysis_export.h"
#include "uvmsc/tlm1/uvm_analysis_port.h"
#include "uvmsc/tlm1/uvm_tlm_fifos.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_in_order_comparator
//
//! Compares two streams of data objects of the type parameter T.
//! These transactions may either be classes or built-in types. To be
//! successfully compared, the two streams of data shall be in the same
//! order. Apart from that, there are no assumptions made about the
//! relative timing of the two streams of data.
//!
//! Type parameters:
//!
//!   T         - Specifies the type of transactions to be compared.
//!
//!   comp_type - A policy class with a static member function comp
//!               which compares two transactions and returns true when
//!               they match.
//!
//!   convert   - A policy class with a static member function
//!               convert2string which returns a transaction as string.
//!
//!   pair_type - A class which holds a pair of transactions in its
//!               data members first and second, and which is
//!               constructed with a name.
//!
//! Built in types (such as ints, bools, etc.) can be compared using the
//! default values for comp_type, convert, and pair_type. For
//! convenience, the subtype uvm_in_order_built_in_comparator is
//! provided for this purpose. Classes derived from uvm_object can be
//! compared with the subtype uvm_in_order_class_comparator.
//----------------------------------------------------------------------

template < typename T = int,
           typename comp_type = uvm_built_in_comp<T>,
           typename convert = uvm_built_in_converter<T>,
           typename pair_type = uvm_built_in_pair<T> >
class uvm_in_order_comparator : public uvm_component
{
 public:

  //! The export to which one stream of data is written. The port shall
  //! be connected to an analysis port that will provide such data.
  uvm_analysis_export<T> before_export;

  //! The export to which the other stream of data is written. The port
  //! shall be connected to an analysis port that will provide such
  //! data.
  uvm_analysis_export<T> after_export;

  //! The comparator sends out pairs of transactions across this
  //! analysis port. Both matched and unmatched pairs are published via
  //! a pair_type transaction. Any connected analysis export(s) will
  //! receive these transaction pairs.
  uvm_analysis_port<pair_type> pair_ap;

  //! The number of transaction pairs which matched and did not match
  int m_matches;
  int m_mismatches;

  explicit uvm_in_order_comparator( uvm_component_name name )
  : uvm_component(name),
    before_export("before_export"),
    after_export("after_export"),
    pair_ap("pair_ap"),
    m_matches(0),
    m_mismatches(0),
    m_before_fifo("before"),
    m_after_fifo("after")
  {
    before_export.bind(m_before_fifo);
    after_export.bind(m_after_fifo);
  }

  virtual const std::string get_type_name() const
  {
    return "uvm_in_order_comparator #(T,comp_type,convert,pair_type)";
  }

  //! Takes one transaction from each stream and compares them, for as
  //! long as the simulation runs. A report of the result is issued for
  //! each pair, and the pair is written to pair_ap.
  virtual void run_phase( uvm_phase& phase )
  {
    while (true)
    {
      T b = m_before_fifo.get();
      T a = m_after_fifo.get();

      if (!comp_type::comp(b, a))
      {
        uvm_report_warning("Comparator Mismatch",
          convert::convert2string(a) + " differs from " +
          convert::convert2string(b));
        m_mismatches++;
      }
      else
      {
        uvm_report_info("Comparator Match", convert::convert2string(b));
        m_matches++;
      }

      // we make the assumption here that a transaction "sent for
      // analysis" is safe from being edited by another process
      pair_type pair("after/before");
      pair.first = a;
      pair.second = b;
      pair_ap.write(pair);
    }
  }

  //! Resets the internal counters of the matches and mismatches.
  virtual void flush()
  {
    m_matches = 0;
    m_mismatches = 0;
  }

 private:
  uvm_tlm_analysis_fifo<T> m_before_fifo;
  uvm_tlm_analysis_fifo<T> m_after_fifo;

}; // class uvm_in_order_comparator


//----------------------------------------------------------------------
// CLASS: uvm_in_order_built_in_comparator
//
//! This class uses the uvm_built_in_* comparison, converter, and pair
//! classes. Use this class for built-in types (int, bool, etc.) and
//! for other types with operator== and operator<<.
//----------------------------------------------------------------------

template <typename T = int>
class uvm_in_order_built_in_comparator : public uvm_in_order_comparator<T>
{
 public:

  explicit uvm_in_order_built_in_comparator( uvm_component_name name )
  : uvm_in_order_comparator<T>(name)
  {}

  virtual const std::string get_type_name() const
  {
    return "uvm_in_order_built_in_comparator #(T)";
  }

}; // class uvm_in_order_built_in_comparator


//----------------------------------------------------------------------
// CLASS: uvm_in_order_class_comparator
//
//! This class uses the uvm_class_* comparison, converter, and pair
//! classes. Use this class for comparing user-defined objects of type
//! T, which shall provide the member functions compare and
//! convert2string, as objects derived from uvm_object do.
//----------------------------------------------------------------------

template <typename T>
class uvm_in_order_class_comparator
: public uvm_in_order_comparator< T,
                                  uvm_class_comp<T>,
                                  uvm_class_converter<T>,
                                  uvm_class_pair<T, T> >
{
 public:

  explicit uvm_in_order_class_comparator( uvm_component_name name )
  : uvm_in_order_comparator< T,
                             uvm_class_comp<T>,
                             uvm_class_converter<T>,
                             uvm_class_pair<T, T> >(name)
  {}

  virtual const std::string get_type_name() const
  {
    return "uvm_in_order_class_comparator #(T)";
  }

}; // class uvm_in_order_class_comparator

} // namespace uvm

#endif // UVM_IN_ORDER_COMPARATOR_H_
