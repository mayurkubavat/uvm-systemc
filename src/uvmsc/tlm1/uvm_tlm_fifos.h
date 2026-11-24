//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2017 Cisco Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2014-2024 NVIDIA Corporation
//   Copyright 2014 Semifore
//   Copyright 2010-2018 Synopsys, Inc.
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

#ifndef UVM_TLM_FIFOS_H_
#define UVM_TLM_FIFOS_H_

#include <string>

#include <systemc>
#include <tlm>

#include "uvmsc/tlm1/uvm_analysis_export.h"
#include "uvmsc/tlm1/uvm_analysis_port.h"
#include "uvmsc/tlm1/uvm_exports.h"

namespace uvm {

//----------------------------------------------------------------------
// CLASS: uvm_tlm_fifo
//
//! The class uvm_tlm_fifo provides storage of transactions between two
//! independently running processes. Transactions are put into the FIFO
//! via the put interface and are retrieved, in the order they arrived,
//! via the get and peek interfaces.
//!
//! The class is a tlm::tlm_fifo with the member functions and the
//! exports of the UVM class. A port can be connected to the FIFO itself
//! or to one of its exports. As for a tlm::tlm_fifo, a transaction can
//! be retrieved from the delta cycle after the one in which it was put.
//! Because of its exports, the FIFO has to be created in a component
//! or another module.
//----------------------------------------------------------------------

template <typename T = int>
class uvm_tlm_fifo : public tlm::tlm_fifo<T>
{
 public:

  //! Export of the put interfaces
  uvm_put_export<T> put_export;

  //! Export of the get and peek interfaces
  uvm_get_peek_export<T> get_peek_export;

  //! Transactions passed through the put interfaces
  uvm_analysis_port<T> put_ap;

  //! Transactions passed through the get interfaces
  uvm_analysis_port<T> get_ap;

  // The following are aliases of the exports above
  uvm_put_export<T>& blocking_put_export;
  uvm_put_export<T>& nonblocking_put_export;

  uvm_get_peek_export<T>& blocking_get_export;
  uvm_get_peek_export<T>& nonblocking_get_export;
  uvm_get_peek_export<T>& get_export;

  uvm_get_peek_export<T>& blocking_peek_export;
  uvm_get_peek_export<T>& nonblocking_peek_export;
  uvm_get_peek_export<T>& peek_export;

  uvm_get_peek_export<T>& blocking_get_peek_export;
  uvm_get_peek_export<T>& nonblocking_get_peek_export;

  //! Constructor. The argument size_ is the maximum number of
  //! transactions the FIFO holds; 0 indicates no limit.
  explicit uvm_tlm_fifo( const std::string& name, int size_ = 1 )
  : tlm::tlm_fifo<T>(name.c_str(), m_tlm_size(size_)),
    put_export(m_sub_name(name, "put_export")),
    get_peek_export(m_sub_name(name, "get_peek_export")),
    put_ap(m_sub_name(name, "put_ap")),
    get_ap(m_sub_name(name, "get_ap")),
    blocking_put_export(put_export),
    nonblocking_put_export(put_export),
    blocking_get_export(get_peek_export),
    nonblocking_get_export(get_peek_export),
    get_export(get_peek_export),
    blocking_peek_export(get_peek_export),
    nonblocking_peek_export(get_peek_export),
    peek_export(get_peek_export),
    blocking_get_peek_export(get_peek_export),
    nonblocking_get_peek_export(get_peek_export)
  {
    put_export.bind(*this);
    get_peek_export.bind(*this);
  }

  //! Constructor without name, as for tlm::tlm_fifo. A unique name is
  //! generated.
  explicit uvm_tlm_fifo( int size_ )
  : uvm_tlm_fifo(std::string(sc_core::sc_gen_unique_name("fifo")), size_)
  {}

  virtual ~uvm_tlm_fifo() {}

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_tlm_fifo";
  }

  virtual const char* kind() const
  {
    return "uvm::uvm_tlm_fifo";
  }

  // The member functions size and used are those of tlm::tlm_fifo:
  //
  // int size() const;
  //   Returns the capacity of the FIFO, that is the number of
  //   transactions it is capable of holding. Other than in
  //   SystemVerilog, where 0 is returned, the value is negative when
  //   the FIFO capacity has no limit. Use is_unbounded to test for it.
  //
  // int used() const;
  //   Returns the number of transactions stored in the FIFO which can
  //   be retrieved.

