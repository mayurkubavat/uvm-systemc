//----------------------------------------------------------------------
//   Copyright 2007-2018 Cadence Design Systems, Inc.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2015-2024 NVIDIA Corporation
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

#ifndef UVM_BIDIR_EXPORTS_H_
#define UVM_BIDIR_EXPORTS_H_

#include <systemc>
#include <tlm>

#include <string>

#include "uvmsc/base/uvm_export_base.h"
#include "uvmsc/tlm1/uvm_tlm_transport_ifs.h"

namespace uvm {

//---------------------------------------------------------------------------
// TLM1 transport exports
//---------------------------------------------------------------------------

//! The export uvm_blocking_transport_export provides the blocking interface
//! tlm::tlm_transport_if<REQ, RSP> to transport a request and its response in one call.

template <typename REQ, typename RSP = REQ>
class uvm_blocking_transport_export
: public uvm_export_base< tlm::tlm_transport_if<REQ, RSP> >
{
 public:
  uvm_blocking_transport_export() :
    uvm_export_base< tlm::tlm_transport_if<REQ, RSP> >() {}

  uvm_blocking_transport_export( const std::string& name ) :
    uvm_export_base< tlm::tlm_transport_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_transport_export";
  }

  virtual ~uvm_blocking_transport_export() {}
};

//! The export uvm_nonblocking_transport_export provides the non-blocking interface
//! uvm_tlm_nonblocking_transport_if<REQ, RSP> to transport a request and its response in one call.

template <typename REQ, typename RSP = REQ>
class uvm_nonblocking_transport_export
: public uvm_export_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >
{
 public:
  uvm_nonblocking_transport_export() :
    uvm_export_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >() {}

  uvm_nonblocking_transport_export( const std::string& name ) :
    uvm_export_base< uvm_tlm_nonblocking_transport_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_transport_export";
  }

  virtual ~uvm_nonblocking_transport_export() {}
};

//! The export uvm_transport_export provides the blocking and non-blocking interface
//! uvm_tlm_transport_if<REQ, RSP> to transport a request and its response in one call.

template <typename REQ, typename RSP = REQ>
class uvm_transport_export
: public uvm_export_base< uvm_tlm_transport_if<REQ, RSP> >
{
 public:
  uvm_transport_export() :
    uvm_export_base< uvm_tlm_transport_if<REQ, RSP> >() {}

  uvm_transport_export( const std::string& name ) :
    uvm_export_base< uvm_tlm_transport_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_transport_export";
  }

  virtual ~uvm_transport_export() {}
};

//---------------------------------------------------------------------------
// TLM1 master exports
//---------------------------------------------------------------------------

//! The export uvm_blocking_master_export provides the blocking interface
//! tlm::tlm_blocking_master_if<REQ, RSP> to put requests and get or peek responses.

template <typename REQ, typename RSP = REQ>
class uvm_blocking_master_export
: public uvm_export_base< tlm::tlm_blocking_master_if<REQ, RSP> >
{
 public:
  uvm_blocking_master_export() :
    uvm_export_base< tlm::tlm_blocking_master_if<REQ, RSP> >() {}

  uvm_blocking_master_export( const std::string& name ) :
    uvm_export_base< tlm::tlm_blocking_master_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_master_export";
  }

  virtual ~uvm_blocking_master_export() {}
};

//! The export uvm_nonblocking_master_export provides the non-blocking interface
//! tlm::tlm_nonblocking_master_if<REQ, RSP> to put requests and get or peek responses.

template <typename REQ, typename RSP = REQ>
class uvm_nonblocking_master_export
: public uvm_export_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >
{
 public:
  uvm_nonblocking_master_export() :
    uvm_export_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >() {}

  uvm_nonblocking_master_export( const std::string& name ) :
    uvm_export_base< tlm::tlm_nonblocking_master_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_master_export";
  }

  virtual ~uvm_nonblocking_master_export() {}
};

//! The export uvm_master_export provides the blocking and non-blocking interface
//! tlm::tlm_master_if<REQ, RSP> to put requests and get or peek responses.

template <typename REQ, typename RSP = REQ>
class uvm_master_export
: public uvm_export_base< tlm::tlm_master_if<REQ, RSP> >
{
 public:
  uvm_master_export() :
    uvm_export_base< tlm::tlm_master_if<REQ, RSP> >() {}

  uvm_master_export( const std::string& name ) :
    uvm_export_base< tlm::tlm_master_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_master_export";
  }

  virtual ~uvm_master_export() {}
};

//---------------------------------------------------------------------------
// TLM1 slave exports
//---------------------------------------------------------------------------

//! The export uvm_blocking_slave_export provides the blocking interface
//! tlm::tlm_blocking_slave_if<REQ, RSP> to get or peek requests and put responses.

template <typename REQ, typename RSP = REQ>
class uvm_blocking_slave_export
: public uvm_export_base< tlm::tlm_blocking_slave_if<REQ, RSP> >
{
 public:
  uvm_blocking_slave_export() :
    uvm_export_base< tlm::tlm_blocking_slave_if<REQ, RSP> >() {}

  uvm_blocking_slave_export( const std::string& name ) :
    uvm_export_base< tlm::tlm_blocking_slave_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_blocking_slave_export";
  }

  virtual ~uvm_blocking_slave_export() {}
};

//! The export uvm_nonblocking_slave_export provides the non-blocking interface
//! tlm::tlm_nonblocking_slave_if<REQ, RSP> to get or peek requests and put responses.

template <typename REQ, typename RSP = REQ>
class uvm_nonblocking_slave_export
: public uvm_export_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >
{
 public:
  uvm_nonblocking_slave_export() :
    uvm_export_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >() {}

  uvm_nonblocking_slave_export( const std::string& name ) :
    uvm_export_base< tlm::tlm_nonblocking_slave_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_nonblocking_slave_export";
  }

  virtual ~uvm_nonblocking_slave_export() {}
};

//! The export uvm_slave_export provides the blocking and non-blocking interface
//! tlm::tlm_slave_if<REQ, RSP> to get or peek requests and put responses.

template <typename REQ, typename RSP = REQ>
class uvm_slave_export
: public uvm_export_base< tlm::tlm_slave_if<REQ, RSP> >
{
 public:
  uvm_slave_export() :
    uvm_export_base< tlm::tlm_slave_if<REQ, RSP> >() {}

  uvm_slave_export( const std::string& name ) :
    uvm_export_base< tlm::tlm_slave_if<REQ, RSP> >( name ) {}

  // exports have no implementation

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_slave_export";
  }

  virtual ~uvm_slave_export() {}
};

} // namespace uvm

#endif // UVM_BIDIR_EXPORTS_H_
