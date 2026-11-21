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

#ifndef UVM_VERILATOR_REG_BACKDOOR_H_
#define UVM_VERILATOR_REG_BACKDOOR_H_

#include <cstdint>
#include <string>
#include <type_traits>

#include <systemc>

#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/reg/uvm_reg.h"
#include "uvmsc/reg/uvm_reg_backdoor.h"
#include "uvmsc/reg/uvm_reg_item.h"

namespace uvm {

namespace detail {

inline uvm_reg_data_t reg_data_from_value(const bool& v)
{
  return uvm_reg_data_t(v ? 1 : 0);
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value,
                        uvm_reg_data_t>::type
reg_data_from_value(const T& v)
{
  using U = typename std::make_unsigned<T>::type;
  return uvm_reg_data_t(static_cast<uint64_t>(static_cast<U>(v)));
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, uvm_reg_data_t>::type
reg_data_from_value(const T& v)
{
  return uvm_reg_data_t(v);
}

inline void reg_data_to_value(const uvm_reg_data_t& d, bool& out)
{
  out = (d.to_uint64() & 1ULL) != 0ULL;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value,
                        void>::type
reg_data_to_value(const uvm_reg_data_t& d, T& out)
{
  using U = typename std::make_unsigned<T>::type;
  out = static_cast<T>(static_cast<U>(d.to_uint64()));
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, void>::type
reg_data_to_value(const uvm_reg_data_t& d, T& out)
{
  out = d;
}

inline uint64_t reg_width_mask(unsigned int n_bits)
{
  return (n_bits == 0u || n_bits >= 64u) ? ~uint64_t(0) : ((uint64_t(1) << n_bits) - 1u);
}

} // namespace detail

//----------------------------------------------------------------------
// Class: uvm_cpp_reg_backdoor
//
//! Typed user-defined register backdoor binding a uvm_reg directly to a
//! C++ storage variable (such as a Verilator rootp /*verilator public_flat_rw*/
//! CSR flip-flop: CData/SData/IData/QData). Intentionally not registered with
//! the UVM factory so global factory tables remain unchanged.
//----------------------------------------------------------------------
template <typename StorageT>
class uvm_cpp_reg_backdoor : public uvm_reg_backdoor
{
 public:
  uvm_cpp_reg_backdoor(StorageT& storage,
                       bool writable = true,
                       unsigned int n_bits = 0,
                       const std::string& name = "uvm_cpp_reg_backdoor")
      : uvm_reg_backdoor(name),
        m_storage(&storage),
        m_writable(writable),
        m_n_bits(n_bits)
  {}

  virtual void read_func(uvm_reg_item* rw) override
  {
    if (rw == nullptr || m_storage == nullptr)
    {
      if (rw != nullptr)
        rw->status = UVM_NOT_OK;
      return;
    }

    unsigned int width = m_n_bits;
    if (width == 0u)
    {
      uvm_reg* rg = dynamic_cast<uvm_reg*>(rw->element);
      if (rg != nullptr)
        width = rg->get_n_bits();
    }

    if (rw->value.empty())
      rw->value.resize(1);
    uvm_reg_data_t raw = detail::reg_data_from_value(*m_storage);
    if (width > 0u && width < 64u)
      raw = uvm_reg_data_t(raw.to_uint64() & detail::reg_width_mask(width));
    rw->value[0] = raw;
    rw->status = UVM_IS_OK;
  }

  virtual void write(uvm_reg_item* rw) override
  {
    if (rw == nullptr || m_storage == nullptr)
    {
      if (rw != nullptr)
        rw->status = UVM_NOT_OK;
      return;
    }

    if (!m_writable)
    {
      UVM_ERROR("RegModel",
                "Backdoor write attempted on read-only C++ storage backdoor '" +
                    get_name() + "'");
      rw->status = UVM_NOT_OK;
      return;
    }

    do_pre_write(rw);

    unsigned int width = m_n_bits;
    if (width == 0u)
    {
      uvm_reg* rg = dynamic_cast<uvm_reg*>(rw->element);
      if (rg != nullptr)
        width = rg->get_n_bits();
    }

    uvm_reg_data_t val = rw->value.empty() ? uvm_reg_data_t(0) : rw->value[0];
    if (width > 0u && width < 64u)
      val = uvm_reg_data_t(val.to_uint64() & detail::reg_width_mask(width));

    detail::reg_data_to_value(val, *m_storage);
    rw->status = UVM_IS_OK;

    do_post_write(rw);
  }

 private:
  StorageT* m_storage;
  bool m_writable;
  unsigned int m_n_bits;
};

//----------------------------------------------------------------------
// Class: uvm_sc_signal_reg_backdoor
//
//! Typed user-defined register backdoor binding a uvm_reg to an arbitrary
//! width SystemC signal/port (e.g. sc_signal<uint32_t>, sc_out<uint32_t>),
//! bridging between SignalValT and uvm_reg_data_t without requiring
//! exact sc_bv<64> matching.
//----------------------------------------------------------------------
template <typename SignalValT>
class uvm_sc_signal_reg_backdoor : public uvm_reg_backdoor
{
 public:
  explicit uvm_sc_signal_reg_backdoor(
      const sc_core::sc_signal_in_if<SignalValT>& rd_if,
      sc_core::sc_signal_write_if<SignalValT>* wr_if = nullptr,
      const std::string& name = "uvm_sc_signal_reg_backdoor")
      : uvm_reg_backdoor(name),
        m_rd_if(&rd_if),
        m_wr_if(wr_if)
  {}

  virtual void read_func(uvm_reg_item* rw) override
  {
    if (rw == nullptr || m_rd_if == nullptr)
    {
      if (rw != nullptr)
        rw->status = UVM_NOT_OK;
      return;
    }
    if (rw->value.empty())
      rw->value.resize(1);
    uvm_reg* rg = dynamic_cast<uvm_reg*>(rw->element);
    const unsigned int width = (rg != nullptr) ? rg->get_n_bits() : 0u;
    uvm_reg_data_t raw = detail::reg_data_from_value(m_rd_if->read());
    if (width > 0u && width < 64u)
      raw = uvm_reg_data_t(raw.to_uint64() & detail::reg_width_mask(width));
    rw->value[0] = raw;
    rw->status = UVM_IS_OK;
  }

  virtual void write(uvm_reg_item* rw) override
  {
    if (rw == nullptr || m_wr_if == nullptr)
    {
      UVM_ERROR("RegModel",
                "Backdoor write attempted on non-writable SystemC signal backdoor '" +
                    get_name() + "'");
      if (rw != nullptr)
        rw->status = UVM_NOT_OK;
      return;
    }

    do_pre_write(rw);

    uvm_reg* rg = dynamic_cast<uvm_reg*>(rw->element);
    const unsigned int width = (rg != nullptr) ? rg->get_n_bits() : 0u;
    uvm_reg_data_t val = rw->value.empty() ? uvm_reg_data_t(0) : rw->value[0];
    if (width > 0u && width < 64u)
      val = uvm_reg_data_t(val.to_uint64() & detail::reg_width_mask(width));

    SignalValT out_val{};
    detail::reg_data_to_value(val, out_val);
    m_wr_if->write(out_val);
    rw->status = UVM_IS_OK;

    do_post_write(rw);
  }

 private:
  const sc_core::sc_signal_in_if<SignalValT>* m_rd_if;
  sc_core::sc_signal_write_if<SignalValT>* m_wr_if;
};

template <typename StorageT>
uvm_cpp_reg_backdoor<StorageT>* uvm_bind_cpp_backdoor(
    uvm_reg* rg,
    StorageT& storage,
    bool writable = true)
{
  if (rg == nullptr)
    return nullptr;
  auto* bkdr = new uvm_cpp_reg_backdoor<StorageT>(
      storage, writable, rg->get_n_bits(), rg->get_name() + "_cpp_bkdr");
  rg->set_backdoor(bkdr);
  return bkdr;
}

template <typename SignalValT>
uvm_sc_signal_reg_backdoor<SignalValT>* uvm_bind_signal_backdoor(
    uvm_reg* rg,
    sc_core::sc_signal<SignalValT, sc_core::SC_MANY_WRITERS>& sig)
{
  if (rg == nullptr)
    return nullptr;
  auto* bkdr = new uvm_sc_signal_reg_backdoor<SignalValT>(
      sig, &sig, rg->get_name() + "_sig_bkdr");
  rg->set_backdoor(bkdr);
  return bkdr;
}

template <typename SignalValT>
uvm_sc_signal_reg_backdoor<SignalValT>* uvm_bind_signal_backdoor(
    uvm_reg* rg,
    const sc_core::sc_signal_in_if<SignalValT>& rd_sig)
{
  if (rg == nullptr)
    return nullptr;
  auto* bkdr = new uvm_sc_signal_reg_backdoor<SignalValT>(
      rd_sig, nullptr, rg->get_name() + "_sig_ro_bkdr");
  rg->set_backdoor(bkdr);
  return bkdr;
}

} // namespace uvm

#endif // UVM_VERILATOR_REG_BACKDOOR_H_
