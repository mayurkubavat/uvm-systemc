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

#ifndef UVM_TLM_GP_H_
#define UVM_TLM_GP_H_

#include <map>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include <systemc>
#include <tlm>

#include "uvmsc/base/uvm_object.h"
#include "uvmsc/factory/uvm_abstract_object_registry.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/macros/uvm_object_defines.h"
#include "uvmsc/seq/uvm_sequence_item.h"
#include "uvmsc/tlm2/uvm_tlm2_defines.h"

namespace uvm {

//----------------------------------------------------------------------
// Class: uvm_tlm_extension_base
//
//! Base class for UVM TLM-2 generic payload extensions per IEEE
//! 1800.2-2020 section 12.3.2.
//----------------------------------------------------------------------

class uvm_tlm_extension_base : public uvm_object
{
 public:
  UVM_OBJECT_ABSTRACT_UTILS(uvm_tlm_extension_base);

  explicit uvm_tlm_extension_base( const std::string& name = "" );
  virtual ~uvm_tlm_extension_base();

  virtual uvm_tlm_extension_base* get_type_handle() const = 0;
  virtual std::string get_type_handle_name() const = 0;

  virtual uvm_object* create( const std::string& name = "" );
  virtual void do_copy( const uvm_object& rhs );
  virtual void copy_from( const uvm_tlm_extension_base& rhs );

 protected:
  uvm_tlm_extension_base( const std::string& name, no_inst_count_t );
};

//----------------------------------------------------------------------
// Class: uvm_tlm_extension<T>
//
//! Parameterized extension base providing per-type ID handles.
//----------------------------------------------------------------------

template <typename T>
class uvm_tlm_extension : public uvm_tlm_extension_base
{
 public:
  explicit uvm_tlm_extension( const std::string& name = "" )
    : uvm_tlm_extension_base(name) {}

  virtual ~uvm_tlm_extension() {}

  static uvm_tlm_extension_base* ID()
  {
    class type_handle_t final : public uvm_tlm_extension_base
    {
     public:
      explicit type_handle_t( const std::string& type_name )
        : uvm_tlm_extension_base(type_name, no_inst_count_t()) {}
      virtual uvm_tlm_extension_base* get_type_handle() const { return ID(); }
      virtual std::string get_type_handle_name() const { return get_name(); }
    };
    static type_handle_t s_handle(typeid(T).name());
    return &s_handle;
  }

  virtual uvm_tlm_extension_base* get_type_handle() const
  {
    return ID();
  }

  virtual std::string get_type_handle_name() const
  {
    const std::string tn = this->get_type_name();
    return tn.empty() ? std::string(typeid(T).name()) : tn;
  }
};

//----------------------------------------------------------------------
// Class: uvm_tlm_generic_payload
//
//! IEEE 1800.2-2020 section 12.3.1 generic payload transaction class
//! with safe value-owned byte buffers, deep-copy/compare/pack/print/record
//! support, extensions, and bidirectional tlm::tlm_generic_payload interop.
//----------------------------------------------------------------------

class uvm_tlm_generic_payload : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(uvm_tlm_generic_payload);

  explicit uvm_tlm_generic_payload( const std::string& name = "uvm_tlm_generic_payload" );
  uvm_tlm_generic_payload( const uvm_tlm_generic_payload& rhs );
  uvm_tlm_generic_payload& operator=( const uvm_tlm_generic_payload& rhs );
  virtual ~uvm_tlm_generic_payload();

  virtual uvm_object* create( const std::string& name = "" );

  //--------------------------------------------------------------------
  // Command API
  //--------------------------------------------------------------------

  bool is_read() const;
  void set_read();
  bool is_write() const;
  void set_write();
  uvm_tlm_command_e get_command() const;
  void set_command( uvm_tlm_command_e command );

  //--------------------------------------------------------------------
  // Address API
  //--------------------------------------------------------------------

  sc_dt::uint64 get_address() const;
  void set_address( sc_dt::uint64 addr );

  //--------------------------------------------------------------------
  // Data API
  //--------------------------------------------------------------------

  void get_data( std::vector<unsigned char>& p ) const;
  void set_data( const std::vector<unsigned char>& p );
  unsigned char* get_data_ptr();
  const unsigned char* get_data_ptr() const;

  unsigned int get_data_length() const;
  void set_data_length( unsigned int length );

  //--------------------------------------------------------------------
  // Response status API
  //--------------------------------------------------------------------

