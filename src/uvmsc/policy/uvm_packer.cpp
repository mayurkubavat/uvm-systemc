//------------------------------------------------------------------------------
//   Copyright 2014-2015 NXP B.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc. 
//   Copyright 2010 Synopsys, Inc.
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
//------------------------------------------------------------------------------

#include <systemc>
#include <sstream>

#include "uvmsc/misc/uvm_status_container.h"
#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_globals.h"
#include "uvmsc/policy/uvm_packer.h"
#include "uvmsc/policy/uvm_field_op.h"
#include "uvmsc/misc/uvm_misc.h"
#include "uvmsc/macros/uvm_message_defines.h"

//using namespace sc_dt;

namespace uvm {

namespace {

class active_object_guard
{
 public:
  active_object_guard( uvm_policy* policy, uvm_object* obj )
  : m_policy(policy)
  {
    if (m_policy != nullptr)
      m_policy->push_active_object(obj);
  }

  ~active_object_guard()
  {
    if (m_policy != nullptr)
      m_policy->pop_active_object();
  }

 private:
  active_object_guard( const active_object_guard& );
  active_object_guard& operator=( const active_object_guard& );

  uvm_policy* m_policy;
};

class field_op_guard
{
 public:
  explicit field_op_guard( uvm_field_op* op )
  : m_op(op)
  {}

  ~field_op_guard()
  {
    if (m_op != nullptr)
      m_op->m_recycle();
  }

 private:
  field_op_guard( const field_op_guard& );
  field_op_guard& operator=( const field_op_guard& );

  uvm_field_op* m_op;
};

class cycle_check_guard
{
 public:
  explicit cycle_check_guard( const uvm_object* val )
  : m_val(val)
  {
    if (m_val != nullptr && uvm_object::__m_uvm_status_container != nullptr)
      uvm_object::__m_uvm_status_container->cycle_check[m_val] = true;
  }

  ~cycle_check_guard()
  {
    if (m_val != nullptr && uvm_object::__m_uvm_status_container != nullptr)
      uvm_object::__m_uvm_status_container->cycle_check.erase(m_val);
  }

 private:
  cycle_check_guard( const cycle_check_guard& );
  cycle_check_guard& operator=( const cycle_check_guard& );

  const uvm_object* m_val;
};

class packer_scope_guard
{
 public:
  packer_scope_guard( uvm_scope_stack* scope, const std::string& name )
  : m_scope(scope)
  {
    if (m_scope != nullptr)
      m_scope->down(name);
  }

  ~packer_scope_guard()
  {
    if (m_scope != nullptr)
      m_scope->up();
  }

 private:
  packer_scope_guard( const packer_scope_guard& );
  packer_scope_guard& operator=( const packer_scope_guard& );

