//----------------------------------------------------------------------
//   Copyright 2007-2010 Mentor Graphics Corporation
//   Copyright 2013-2021 NXP B.V.
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

//----------------------------------------------------------------------
// TITLE: UVM HDL Backdoor Access support routines.
//
// These routines provide an interface to the DPI/PLI
// implementation of backdoor access used by registers.
//----------------------------------------------------------------------

#ifndef UVM_HDL_H_
#define UVM_HDL_H_

#include <cstdint>
#include <sstream>
#include <type_traits>

#include <systemc>

#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/reg/uvm_sc_if.h"

namespace uvm {

#ifndef UVM_HDL_MAX_WIDTH
#define UVM_HDL_MAX_WIDTH 1024
#endif

typedef sc_dt::sc_uint_base uvm_hdl_data_t;

bool uvm_hdl_check_path(const std::string& path );
bool uvm_hdl_release(const std::string& path);
bool uvm_hdl_release_time( const std::string& path, sc_core::sc_time time );

namespace detail {

inline unsigned int hdl_bit_width(const bool&)
{
  return 1u;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value,
                        unsigned int>::type
hdl_bit_width(const T&)
{
  return static_cast<unsigned int>(sizeof(T) * 8u);
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, unsigned int>::type
hdl_bit_width(const T& v)
{
  return static_cast<unsigned int>(v.length());
}

inline bool hdl_bounds_valid(unsigned int width, int start, int stop)
{
  if (start == -1 && stop == -1)
    return true;
  if (start < 0 || static_cast<unsigned int>(start) >= width)
    return false;
  if (stop == -1)
    return true;
  if (stop < 0 || static_cast<unsigned int>(stop) >= width || start < stop)
    return false;
  return true;
}

inline bool hdl_slice_read(const bool& full, bool& out, int start, int stop)
{
  if (!hdl_bounds_valid(1u, start, stop))
    return false;
  out = full;
  return true;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value,
                        bool>::type
hdl_slice_read(const T& full, T& out, int start, int stop)
{
  const unsigned int width = hdl_bit_width(full);
  if (!hdl_bounds_valid(width, start, stop))
    return false;
  if (start == -1 && stop == -1)
  {
    out = full;
    return true;
  }
  const int lo = (stop == -1) ? start : stop;
  const unsigned int slice_w = static_cast<unsigned int>(start - lo + 1);
  using U = typename std::make_unsigned<T>::type;
  const uint64_t ufull = static_cast<uint64_t>(static_cast<U>(full));
  const uint64_t mask = (slice_w >= 64u) ? ~uint64_t(0) : ((uint64_t(1) << slice_w) - 1u);
  out = static_cast<T>(static_cast<U>((ufull >> lo) & mask));
  return true;
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, bool>::type
hdl_slice_read(const T& full, T& out, int start, int stop)
{
  const unsigned int width = hdl_bit_width(full);
  if (!hdl_bounds_valid(width, start, stop))
    return false;
  if (start == -1 && stop == -1)
  {
    out = full;
    return true;
  }
  if (stop == -1 || start == stop)
  {
    out = 0;
    out[0] = full[start];
    return true;
  }
  out = 0;
  out.range(start - stop, 0) = full.range(start, stop);
  return true;
}

inline bool hdl_slice_write(bool& cur, const bool& val, int start, int stop)
{
  if (!hdl_bounds_valid(1u, start, stop))
    return false;
  cur = val;
  return true;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value,
                        bool>::type
hdl_slice_write(T& cur, const T& val, int start, int stop)
{
  const unsigned int width = hdl_bit_width(cur);
  if (!hdl_bounds_valid(width, start, stop))
    return false;
  if (start == -1 && stop == -1)
  {
    cur = val;
    return true;
  }
  const int lo = (stop == -1) ? start : stop;
  const unsigned int slice_w = static_cast<unsigned int>(start - lo + 1);
  using U = typename std::make_unsigned<T>::type;
  const uint64_t ucur = static_cast<uint64_t>(static_cast<U>(cur));
  const uint64_t uval = static_cast<uint64_t>(static_cast<U>(val));
  const uint64_t mask = (slice_w >= 64u) ? ~uint64_t(0) : ((uint64_t(1) << slice_w) - 1u);
  const uint64_t cleared = ucur & ~(mask << lo);
  const uint64_t inserted = (uval & mask) << lo;
  cur = static_cast<T>(static_cast<U>(cleared | inserted));
  return true;
}

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, bool>::type
hdl_slice_write(T& cur, const T& val, int start, int stop)
{
  const unsigned int width = hdl_bit_width(cur);
  if (!hdl_bounds_valid(width, start, stop))
    return false;
  if (start == -1 && stop == -1)
  {
    cur = val;
    return true;
  }
  if (stop == -1 || start == stop)
  {
    cur[start] = val[0];
    return true;
  }
  cur.range(start, stop) = val.range(start - stop, 0);
  return true;
}

template <typename T>
sc_core::sc_signal<T, sc_core::SC_MANY_WRITERS>* hdl_resolve_deposit_sig(sc_core::sc_object* obj)
{
  auto* sig = dynamic_cast<sc_core::sc_signal<T, sc_core::SC_MANY_WRITERS>*>(obj);
  if (sig != nullptr)
    return sig;
  auto* port = dynamic_cast<sc_core::sc_port_base*>(obj);
  if (port != nullptr)
    return dynamic_cast<sc_core::sc_signal<T, sc_core::SC_MANY_WRITERS>*>(port->get_interface());
  return nullptr;
}

template <typename T>
const sc_core::sc_signal_in_if<T>* hdl_resolve_read_if(const sc_core::sc_object* obj)
{
  const auto* rd_if = dynamic_cast<const sc_core::sc_signal_in_if<T>*>(obj);
  if (rd_if != nullptr)
    return rd_if;
  const auto* port = dynamic_cast<const sc_core::sc_port_base*>(obj);
  if (port != nullptr)
    return dynamic_cast<const sc_core::sc_signal_in_if<T>*>(port->get_interface());
  return nullptr;
}

} // namespace detail

//----------------------------------------------------------------------
// Function: uvm_hdl_deposit
//
// Sets the given HDL ~path~ to the specified ~value~.
// Returns 1 if the call succeeded, 0 otherwise.
//----------------------------------------------------------------------

// Message for an object in the design which cannot be accessed with
// the type T used for the access
inline std::string uvm_hdl_type_mismatch( const std::string& path,
                                          const sc_core::sc_object* obj,
                                          unsigned int n_bits )
{
  std::ostringstream str;
  str << "Object " << path << " (" << obj->kind() << ") cannot be accessed: it is "
      << "not a uvm_sc_if<T> or an sc_signal<T, SC_MANY_WRITERS>, or an sc_vector of "
      << "them, with exactly the value type T of the access, a type of " << n_bits
      << " bits (register and memory backdoor accesses use uvm_reg_data_t).";
  return str.str();
}

template <typename T>
bool uvm_hdl_deposit( const std::string& path, const T& value )
{
  std::string objname1, objname2;
  int idx_start = -1;
  int idx_stop = -1;
  int vidx_start = -1;
  int vidx_stop = -1;

  uvm_extract_path_index(path, objname1, idx_start, idx_stop);
  // check again, perhaps we've hit a vector
  uvm_extract_path_index(objname1, objname2, vidx_start, vidx_stop);

  sc_core::sc_object* obj = sc_core::sc_find_object(objname2.c_str());

  if (obj == nullptr)
  {
    uvm_report_error("HDL_DEPOSIT", "Object " + objname2 + " not found in design hierarchy.");
    return false;
  }

  const unsigned int width = detail::hdl_bit_width(value);

  // possibility 1: backdoor register represented by sc_signal<T, SC_MANY_WRITERS>
  // or a port bound to one
  sc_core::sc_signal<T, sc_core::SC_MANY_WRITERS>* sig_md =
      detail::hdl_resolve_deposit_sig<T>(obj);
  if (sig_md != nullptr)
  {
    T next_val = sig_md->read();
    if (!detail::hdl_slice_write(next_val, value, idx_start, idx_stop))
    {
      uvm_report_error("HDL_DEPOSIT", "Specified index for bit select is out of range.");
      return false;
    }

    sig_md->write(next_val);

    std::ostringstream str;
    str << "Value 0x" << std::hex << value << " written to " << path << ".";
    uvm_report_info("HDL_DEPOSIT", str.str(), uvm::UVM_FULL);
    return true;
  }

  // possibility 2: backdoor register represented by object of type uvm_sc_if
  uvm_sc_if<T>* interf = dynamic_cast<uvm_sc_if<T>* > (obj);
  if (interf != nullptr)
  {
    if (!detail::hdl_bounds_valid(width, idx_start, idx_stop))
    {
      uvm_report_error("HDL_DEPOSIT", "Specified index for bit select is out of range.");
      return false;
    }

    interf->write(value, idx_start, idx_stop);
    std::ostringstream str;
    str << "Value 0x" << std::hex << value << " written to " << path << ".";
    uvm_report_info("HDL_DEPOSIT", str.str(), uvm::UVM_FULL);
    return true;
  }

  // possibility 3: sc_vector containing uvm_sc_if, sc_signal, or port
  sc_core::sc_vector_base* vec = dynamic_cast<sc_core::sc_vector_base* > (obj);
  if (vec != nullptr)
  {
    if (vidx_start == -1)
    {
      vidx_start = idx_start;
      idx_start = -1;
      idx_stop = -1;
    }

    std::vector<sc_core::sc_object*> vec_element = vec->get_elements();

    if ( (vidx_start > (((int)vec_element.size())-1)) || (vidx_start < 0) )
    {
      uvm_report_error("HDL_DEPOSIT", "Specified index of sc_vector out of range.");
      return false;
    }

    uvm_sc_if<T>* elem_if = dynamic_cast<uvm_sc_if<T>* > (vec_element[vidx_start]);
    if (elem_if != nullptr)
    {
      if (!detail::hdl_bounds_valid(width, idx_start, idx_stop))
      {
        uvm_report_error("HDL_DEPOSIT", "Specified index for bit select is out of range.");
        return false;
      }

      elem_if->write(value, idx_start, idx_stop);
      std::ostringstream str;
      str << "Value 0x" << std::hex << value << " written to " << path << ".";
      uvm_report_info("HDL_DEPOSIT", str.str(), uvm::UVM_FULL);
      return true;
    }

    sc_core::sc_signal<T, sc_core::SC_MANY_WRITERS>* elem_sig =
        detail::hdl_resolve_deposit_sig<T>(vec_element[vidx_start]);
    if (elem_sig != nullptr)
    {
      T next_val = elem_sig->read();
      if (!detail::hdl_slice_write(next_val, value, idx_start, idx_stop))
      {
        uvm_report_error("HDL_DEPOSIT", "Specified index for bit select is out of range.");
        return false;
      }

      elem_sig->write(next_val);
      std::ostringstream str;
      str << "Value 0x" << std::hex << value << " written to " << path << ".";
      uvm_report_info("HDL_DEPOSIT", str.str(), uvm::UVM_FULL);
      return true;
    }
  }

  uvm_report_error("HDL_DEPOSIT", uvm_hdl_type_mismatch(objname2, obj, width));
  return false;
}

//----------------------------------------------------------------------
// Function: uvm_hdl_force
//
// Forces the ~value~ on the given ~path~. Returns 1 if the call succeeded, 0 otherwise.
//----------------------------------------------------------------------

template <typename T>
bool uvm_hdl_force( const std::string&, const T& )
{
  uvm_report_error("HDL_FORCE", "HDL signal force not available without DPI.");
  return false;
}

//----------------------------------------------------------------------
// Function: uvm_hdl_force_time
//
// Forces the ~value~ on the given ~path~ for the specified amount of ~force_time~.
// If ~force_time~ is 0, <uvm_hdl_deposit> is called.
// Returns 1 if the call succeeded, 0 otherwise.
//----------------------------------------------------------------------

template <typename T>
bool uvm_hdl_force_time( const std::string&, const T&, sc_core::sc_time )
{
  uvm_report_error("HDL_FORCE_TIME", "HDL signal force with time not available without DPI.");
  return false;
}


//----------------------------------------------------------------------
// Function: uvm_hdl_release_and_read
//
// Releases a value previously set with <uvm_hdl_force>.
// Returns 1 if the call succeeded, 0 otherwise.
//----------------------------------------------------------------------

template <typename T>
bool uvm_hdl_release_and_read( const std::string&, T& )
{
  uvm_report_error("HDL_RELEASE_READ", "HDL signal release and read not available without DPI.");
  return false;
}


//----------------------------------------------------------------------
// Function: uvm_hdl_read()
//
// Gets the value at the given ~path~.
// Returns 1 if the call succeeded, 0 otherwise.
//----------------------------------------------------------------------

template <typename T>
bool uvm_hdl_read( const std::string& path, T& value )
{
  std::string objname1, objname2;
  int idx_start = -1;
  int idx_stop = -1;
  int vidx_start = -1;
  int vidx_stop = -1;

  uvm_extract_path_index(path, objname1, idx_start, idx_stop);
  // check again, perhaps we've hit a vector
  uvm_extract_path_index(objname1, objname2, vidx_start, vidx_stop);

  sc_core::sc_object* obj = sc_core::sc_find_object(objname2.c_str());

  if (obj == nullptr)
  {
    uvm_report_error("HDL_READ", "Object " + objname2 + " not found in design hierarchy.");
    return false;
  }

  const unsigned int width = detail::hdl_bit_width(value);

  // possibility 1: any sc_signal_in_if<T> (SC_ONE_WRITER or SC_MANY_WRITERS)
  // or sc_in<T>/sc_out<T>/sc_inout<T> port bound to one
  const sc_core::sc_signal_in_if<T>* sig_rd = detail::hdl_resolve_read_if<T>(obj);
  if (sig_rd != nullptr)
  {
    if (!detail::hdl_slice_read(sig_rd->read(), value, idx_start, idx_stop))
    {
      uvm_report_error("HDL_READ", "Specified index for bit select is out of range.");
      return false;
    }

    std::ostringstream str;
    str << "Value 0x" << std::hex << value << " read from " << path << ".";
    uvm_report_info("HDL_READ", str.str(), uvm::UVM_FULL);
    return true;
  }

  // possibility 2: backdoor register represented by object of type uvm_sc_if
  uvm_sc_if<T>* interf = dynamic_cast<uvm_sc_if<T>* > (obj);
  if (interf != nullptr)
  {
    if (!detail::hdl_bounds_valid(width, idx_start, idx_stop))
    {
      uvm_report_error("HDL_READ", "Specified index for bit select is out of range.");
      return false;
    }

    value = interf->read(idx_start, idx_stop);
    std::ostringstream str;
    str << "Value 0x" << std::hex << value << " read from " << path << ".";
    uvm_report_info("HDL_READ", str.str(), uvm::UVM_FULL);
    return true;
  }

  // possibility 3: sc_vector containing uvm_sc_if, sc_signal, or port
  sc_core::sc_vector_base* vec = dynamic_cast<sc_core::sc_vector_base* > (obj);
  if (vec != nullptr)
  {
    if (vidx_start == -1)
    {
      vidx_start = idx_start;
      idx_start = -1;
      idx_stop = -1;
    }

    std::vector<sc_core::sc_object*> vec_element = vec->get_elements();

    if ( (vidx_start > (((int)vec_element.size())-1)) || (vidx_start < 0) )
    {
      uvm_report_error("HDL_READ", "Specified index of sc_vector out of range.");
      return false;
    }

    uvm_sc_if<T>* elem_if = dynamic_cast<uvm_sc_if<T>* > (vec_element[vidx_start]);
    if (elem_if != nullptr)
    {
      if (!detail::hdl_bounds_valid(width, idx_start, idx_stop))
      {
        uvm_report_error("HDL_READ", "Specified index for bit select is out of range.");
        return false;
      }

      value = elem_if->read(idx_start, idx_stop);
      std::ostringstream str;
      str << "Value 0x" << std::hex << value << " read from " << path << ".";
      uvm_report_info("HDL_READ", str.str(), uvm::UVM_FULL);
      return true;
    }

    const sc_core::sc_signal_in_if<T>* elem_rd =
        detail::hdl_resolve_read_if<T>(vec_element[vidx_start]);
    if (elem_rd != nullptr)
    {
      if (!detail::hdl_slice_read(elem_rd->read(), value, idx_start, idx_stop))
      {
        uvm_report_error("HDL_READ", "Specified index for bit select is out of range.");
        return false;
      }

      std::ostringstream str;
      str << "Value 0x" << std::hex << value << " read from " << path << ".";
      uvm_report_info("HDL_READ", str.str(), uvm::UVM_FULL);
      return true;
    }
  }

  uvm_report_error("HDL_READ", uvm_hdl_type_mismatch(objname2, obj, width));
  return false;
}


} // namespace uvm

#endif // UVM_HDL_H_