  bool is_response_ok() const;
  bool is_response_error() const;
  uvm_tlm_response_status_e get_response_status() const;
  void set_response_status( uvm_tlm_response_status_e response_status );
  std::string get_response_string() const;

  //--------------------------------------------------------------------
  // Streaming width, byte enable, DMI, and option API
  //--------------------------------------------------------------------

  unsigned int get_streaming_width() const;
  void set_streaming_width( unsigned int streaming_width );

  void get_byte_enable( std::vector<unsigned char>& p ) const;
  void set_byte_enable( const std::vector<unsigned char>& p );
  unsigned char* get_byte_enable_ptr();
  const unsigned char* get_byte_enable_ptr() const;

  unsigned int get_byte_enable_length() const;
  void set_byte_enable_length( unsigned int byte_enable_length );

  void set_dmi_allowed( bool dmi_allowed );
  bool is_dmi_allowed() const;

  uvm_tlm_gp_option_e get_gp_option() const;
  void set_gp_option( uvm_tlm_gp_option_e gp_opt );

  void reset();

  //--------------------------------------------------------------------
  // Extension API
  //--------------------------------------------------------------------

  uvm_tlm_extension_base* set_extension( uvm_tlm_extension_base* ext );

  template <typename T>
  T* set_extension( T* ext )
  {
    return dynamic_cast<T*>(set_extension(static_cast<uvm_tlm_extension_base*>(ext)));
  }

  uvm_tlm_extension_base* get_extension( const uvm_tlm_extension_base* ext_handle ) const;

  template <typename T>
  T* get_extension() const
  {
    return dynamic_cast<T*>(get_extension(uvm_tlm_extension<T>::ID()));
  }

  template <typename T>
  void get_extension( T*& ext ) const
  {
    ext = get_extension<T>();
  }

  void clear_extension( const uvm_tlm_extension_base* ext_handle );

  template <typename T>
  void clear_extension()
  {
    clear_extension(uvm_tlm_extension<T>::ID());
  }

  template <typename T>
  void clear_extension( const T* )
  {
    clear_extension<T>();
  }

  void clear_extensions();
  unsigned int get_num_extensions() const;

  //--------------------------------------------------------------------
  // SystemC TLM-2 generic payload interoperability
  //--------------------------------------------------------------------

  // Populates dst from *this. If data_buf/be_buf are provided, copies into
  // them and binds dst's pointers to them; else if dst already has non-null
  // pointers, copies into dst's existing buffers; otherwise points dst at
  // *this's internal storage (valid while *this remains alive and unmodified).
  void to_tlm_gp( tlm::tlm_generic_payload& dst,
                  unsigned char* data_buf = nullptr,
                  unsigned char* be_buf = nullptr ) const;

  // Populates dst's metadata and copies into dst's existing non-null buffers
  // without repointing dst's data/byte-enable pointers when they are nullptr.
  void to_tlm_gp_in_place( tlm::tlm_generic_payload& dst ) const;

  void from_tlm_gp( const tlm::tlm_generic_payload& src );

  //--------------------------------------------------------------------
  // uvm_object / uvm_sequence_item virtual hooks
  //--------------------------------------------------------------------

  virtual void do_copy( const uvm_object& rhs );
  virtual bool do_compare( const uvm_object& rhs,
                           const uvm_comparer* comparer = nullptr ) const;
  virtual void do_print( const uvm_printer& printer ) const;
  virtual void do_pack( uvm_packer& packer ) const;
  virtual void do_unpack( uvm_packer& packer );
  virtual void do_record( const uvm_recorder& recorder );
  virtual std::string convert2string() const;

  //--------------------------------------------------------------------
  // Public generic payload attributes
  //--------------------------------------------------------------------

  sc_dt::uint64 m_address;
  uvm_tlm_command_e m_command;
  std::vector<unsigned char> m_data;
  unsigned int m_length;
  uvm_tlm_response_status_e m_response_status;
  bool m_dmi;
  std::vector<unsigned char> m_byte_enable;
  unsigned int m_byte_enable_length;
  unsigned int m_streaming_width;
  uvm_tlm_gp_option_e m_gp_option;

 private:
  void m_copy_gp_fields( const uvm_tlm_generic_payload& rhs );
  std::string m_command_string() const;
  std::string m_option_string() const;

  // Map from extension type handle -> (extension pointer, owned_by_this_gp)
  std::map<const uvm_tlm_extension_base*, std::pair<uvm_tlm_extension_base*, bool> > m_extensions;
};

typedef uvm_tlm_generic_payload uvm_tlm_gp;

} // namespace uvm

#endif // UVM_TLM_GP_H_
