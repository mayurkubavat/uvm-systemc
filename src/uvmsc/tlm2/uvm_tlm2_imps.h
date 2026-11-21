//----------------------------------------------------------------------
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

#ifndef UVM_TLM2_IMPS_H_
#define UVM_TLM2_IMPS_H_

#include <string>

#include "uvmsc/base/uvm_export_base.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/tlm2/uvm_tlm2_ifs.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_b_transport_imp<T, IMP>
//
//! Blocking transport implementation export forwarding to IMP::b_transport.
//----------------------------------------------------------------------

template <typename T, typename IMP>
class uvm_tlm_b_transport_imp
  : public uvm_export_base<uvm_tlm_if_base<T, uvm_tlm_phase_e> >,
    public virtual uvm_tlm_if_base<T, uvm_tlm_phase_e>
{
 public:
  explicit uvm_tlm_b_transport_imp( IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, uvm_tlm_phase_e> >(),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  uvm_tlm_b_transport_imp( const std::string& name, IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, uvm_tlm_phase_e> >(name),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  virtual void b_transport( T& t, uvm_tlm_time& delay )
  {
    if (m_imp == nullptr)
    {
      UVM_FATAL("UVM/TLM2/NULLIMP",
                "uvm_tlm_b_transport_imp has null implementation pointer.");
      return;
    }
    m_imp->b_transport(t, delay);
  }

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_tlm_b_transport_imp";
  }

 private:
  IMP* m_imp;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_transport_fw_imp<T, P, IMP>
//
//! Non-blocking forward transport implementation export.
//----------------------------------------------------------------------

template <typename T, typename P, typename IMP>
class uvm_tlm_nb_transport_fw_imp
  : public uvm_export_base<uvm_tlm_if_base<T, P> >,
    public virtual uvm_tlm_if_base<T, P>
{
 public:
  explicit uvm_tlm_nb_transport_fw_imp( IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, P> >(),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  uvm_tlm_nb_transport_fw_imp( const std::string& name, IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, P> >(name),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  virtual uvm_tlm_sync_e nb_transport_fw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_imp == nullptr)
    {
      UVM_FATAL("UVM/TLM2/NULLIMP",
                "uvm_tlm_nb_transport_fw_imp has null implementation pointer.");
      return UVM_TLM_COMPLETED;
    }
    return m_imp->nb_transport_fw(t, p, delay);
  }

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_tlm_nb_transport_fw_imp";
  }

 private:
  IMP* m_imp;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_transport_bw_imp<T, P, IMP>
//
//! Non-blocking backward transport implementation export.
//----------------------------------------------------------------------

template <typename T, typename P, typename IMP>
class uvm_tlm_nb_transport_bw_imp
  : public uvm_export_base<uvm_tlm_if_base<T, P> >,
    public virtual uvm_tlm_if_base<T, P>
{
 public:
  explicit uvm_tlm_nb_transport_bw_imp( IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, P> >(),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  uvm_tlm_nb_transport_bw_imp( const std::string& name, IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, P> >(name),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  virtual uvm_tlm_sync_e nb_transport_bw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_imp == nullptr)
    {
      UVM_FATAL("UVM/TLM2/NULLIMP",
                "uvm_tlm_nb_transport_bw_imp has null implementation pointer.");
      return UVM_TLM_COMPLETED;
    }
    return m_imp->nb_transport_bw(t, p, delay);
  }

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_tlm_nb_transport_bw_imp";
  }

 private:
  IMP* m_imp;
};

//----------------------------------------------------------------------
// Class: uvm_tlm_nb_transport_imp<T, P, IMP>
//
//! Combined forward/backward non-blocking implementation export.
//----------------------------------------------------------------------

template <typename T, typename P, typename IMP>
class uvm_tlm_nb_transport_imp
  : public uvm_export_base<uvm_tlm_if_base<T, P> >,
    public virtual uvm_tlm_if_base<T, P>
{
 public:
  explicit uvm_tlm_nb_transport_imp( IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, P> >(),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  uvm_tlm_nb_transport_imp( const std::string& name, IMP* imp )
    : uvm_export_base<uvm_tlm_if_base<T, P> >(name),
      m_imp(imp)
  {
    this->m_connect(*this);
  }

  virtual uvm_tlm_sync_e nb_transport_fw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_imp == nullptr)
    {
      UVM_FATAL("UVM/TLM2/NULLIMP",
                "uvm_tlm_nb_transport_imp has null implementation pointer.");
      return UVM_TLM_COMPLETED;
    }
    return m_imp->nb_transport_fw(t, p, delay);
  }

  virtual uvm_tlm_sync_e nb_transport_bw( T& t, P& p, uvm_tlm_time& delay )
  {
    if (m_imp == nullptr)
    {
      UVM_FATAL("UVM/TLM2/NULLIMP",
                "uvm_tlm_nb_transport_imp has null implementation pointer.");
      return UVM_TLM_COMPLETED;
    }
    return m_imp->nb_transport_bw(t, p, delay);
  }

  virtual const std::string get_type_name() const
  {
    return "uvm::uvm_tlm_nb_transport_imp";
  }

 private:
  IMP* m_imp;
};

} // namespace uvm

#endif // UVM_TLM2_IMPS_H_