  //! Returns true when the FIFO capacity has no limit.
  bool is_unbounded() const
  {
    return this->size() < 0;
  }

  //! Returns true when there are no transactions in the FIFO.
  bool is_empty() const
  {
    return this->used() == 0;
  }

  //! Returns true when the FIFO has a limit and no transaction can be
  //! put. The place of a transaction which has been retrieved becomes
  //! available in the next delta cycle.
  bool is_full() const
  {
    return !this->nb_can_put();
  }

  //! Removes all transactions which can be retrieved from the FIFO.
  //! After this call used returns 0 and is_empty returns true. A
  //! transaction put in the current delta cycle is not removed. The
  //! removed transactions are not written to get_ap.
  virtual void flush()
  {
    while (this->nb_can_get())
      tlm::tlm_fifo<T>::get();
  }

  //--------------------------------------------------------------------
  // Put and get interfaces: as tlm::tlm_fifo, and the transaction is
  // written to the analysis port put_ap or get_ap.
  //--------------------------------------------------------------------

  virtual void put( const T& t )
  {
    tlm::tlm_fifo<T>::put(t);
    put_ap.write(t);
  }

  virtual bool nb_put( const T& t )
  {
    if (!tlm::tlm_fifo<T>::nb_put(t))
      return false;

    put_ap.write(t);
    return true;
  }

  virtual T get( tlm::tlm_tag<T>* tag = 0 )
  {
    T t = tlm::tlm_fifo<T>::get(tag);
    get_ap.write(t);
    return t;
  }

  virtual bool nb_get( T& t )
  {
    if (!tlm::tlm_fifo<T>::nb_get(t))
      return false;

    get_ap.write(t);
    return true;
  }

  // UVM names of the non-blocking interfaces

  bool try_put( const T& t ) { return nb_put(t); }
  bool can_put() const { return this->nb_can_put(); }

  bool try_get( T& t ) { return nb_get(t); }
  bool can_get() const { return this->nb_can_get(); }

  bool try_peek( T& t ) const { return this->nb_peek(t); }
  bool can_peek() const { return this->nb_can_peek(); }

  // The member functions with an output argument

  void get( T& t ) { t = get(); }
  void peek( T& t ) const { t = tlm::tlm_fifo<T>::peek(); }
  using tlm::tlm_fifo<T>::peek;

 private:

  // A tlm::tlm_fifo without limit has a negative size
  static int m_tlm_size( int size_ )
  {
    return (size_ <= 0) ? -16 : size_;
  }

  // The exports and ports are siblings of the FIFO in the object
  // hierarchy, so their names start with the name of the FIFO
  static std::string m_sub_name( const std::string& name,
                                 const std::string& sub_name )
  {
    return name + "_" + sub_name;
  }

}; // class uvm_tlm_fifo


//----------------------------------------------------------------------
// CLASS: uvm_tlm_analysis_fifo
//
//! The class uvm_tlm_analysis_fifo is a uvm_tlm_fifo with an unbounded
//! size and a write interface. It can be used any place an analysis
//! implementation is used. Typical usage is as a buffer between a
//! uvm_analysis_port in an initiator component and a component which
//! retrieves the transactions with get.
//----------------------------------------------------------------------

template <typename T = int>
class uvm_tlm_analysis_fifo : public uvm_tlm_fifo<T>,
                              public virtual tlm::tlm_analysis_if<T>
{
 public:

  //! Export of the write interface, to connect an analysis port to
  uvm_analysis_export<T> analysis_export;

  explicit uvm_tlm_analysis_fifo( const std::string& name )
  : uvm_tlm_fifo<T>(name, 0),
    analysis_export(name + "_analysis_export")
  {
    analysis_export.bind(*this);
  }

  virtual ~uvm_tlm_analysis_fifo() {}

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_tlm_analysis_fifo";
  }

  virtual const char* kind() const
  {
    return "uvm::uvm_tlm_analysis_fifo";
  }

  //! Puts the transaction into the FIFO. As the FIFO has no limit,
  //! this never blocks.
  virtual void write( const T& t )
  {
    this->nb_put(t);
  }

}; // class uvm_tlm_analysis_fifo

} // namespace uvm

#endif // UVM_TLM_FIFOS_H_