  uvm_scope_stack* m_scope;
};

} // namespace

uvm_packer::uvm_packer( const std::string& name )
: uvm_policy(name),
  pack_index(0),
  unpack_index(0),
  reverse_order(false),
  byte_size(8),
  word_size(16),
  nopack(false),
  policy(UVM_DEFAULT_POLICY),
  m_bits(nullptr),
  m_size(0),
  m_max_size(0)
{
  physical = true;
  abstract = false;
  use_metadata = false;
  big_endian = false;
}

uvm_packer::uvm_packer( const std::string& name, no_inst_count_t )
: uvm_policy(name, no_inst_count_t()),
  pack_index(0),
  unpack_index(0),
  reverse_order(false),
  byte_size(8),
  word_size(16),
  nopack(false),
  policy(UVM_DEFAULT_POLICY),
  m_bits(nullptr),
  m_size(0),
  m_max_size(0)
{
  physical = true;
  abstract = false;
  use_metadata = false;
  big_endian = false;
}

uvm_packer::uvm_packer( const uvm_packer& rhs )
: uvm_policy(rhs),
  physical(rhs.physical),
  abstract(rhs.abstract),
  use_metadata(rhs.use_metadata),
  big_endian(rhs.big_endian),
  pack_index(rhs.pack_index),
  unpack_index(rhs.unpack_index),
  scope(rhs.scope),
  reverse_order(rhs.reverse_order),
  byte_size(rhs.byte_size),
  word_size(rhs.word_size),
  nopack(rhs.nopack),
  policy(rhs.policy),
  m_bits(nullptr),
  m_size(rhs.m_size),
  m_max_size(rhs.m_max_size)
{
  if (rhs.m_bits != nullptr && rhs.m_max_size > 0)
  {
    m_bits = new sc_dt::sc_bv_base(rhs.m_max_size);
    *m_bits = *rhs.m_bits;
  }
}

uvm_packer& uvm_packer::operator=( const uvm_packer& rhs )
{
  if (this == &rhs)
    return *this;

  uvm_policy::operator=(rhs);
  physical = rhs.physical;
  abstract = rhs.abstract;
  use_metadata = rhs.use_metadata;
  big_endian = rhs.big_endian;
  pack_index = rhs.pack_index;
  unpack_index = rhs.unpack_index;
  scope = rhs.scope;
  reverse_order = rhs.reverse_order;
  byte_size = rhs.byte_size;
  word_size = rhs.word_size;
  nopack = rhs.nopack;
  policy = rhs.policy;

  delete m_bits;
  m_bits = nullptr;
  m_size = rhs.m_size;
  m_max_size = rhs.m_max_size;
  if (rhs.m_bits != nullptr && rhs.m_max_size > 0)
  {
    m_bits = new sc_dt::sc_bv_base(rhs.m_max_size);
    *m_bits = *rhs.m_bits;
  }
  return *this;
}

uvm_packer::~uvm_packer()
{
  if (m_bits != nullptr)
  {
    delete m_bits;
    m_bits = nullptr;
  }
}

uvm_packer& uvm_packer::m_builtin_default()
{
  static uvm_packer builtin_default("uvm_default_packer", no_inst_count_t());
  return builtin_default;
}

uvm_packer* uvm_packer::get_default()
{
  if (uvm_default_packer == nullptr)
    uvm_default_packer = &m_builtin_default();
  return uvm_default_packer;
}

void uvm_packer::set_default( uvm_packer* packer )
{
  uvm_default_packer = (packer != nullptr) ? packer : &m_builtin_default();
}

void uvm_packer::flush()
{
  reset();
  scope.m_stack.clear();
  scope.m_arg.clear();
  uvm_policy::flush();
}

void uvm_packer::set_recursion_policy( uvm_recursion_policy_enum p )
{
  policy = p;
}

uvm_recursion_policy_enum uvm_packer::get_recursion_policy() const
{
  return policy;
}

//------------------------------------------------------------------------------
// Member function: pack_field (virtual)
//
//! Packs an integral value (less than or equal to 4096 bits) into the
//! packed array. Argument \p size is the number of bits of argument \p value
//! to pack.
//------------------------------------------------------------------------------

void uvm_packer::pack_field( const uvm_bitstream_t& value, int size )
{
  if (size <= 0)
    return;

  sc_dt::sc_bv<UVM_MAX_STREAMBITS> tmp = value;
  m_check_size(size);

  if(big_endian)
    for (int i = 0; i < size; i++)
      tmp[i] = value[size-1-i].to_bool();

  (*m_bits)(pack_index + size - 1, pack_index) = tmp.range(size-1, 0); // make sure we only copy the size
  pack_index += size;

  std::ostringstream str;
  str << "uvm_packer::pack_field 0x" << tmp.range(size-1, 0).to_string(sc_dt::SC_HEX_US, false)
      << ", length = " << size << std::endl;
  str << "pack_index = " << pack_index << std::endl;
  UVM_INFO("PCKFLD", str.str(), uvm::UVM_DEBUG);
}

//------------------------------------------------------------------------------
// member function: pack_field_int (virtual)
//
//! Packs the integral value (less than or equal to 64 bits) into the
//! pack array.  The argument \p size is the number of bits to pack, usually obtained by
//! $bits. This optimized version of #pack_field is useful for sizes up
//! to 64 bits.
//------------------------------------------------------------------------------

void uvm_packer::pack_field_int( const uvm_integral_t& value, int size )
{
  if (size <= 0)
    return;

  sc_dt::sc_bv<64> tmp = value;
  m_check_size(size);

  if(big_endian)
    for (int i = 0; i < size; i++)
      tmp[i] = value[size-1-i].to_bool();

  (*m_bits)(pack_index + size - 1, pack_index) = tmp.range(size-1, 0); // make sure we only copy the size
  pack_index += size;

  std::ostringstream str;
  str << "uvm_packer::pack_field_int 0x" << std::hex << tmp.to_uint64() << ", length = " << size << std::endl;
  str << "pack_index = " << pack_index << std::endl;
  UVM_INFO("PCKFLDINT", str.str(), uvm::UVM_DEBUG);
}


//------------------------------------------------------------------------------
// Member function: pack_string
//
//! Packs a string value into the pack array.
//!
//! When the metadata flag is set, the packed string is terminated by a null
//! character to mark the end of the string.
//! This is useful for mixed language communication where unpacking may occur
//! outside of SystemVerilog UVM.
//------------------------------------------------------------------------------

void uvm_packer::pack_string( const std::string& value )
{
  int nchars = (int)value.length();

  for (int i = 0; i < nchars; i++)
  {
    pack_char(value[i]);
  }

  if(use_metadata)
    pack_char(0); // nullptr character

  std::ostringstream str;
  str << "uvm_packer::pack_string '" << value << "'" << std::endl;
  str << "pack_index = " << pack_index << std::endl;
  if(use_metadata) str << "use_metadata = true" << std::endl;
  UVM_INFO("PCKSTR", str.str(), uvm::UVM_DEBUG);
}

//------------------------------------------------------------------------------
// Member function: pack_time (virtual)
//
//! Packs a time value as 64 bits into the pack array.
//------------------------------------------------------------------------------

void uvm_packer::pack_time( const sc_core::sc_time& value )
{
  double v = value.to_seconds();
  pack_field_int(uvm_realtobits(v), 64);

  std::ostringstream str;
  str << "uvm_packer::pack_time" << v << std::endl;
  str << "pack_index = " << pack_index << std::endl;
  UVM_INFO("PCKTIME", str.str(), uvm::UVM_DEBUG);
}


//------------------------------------------------------------------------------
// Member function: pack_real (virtual)
//
//! Packs a real \p value as 64 bits into the pack array.
//!
//! The real \p value is converted to a 6-bit scalar value using the function
//! uvm_realtobits before it is packed into the array.
//------------------------------------------------------------------------------

void uvm_packer::pack_real( double value )
{
  pack_field_int(uvm_realtobits(value), 64);
}


//------------------------------------------------------------------------------
// Member function: pack_object (virtual)
//
//! Packs an object value into the pack array.
//
//! A 4-bit header is inserted ahead of the string to indicate the number of
//! bits that was packed. If a null object was packed, then this header will
//! be 0.
//!
//! This is useful for mixed-language communication where unpacking may occur
//! outside of SystemVerilog UVM.
//------------------------------------------------------------------------------

void uvm_packer::m_pack_object( const uvm_object* val, bool with_meta )
{
  if (val != nullptr &&
      (is_active_object(val) ||
       val->__m_uvm_status_container->cycle_check.find(val) !=
         val->__m_uvm_status_container->cycle_check.end()))
  {
    std::ostringstream str;
    str << "Cycle detected for object "
        << val->get_inst_id()
        << " during pack";
    UVM_WARNING("CYCFND", str.str());
    return;
  }

  if (val == nullptr || policy == UVM_REFERENCE)
  {
    if (with_meta)
    {
      m_check_size(4);
      m_bits->range(pack_index + 3, pack_index) = 0;
      pack_index += 4;
    }
    return;
  }

  if (with_meta)
  {
    m_check_size(4);
    m_bits->range(pack_index + 3, pack_index) = 1;
    pack_index += 4;
  }

  cycle_check_guard cyc_guard(val);
  active_object_guard act_guard(this, const_cast<uvm_object*>(val));
  packer_scope_guard sc_guard(&scope, val->get_name());
  uvm_field_op* op = uvm_field_op::m_get_available_op();
  field_op_guard op_guard(op);

  op->set(UVM_PACK, this, val);
  const_cast<uvm_object*>(val)->do_execute_op(op);
  if (op->user_hook_enabled())
    val->do_pack(*this);
}

void uvm_packer::pack_object( const uvm_object& value )
{
  m_pack_object(&value, use_metadata);
}

void uvm_packer::pack_object( const uvm_object* value )
{
  m_pack_object(value, use_metadata);
}

void uvm_packer::pack_object_with_meta( const uvm_object* value )
{
  m_pack_object(value, true);
}

void uvm_packer::pack_object_with_meta( const uvm_object& value )
{
  m_pack_object(&value, true);
}

//------------------------------------------------------------------------------
// Member function: is_null
//
//! This member function is used during unpack operations to peek at the next 4-bit
//! chunk of the pack data and determine if it is 0.
//!
//! If the next four bits are all 0, then the return value is a 1; otherwise
//! it is 0.
//!
//! This is useful when unpacking objects, to decide whether a new object
//! needs to be allocated or not.
//------------------------------------------------------------------------------

bool uvm_packer::is_null()
{
  if (!m_enough_bits(4, "object metadata") || m_bits == nullptr)
    return true;
  return (m_bits->range(unpack_index + 3, unpack_index) == 0);
}


//------------------------------------------------------------------------------
// Member function: unpack_field_int (virtual)
//
//! Unpacks bits from the pack array and returns the bit-stream that was
//! unpacked.
//! Argument \p size is the number of bits to unpack; the maximum is 64 bits.
//! This is a more efficient variant than #unpack_field when unpacking into
//! smaller vectors.
//------------------------------------------------------------------------------

uvm_integral_t uvm_packer::unpack_field_int( int size )
{
  if (size <= 0)
    return 0;

  sc_dt::sc_bv<64> a, tmp;

  if (m_enough_bits(size, "integral"))
  {
    a = (*m_bits)(unpack_index + size - 1, unpack_index);

    inc_unpack_index(size);
    if(big_endian)
    {
      for (int i = 0; i < size; i++)
        tmp[i] = a[size-1-i];
      a = tmp; // content swapped
    }

    std::ostringstream str;
    str << "uvm_packer::unpack_field_int 0x" << std::hex << a.to_uint64() << ", length = " << size << std::endl;
    str << "unpack_index = " << unpack_index << std::endl;
    UVM_INFO("UNPCKFLDINT", str.str(), uvm::UVM_DEBUG);

    return a;
  }
  else
  {
    UVM_ERROR("UNPCKERR","Not enough bits in packed structure.");
    return 0;
  }
}


//------------------------------------------------------------------------------
// Member function: unpack_field (virtual)
//
//! Unpacks bits from the pack array and returns the bit-stream that was
//! unpacked. \p size is the number of bits to unpack; the maximum is 4096 bits.
//------------------------------------------------------------------------------

uvm_bitstream_t uvm_packer::unpack_field( int size )
{
  if (size <= 0)
    return 0;

  sc_dt::sc_bv<UVM_MAX_STREAMBITS> a, tmp;

  if (m_enough_bits(size,"integral"))
  {
    a.range(size-1, 0) = (*m_bits)(unpack_index + size - 1, unpack_index);
    inc_unpack_index(size);
    if(big_endian)
    {
      for (int i = 0; i < size; i++)
        tmp[i] = a[size-1-i];
      a = tmp; // content swapped
    }

    std::ostringstream str;
    str << "uvm_packer::unpack_field 0x" << a.range(size-1, 0).to_string(sc_dt::SC_HEX_US, false)
        << ", length = " << size << std::endl;
    str << "unpack_index = " << unpack_index << std::endl;
    UVM_INFO("UNPCKFLD", str.str(), uvm::UVM_DEBUG);

    return a;
  }
  else
  {
    UVM_ERROR("UNPCKERR","Not enough bits in packed structure.");
    return 0;
  }
}

//------------------------------------------------------------------------------
// Member function: unpack_string (virtual)
//
//! Unpacks a string.
//!
//! num_chars bytes are unpacked into a string. If num_chars is -1 then
//! unpacking stops on at the first null character that is encountered.
//------------------------------------------------------------------------------

// If num_chars is not -1, then the user only wants to unpack a
// specific number of bytes into the string.

std::string uvm_packer::unpack_string( int num_chars )
{
  sc_dt::sc_bv<8> b;
  bool is_null_term; // Assumes a NULL terminated string
  int i = 0;

  std::string s;

  if(num_chars == -1)
    is_null_term = true;
  else
    is_null_term = false;

  bool enough = true;
  while( (is_null_term || (i < num_chars)) &&
         (enough = m_enough_bits(8, "string")) &&
         m_bits != nullptr &&
        (( (*m_bits).range(unpack_index+7, unpack_index) != 0 ) || (is_null_term == false)) )
  {
    s += " ";
    if(!big_endian)
      s[i] = (char)(*m_bits)(unpack_index+7, unpack_index).to_int();
    else
    {
      for(int j = 0; j < 8; ++j)
        b[7-j] = (*m_bits)[unpack_index+j].to_bool();
      s[i] = b.to_int();
    }

    inc_unpack_index(8);

    std::ostringstream str;
    str << "uvm_packer::unpack_char -> " << s[i] << std::endl;
    str << "unpack_index = " << unpack_index << std::endl;
    UVM_INFO("UNPCKSTR", str.str(), uvm::UVM_DEBUG);

    ++i;
  }

  if(use_metadata && enough) // metadata adds null string at the end
  {
    if(m_enough_bits(8, "string"))
      inc_unpack_index(8);
  }

  std::ostringstream str;
  str << "uvm_packer::unpack_string '" << s << "'" << std::endl;
  str << "unpack_index = " << unpack_index << std::endl;
  UVM_INFO("UNPCKSTR", str.str(), uvm::UVM_DEBUG);

  return s;
}


//------------------------------------------------------------------------------
// Member function: unpack_time (virtual)
//
//! Unpacks the next 64 bits of the pack array and places them into a
//! time variable.
//! In case the time cannot be unpacked, return SC_ZERO_TIME
//------------------------------------------------------------------------------

sc_core::sc_time uvm_packer::unpack_time()
{
  sc_core::sc_time sct = sc_core::SC_ZERO_TIME;

  if (m_enough_bits(64, "time"))
  {
    double t = unpack_real();
    sct = sc_core::sc_time(t, sc_core::SC_SEC);
  }
  else
    uvm_report_error("UNPCKERR","Not able to unpack time.", UVM_NONE);

  std::ostringstream str;
  str << "uvm_packer::unpack_time" << sct << std::endl;
  str << "pack_index = " << pack_index << std::endl;
  UVM_INFO("UNPCKTIME", str.str(), uvm::UVM_DEBUG);

  return sct;
}


//------------------------------------------------------------------------------
// Member function: unpack_real (virtual)
//
//! Unpacks the next 64 bits of the pack array and places them into a
//! real variable.
//!
//! The 64 bits of packed data are converted to a real using the $bits2real
//! system function.
//------------------------------------------------------------------------------

double uvm_packer::unpack_real()
{
  if (m_enough_bits(64,"real"))
    return uvm_bitstoreal(unpack_field_int(64));

  UVM_ERROR("UNPCKERR","Not able to unpack real value.");
  return 0.0;
}


//------------------------------------------------------------------------------
// Member function: unpack_object (virtual)
//
//! Unpacks an object and stores the result into \p value.
//!
//! \p value must be an allocated object that has enough space for the data
//! being unpacked. The first four bits of packed data are used to determine
//! if a null object was packed into the array.
//!
//! The #is_null function can be used to peek at the next four bits in
//! the pack array before calling this method.
//------------------------------------------------------------------------------

void uvm_packer::m_unpack_object( uvm_object*& val, bool with_meta, bool allow_nullify )
{
  if (val != nullptr &&
      (is_active_object(val) ||
       val->__m_uvm_status_container->cycle_check.find(val) !=
         val->__m_uvm_status_container->cycle_check.end()))
  {
    std::ostringstream str;
    str << "Cycle detected for object "
        << val->get_inst_id()
        << " during unpack.";
    UVM_WARNING("CYCFND", str.str());
    return;
  }

  int is_non_null = 1;
  if (with_meta)
  {
    if (!m_enough_bits(4, "object metadata") || m_bits == nullptr)
      return;
    is_non_null = (*m_bits).range(unpack_index + 3, unpack_index).to_int();
    inc_unpack_index(4);
  }

  if (is_non_null == 0)
  {
    if (allow_nullify)
      val = nullptr;
    return;
  }

  if (val == nullptr)
  {
    UVM_ERROR("UNPCKERR", "Can not unpack into nullptr object.");
    return;
  }

  cycle_check_guard cyc_guard(val);
  active_object_guard act_guard(this, val);
  packer_scope_guard sc_guard(&scope, val->get_name());
  uvm_field_op* op = uvm_field_op::m_get_available_op();
  field_op_guard op_guard(op);

  op->set(UVM_UNPACK, this, val);
  val->do_execute_op(op);
  if (op->user_hook_enabled())
    val->do_unpack(*this);
}

void uvm_packer::unpack_object( uvm_object& value )
{
  uvm_object* val = &value;
  m_unpack_object(val, use_metadata, false);
}

void uvm_packer::unpack_object( uvm_object*& value )
{
  m_unpack_object(value, use_metadata, true);
}

void uvm_packer::unpack_object_with_meta( uvm_object*& value )
{
  m_unpack_object(value, true, true);
}

void uvm_packer::unpack_object_with_meta( uvm_object& value )
{
  uvm_object* val = &value;
  m_unpack_object(val, true, false);
}


//------------------------------------------------------------------------------
// Member function: get_packed_size (virtual)
//
//! Returns the number of bits that were packed.
//------------------------------------------------------------------------------

unsigned int uvm_packer::get_packed_size() const
{
  return pack_index;
}


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
///      Implementation-defined member functions start here           //
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

uvm_packer& uvm_packer::operator<< ( bool a )
{
  pack_bool(a);
  return *this;
}

uvm_packer& uvm_packer::operator>> ( bool& a )
{
  unpack_bool(a);
  return *this;
}

uvm_packer& uvm_packer::operator<< ( double a )
{
  pack_real(a);
  return *this;
}

uvm_packer& uvm_packer::operator>> ( double& a )
{
  a = unpack_real();
  return *this;
}

uvm_packer& uvm_packer::operator<< ( float a )
{
  pack_real(a);
  return *this;
}

uvm_packer& uvm_packer::operator>> ( float& a )
{
  a = unpack_real();
  return *this;
}

uvm_packer& uvm_packer::operator<< ( const sc_core::sc_time& a )
{
  pack_time(a);
  return *this;
}

uvm_packer& uvm_packer::operator>> ( sc_core::sc_time& a )
{
  a = unpack_time();
  return *this;
}

#define UVM_PACK_UNPACK_OPERATORS(T) \
  uvm_packer& uvm_packer::operator<< ( T a ) \
  { \
    sc_dt::sc_bv<64> a_ = a; \
    pack_field_int(a_, 8*sizeof(T));\
    return *this;\
  } \
  uvm_packer& uvm_packer::operator>> ( T& a ) \
  { \
    a = (T)(unpack_field_int(8*sizeof(T))); \
    return *this; \
  }

UVM_PACK_UNPACK_OPERATORS( char )
UVM_PACK_UNPACK_OPERATORS( unsigned char )
UVM_PACK_UNPACK_OPERATORS( short )
UVM_PACK_UNPACK_OPERATORS( unsigned short  )
UVM_PACK_UNPACK_OPERATORS( int )
UVM_PACK_UNPACK_OPERATORS( unsigned int  )
UVM_PACK_UNPACK_OPERATORS( long )
UVM_PACK_UNPACK_OPERATORS( unsigned long )
UVM_PACK_UNPACK_OPERATORS( long long )
UVM_PACK_UNPACK_OPERATORS( unsigned long long )

uvm_packer& uvm_packer::operator<< ( const char* a )
{
  if (!a ) {
    pack_string("");
  } else {
    pack_string(std::string(a));
  }
  return *this;
}

uvm_packer& uvm_packer::operator<< ( const std::string& a )
{
  pack_string(a);
  return *this;
}

uvm_packer& uvm_packer::operator>> ( std::string& a )
{
  a = unpack_string();
  return *this;
}

uvm_packer& uvm_packer::operator<< ( const uvm_object& a )
{
  pack_object(a);
  return *this;
}

uvm_packer& uvm_packer::operator>> ( uvm_object& a )
{
  unpack_object(a);
  return *this;
}


#define UVM_PACK_UNPACK_SC_OPERATORS(T1, T2) \
  uvm_packer& uvm_packer::operator<< ( const T1& a ) \
  { \
    pack_##T2(a);\
    return *this;\
  } \
  uvm_packer& uvm_packer::operator>> ( T1& a ) \
  {  \
    unpack_##T2(a); \
    return *this; \
  }

UVM_PACK_UNPACK_SC_OPERATORS( sc_dt::sc_logic, sc_logic )
UVM_PACK_UNPACK_SC_OPERATORS( sc_dt::sc_bv_base, sc_bv_base )
UVM_PACK_UNPACK_SC_OPERATORS( sc_dt::sc_lv_base, sc_lv_base )
UVM_PACK_UNPACK_SC_OPERATORS( sc_dt::sc_int_base, sc_int_base )
UVM_PACK_UNPACK_SC_OPERATORS( sc_dt::sc_uint_base, sc_uint_base )
UVM_PACK_UNPACK_SC_OPERATORS( sc_dt::sc_signed, sc_signed )
UVM_PACK_UNPACK_SC_OPERATORS( sc_dt::sc_unsigned, sc_unsigned )


//------------------------------------------------------------------------------
// member function: get_bits
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::get_packed_bits( std::vector<bool>& stream ) const
{
  stream.clear();
  if (pack_index == 0 || m_bits == nullptr)
    return;
  stream.reserve(pack_index);
  for (unsigned int i = 0; i < pack_index; ++i)
    stream.push_back((*m_bits)[i].to_bool());
}

void uvm_packer::get_packed_bytes( std::vector<unsigned char>& stream ) const
{
  stream.clear();
  if (pack_index == 0 || m_bits == nullptr)
    return;

  const unsigned int nbytes = 1u + (pack_index - 1u) / 8u;
  stream.reserve(nbytes);
  for (unsigned int i = 0; i < nbytes; ++i)
  {
    sc_dt::sc_bv<8> bv(0);
    const unsigned int base = 8u * i;
    for (unsigned int j = 0; j < 8u && (base + j) < pack_index; ++j)
      bv[j] = (*m_bits)[base + j].to_bool();

    if (big_endian)
    {
      sc_dt::sc_bv<8> tmp = bv;
      for (int j = 0; j < 8; ++j)
        bv[j] = tmp[7 - j];
    }
    stream.push_back(static_cast<unsigned char>(bv.to_uint()));
  }
}

void uvm_packer::get_packed_ints( std::vector<unsigned int>& stream ) const
{
  stream.clear();
  if (pack_index == 0 || m_bits == nullptr)
    return;

  const unsigned int nints = 1u + (pack_index - 1u) / 32u;
  stream.reserve(nints);
  for (unsigned int i = 0; i < nints; ++i)
  {
    sc_dt::sc_bv<32> bv(0);
    const unsigned int base = 32u * i;
    for (unsigned int j = 0; j < 32u && (base + j) < pack_index; ++j)
      bv[j] = (*m_bits)[base + j].to_bool();

    if (big_endian)
    {
      sc_dt::sc_bv<32> tmp = bv;
      for (int j = 0; j < 32; ++j)
        bv[j] = tmp[31 - j];
    }
    stream.push_back(bv.to_uint());
  }
}

void uvm_packer::get_packed_longints( std::vector<std::uint64_t>& stream ) const
{
  stream.clear();
  if (pack_index == 0 || m_bits == nullptr)
    return;

  const unsigned int nlongints = 1u + (pack_index - 1u) / 64u;
  stream.reserve(nlongints);
  for (unsigned int i = 0; i < nlongints; ++i)
  {
    sc_dt::sc_bv<64> bv(0);
    const unsigned int base = 64u * i;
    for (unsigned int j = 0; j < 64u && (base + j) < pack_index; ++j)
      bv[j] = (*m_bits)[base + j].to_bool();

    if (big_endian)
    {
      sc_dt::sc_bv<64> tmp = bv;
      for (int j = 0; j < 64; ++j)
        bv[j] = tmp[63 - j];
    }
    stream.push_back(bv.to_uint64());
  }
}

void uvm_packer::set_packed_bits( const std::vector<bool>& stream )
{
  flush();
  const int bit_size = static_cast<int>(stream.size());
  if (bit_size == 0)
    return;

  m_set_size(bit_size);
  for (int i = 0; i < bit_size; ++i)
    (*m_bits)[i] = stream[i];

  pack_index = static_cast<unsigned int>(bit_size);
}

void uvm_packer::set_packed_bytes( const std::vector<unsigned char>& stream )
{
  flush();
  const int byte_count = static_cast<int>(stream.size());
  if (byte_count == 0)
    return;

  m_set_size(byte_count * 8);
  for (int i = 0; i < byte_count; ++i)
  {
    sc_dt::sc_bv<8> b = stream[i];
    if (big_endian)
    {
      sc_dt::sc_bv<8> tmp = b;
      for (int j = 0; j < 8; ++j)
        b[j] = tmp[7 - j];
    }
    (*m_bits).range(8 * i + 7, 8 * i) = b;
  }
  pack_index = static_cast<unsigned int>(byte_count * 8);
}

void uvm_packer::set_packed_ints( const std::vector<unsigned int>& stream )
{
  flush();
  const int int_count = static_cast<int>(stream.size());
  if (int_count == 0)
    return;

  m_set_size(int_count * 32);
  for (int i = 0; i < int_count; ++i)
  {
    sc_dt::sc_bv<32> v = stream[i];
    if (big_endian)
    {
      sc_dt::sc_bv<32> tmp = v;
      for (int j = 0; j < 32; ++j)
        v[j] = tmp[31 - j];
    }
    (*m_bits).range(32 * i + 31, 32 * i) = v;
  }
  pack_index = static_cast<unsigned int>(int_count * 32);
}

void uvm_packer::set_packed_longints( const std::vector<std::uint64_t>& stream )
{
  flush();
  const int longint_count = static_cast<int>(stream.size());
  if (longint_count == 0)
    return;

  m_set_size(longint_count * 64);
  for (int i = 0; i < longint_count; ++i)
  {
    sc_dt::sc_bv<64> v = static_cast<sc_dt::uint64>(stream[i]);
    if (big_endian)
    {
      sc_dt::sc_bv<64> tmp = v;
      for (int j = 0; j < 64; ++j)
        v[j] = tmp[63 - j];
    }
    (*m_bits).range(64 * i + 63, 64 * i) = v;
  }
  pack_index = static_cast<unsigned int>(longint_count * 64);
}

//------------------------------------------------------------------------------
// member function: get_bits
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::get_bits( std::vector<bool>& bits ) const
{
  unsigned int n = get_remaining_unpacked_bits();
  if (n == 0 || m_bits == nullptr)
    return;
  for (unsigned int i = 0; i < n; i++)
  {
    bool b = (*m_bits)[i].to_bool();
    bits.push_back(b);
  }
}

//------------------------------------------------------------------------------
// member function: get_bytes
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::get_bytes( std::vector<unsigned char>& bytes ) const
{
  unsigned int n = get_remaining_unpacked_bits();
  if (n == 0 || m_bits == nullptr)
    return;
  unsigned int nbytes = 1 + (n - 1) / 8;

  for (unsigned int i = 0; i < nbytes; i++)
  {
    sc_dt::sc_bv<8> tmp(0);
    unsigned int base = 8 * i;
    for (unsigned int j = 0; j < 8 && (base + j) < pack_index; ++j)
      tmp[j] = (*m_bits)[base + j].to_bool();

    if (big_endian)
    {
      sc_dt::sc_bv<8> tmp2;
      for (int j = 0; j < 8; ++j)
        tmp2[j] = tmp[7 - j];
      tmp = tmp2;
    }

    bytes.push_back(static_cast<unsigned char>(tmp.to_uint()));
  }
}

//------------------------------------------------------------------------------
// member function: get_ints
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::get_ints( std::vector<unsigned int>& ints ) const
{
  unsigned int n = get_remaining_unpacked_bits();
  if (n == 0 || m_bits == nullptr)
    return;
  unsigned int nints = 1 + (n - 1) / 32;

  for (unsigned int i = 0; i < nints; i++)
  {
    sc_dt::sc_bv<32> tmp(0);
    unsigned int base = 32 * i;
    for (unsigned int j = 0; j < 32 && (base + j) < pack_index; ++j)
      tmp[j] = (*m_bits)[base + j].to_bool();

    if (big_endian)
    {
      sc_dt::sc_bv<32> tmp2;
      for (int j = 0; j < 32; ++j)
        tmp2[j] = tmp[31 - j];
      tmp = tmp2;
    }

    ints.push_back(tmp.to_uint());
  }
}

//------------------------------------------------------------------------------
// member function: put_bits
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::put_bits( const std::vector<bool>& bitstream )
{
  reset();
  int bit_size = (int)bitstream.size();
  if (bit_size == 0)
    return;
  m_set_size(bit_size);

  for (int i = 0; i < bit_size; i++)
  {
    (*m_bits)[i] = bitstream[i];
  }

  pack_index = bit_size;
}

//------------------------------------------------------------------------------
// member function: put_bytes
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::put_bytes( const std::vector<unsigned char>& bytestream )
{
  reset();
  int byte_size = (int)bytestream.size();
  if (byte_size == 0)
    return;
  m_set_size(byte_size * 8);

  for (int i = 0; i < byte_size; i++)
  {
    sc_dt::sc_bv<8> b = bytestream[i];
    if (big_endian)
    {
      sc_dt::sc_bv<8> tmp = b;
      for (int j = 0; j < 8; ++j)
        b[j] = tmp[7 - j]; // swap char first
    }
    (*m_bits).range(8 * i + 7, 8 * i) = b;
  }
  pack_index = 8 * byte_size;
}

//------------------------------------------------------------------------------
// member function: put_ints
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::put_ints( const std::vector<unsigned int>& intstream )
{
  reset();
  int int_size = (int)intstream.size();
  if (int_size == 0)
    return;
  m_set_size(int_size * 32);

  for (int i = 0; i < int_size; i++)
  {
    sc_dt::sc_bv<32> v = intstream[i];
    if (big_endian)
    {
      sc_dt::sc_bv<32> tmp = v;
      for (int j = 0; j < 32; ++j)
        v[j] = tmp[31 - j]; // swap int first
    }

    (*m_bits).range(32 * i + 31, 32 * i) = v;
  }
  pack_index = 32 * int_size;
}


//------------------------------------------------------------------------------
// Member function: reset
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::reset()
{
  pack_index = 0;
  unpack_index = 0;
  m_size = 0;
}


//------------------------------------------------------------------------------
// Member function: enough_bits
//
//! Implementation defined
//------------------------------------------------------------------------------

bool uvm_packer::m_enough_bits( int needed, std::string id ) const
{
  if (needed <= 0)
    return true;
  const unsigned int avail = (m_bits != nullptr && pack_index >= unpack_index)
    ? (pack_index - unpack_index)
    : 0u;
  if (m_bits == nullptr || pack_index < unpack_index || avail < static_cast<unsigned int>(needed))
  {
   std::ostringstream str;
   str << needed
       << " bits needed to unpack "
       << id
       << ", yet only "
       << avail
       << " available.";
    UVM_ERROR("PCKSZERR", str.str());
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------
// Member function: m_set_size
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::m_set_size( int nbits )
{
  // check that we are in a "fresh" packer, e.g. one that has been reset
#ifdef _NCSC_DEBUG
  assert(size == 0);
  assert(pack_index == 0);
#endif
  // no need to copy over from old sc_bv_base
  m_allocate(nbits, false);
}


//------------------------------------------------------------------------------
// Member function: m_allocate
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::m_allocate( int nbits, bool copy_ )
{
  // allocate new sc_bv_base with new size, and copy over contents from
  // old sc_bv_base

  int total_bits = pack_index + nbits;
  int new_size =
      ((total_bits - 1) / UVM_PACKING_BLOCK_SIZE + 1) * UVM_PACKING_BLOCK_SIZE;
#ifdef _NCSC_DEBUG
  assert((new_size % UVM_PACKING_BLOCK_SIZE) == 0);
#endif
  m_size = new_size;

  if (m_size > m_max_size)
  { // need to allocate
    m_max_size = m_size;
    sc_dt::sc_bv_base* new_bv = new sc_dt::sc_bv_base(m_size);
    if (m_bits)
    {
      if (copy_)
      {
        *new_bv = *m_bits;
      }
      delete m_bits;
    }
    m_bits = new_bv;
  }
}


//------------------------------------------------------------------------------
// Member function: get_remaining_unpacked_bits
//
//! Implementation defined
//------------------------------------------------------------------------------

unsigned int uvm_packer::get_remaining_unpacked_bits() const
{
  if (m_bits == nullptr || pack_index < unpack_index)
    return 0u;
  return pack_index - unpack_index;
}


//------------------------------------------------------------------------------
// Member function: m_check_size
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::m_check_size( int nbits )
{
  int capacity = m_size - pack_index;
  if (nbits >= capacity)
  {
    m_allocate(nbits);
  }
}

//------------------------------------------------------------------------------
// Member function: inc_unpack_index
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::inc_unpack_index( int n )
{
  unpack_index += n;
  if (unpack_index > pack_index)
  {
    std::ostringstream msg;
    msg << "unpack_index > pack_index. "
        << "unpack_index = " << unpack_index
        << " pack_index = " << pack_index;
    UVM_ERROR("UNPCKERR", msg.str());
  }
}

//------------------------------------------------------------------------------
// Member function: pack_char
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::pack_char( char a )
{
  int nbits = 8;
  m_check_size(nbits);
  sc_dt::sc_bv<8> b, c;
  b = a;

  if (big_endian)
  {
    for (int i = 0; i < 8; i++)
      c[i] = b[7-i];
  }
  else
    c = b;

  (*m_bits)(pack_index + nbits - 1, pack_index) = c;
  pack_index += nbits;

  std::string s;
  if (c.to_int())
    s = (char)c.to_int();
  else
    s = "\0";

  std::ostringstream str;
  str << "uvm_packer::pack_char -> " << s << std::endl;
  str << "pack_index = " << pack_index << std::endl;
  UVM_INFO("PCKCHR", str.str(), uvm::UVM_DEBUG);
}


//------------------------------------------------------------------------------
// Member function: unpack_char
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::unpack_char( char& a )
{
  unsigned nbits = 8;
  if (!m_enough_bits(static_cast<int>(nbits), "char") || m_bits == nullptr)
  {
    a = 0;
    return;
  }
  sc_dt::sc_bv<8> b, c;
  b = (*m_bits)(unpack_index + nbits - 1, unpack_index);

  inc_unpack_index(nbits);

  if (big_endian)
  {
    for (int i = 0; i < 8; i++)
      c[i] = b[7-i];
  }
  else c = b;

  a = c.to_int();

  std::ostringstream str;
  str << "uvm_packer::unpack_char " << c.to_int() << std::endl;
  str << "unpack_index = " << unpack_index << std::endl;
  UVM_INFO("UNPCKCHR", str.str(), uvm::UVM_DEBUG);
}


void uvm_packer::pack_bool( bool a )
{
  m_check_size(1);

  (*m_bits)[pack_index] = a ? 1 : 0;
  pack_index += 1;

  std::ostringstream str;
  str << "uvm_packer::pack_bool: " << (a ? 1 : 0) << std::endl;
  str << "pack_index = " << pack_index << std::endl;
  UVM_INFO("PCKBOOL", str.str(), uvm::UVM_DEBUG);
}



void uvm_packer::unpack_bool( bool& a )
{
  if (!m_enough_bits(1, "bool") || m_bits == nullptr)
  {
    a = false;
    return;
  }
  a = (*m_bits)[unpack_index] == 1;

  inc_unpack_index(1);

  std::ostringstream str;
  str << "uvm_packer::unpack_char " << (a ? 1 : 0) << std::endl;
  str << "unpack_index = " << unpack_index << std::endl;
  UVM_INFO("UNPCKBOOL", str.str(), uvm::UVM_DEBUG);

}

void uvm_packer::pack_sc_logic( const sc_dt::sc_logic& a )
{
  m_check_size(1);
  (*m_bits).set_bit(pack_index, a.value());
  pack_index++;
}

void uvm_packer::unpack_sc_logic( sc_dt::sc_logic& a )
{
  if (!m_enough_bits(1, "sc_logic") || m_bits == nullptr)
  {
    a = sc_dt::SC_LOGIC_X;
    return;
  }
  sc_dt::sc_logic_value_t val = sc_dt::sc_logic_value_t((*m_bits).get_bit(unpack_index));
  a = val;
  inc_unpack_index(1);
}

void uvm_packer::pack_sc_bv_base( const sc_dt::sc_bv_base& a )
{
  int nbits = a.length();
  m_check_size(nbits);
  (*m_bits)(pack_index + nbits - 1, pack_index) = a;
  pack_index += nbits;
}

void uvm_packer::unpack_sc_bv_base( sc_dt::sc_bv_base& a )
{
  int nbits = a.length();
  if (nbits <= 0)
    return;
  if (!m_enough_bits(nbits, "sc_bv") || m_bits == nullptr)
    return;
  a = (*m_bits)(unpack_index + nbits - 1, unpack_index);
  inc_unpack_index(nbits);
}

void uvm_packer::pack_sc_lv_base( const sc_dt::sc_lv_base& a )
{
  int n = a.length();
  m_check_size(n);
  for (int i = 0; i < n; i++)
  {
    sc_dt::sc_logic_value_t val = a.get_bit(i);
    (*m_bits).set_bit(pack_index, val);
    pack_index++;
  }
}

void uvm_packer::unpack_sc_lv_base( sc_dt::sc_lv_base& a )
{
  int n = a.length();
  if (n <= 0)
    return;
  if (!m_enough_bits(n, "sc_lv") || m_bits == nullptr)
    return;
  for (int i = 0; i < n; i++)
  {
    sc_dt::sc_logic_value_t val = sc_dt::sc_logic_value_t((*m_bits).get_bit(unpack_index + i));
    a.set_bit(i, val);
  }
  inc_unpack_index(n);
}

void uvm_packer::pack_sc_int_base( const sc_dt::sc_int_base& a )
{
  int n = a.length();
  m_check_size(n);
  for (int i = 0; i < n; i++)
  {
    bool val = a.test(i);
    if (val) {
      (*m_bits).set_bit(pack_index, sc_dt::Log_1);
    } else {
      (*m_bits).set_bit(pack_index, sc_dt::Log_0);
    }
    pack_index++;
  }
}

void uvm_packer::unpack_sc_int_base( sc_dt::sc_int_base& a )
{
  int n = a.length();
  if (n <= 0)
    return;
  if (!m_enough_bits(n, "sc_int") || m_bits == nullptr)
    return;
  sc_dt::uint_type raw = 0;
  for (int i = 0; i < n; i++)
  {
    sc_dt::sc_logic_value_t val = sc_dt::sc_logic_value_t((*m_bits).get_bit(unpack_index + i));
    if (val == sc_dt::Log_1)
      raw |= (sc_dt::UINT_ONE << i);
  }
  a = static_cast<sc_dt::int_type>(raw);
  inc_unpack_index(n);
}

void uvm_packer::pack_sc_uint_base( const sc_dt::sc_uint_base& a )
{
  int n = a.length();
  m_check_size(n);
  for (int i = 0; i < n; i++)
  {
    bool val = a.test(i);
    if (val)
    {
      (*m_bits).set_bit(pack_index, sc_dt::Log_1);
    } else {
      (*m_bits).set_bit(pack_index, sc_dt::Log_0);
    }
    pack_index++;
  }
}

void uvm_packer::unpack_sc_uint_base( sc_dt::sc_uint_base& a )
{
  int n = a.length();
  if (n <= 0)
    return;
  if (!m_enough_bits(n, "sc_uint") || m_bits == nullptr)
    return;
  for (int i = 0; i < n; i++)
  {
    sc_dt::sc_logic_value_t val = sc_dt::sc_logic_value_t((*m_bits).get_bit(unpack_index + i));
    a.set(i, val == sc_dt::Log_1);
  }
  inc_unpack_index(n);
}

void uvm_packer::pack_sc_signed( const sc_dt::sc_signed& a )
{
  int n = a.length();
  m_check_size(n);
  for (int i = 0; i < n; i++) {
    bool val = a.test(i);
    if (val) {
      (*m_bits).set_bit(pack_index, sc_dt::Log_1);
    } else {
      (*m_bits).set_bit(pack_index, sc_dt::Log_0);
    }
    pack_index++;
  }
}

void uvm_packer::unpack_sc_signed( sc_dt::sc_signed& a )
{
  int n = a.length();
  if (n <= 0)
    return;
  if (!m_enough_bits(n, "sc_signed") || m_bits == nullptr)
    return;
  for (int i = 0; i < n; i++)
  {
    sc_dt::sc_logic_value_t val = sc_dt::sc_logic_value_t((*m_bits).get_bit(unpack_index + i));
    a.set(i, val == sc_dt::Log_1);
  }
  inc_unpack_index(n);
}

void uvm_packer::pack_sc_unsigned( const sc_dt::sc_unsigned& a )
{
  int n = a.length();
  m_check_size(n);
  for (int i = 0; i < n; i++)
  {
    bool val = a.test(i);
    if (val)
    {
      (*m_bits).set_bit(pack_index, sc_dt::Log_1);
    } else {
      (*m_bits).set_bit(pack_index, sc_dt::Log_0);
    }
    pack_index++;
  }
}

void uvm_packer::unpack_sc_unsigned( sc_dt::sc_unsigned& a )
{
  int n = a.length();
  if (n <= 0)
    return;
  if (!m_enough_bits(n, "sc_unsigned") || m_bits == nullptr)
    return;
  for (int i = 0; i < n; i++)
  {
    sc_dt::sc_logic_value_t val = sc_dt::sc_logic_value_t((*m_bits).get_bit(unpack_index + i));
    a.set(i, val == sc_dt::Log_1);
  }
  inc_unpack_index(n);
}


//------------------------------------------------------------------------------
// Member function: set_packed_size
//
//! Implementation defined
//------------------------------------------------------------------------------

void uvm_packer::set_packed_size()
{
  // TODO remove?
  //packed_index = count;
  //pack_index = 0;
}


////////////////

} // namespace uvm
