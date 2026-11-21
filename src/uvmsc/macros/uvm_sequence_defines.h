//----------------------------------------------------------------------
//   Copyright 2012-2020 NXP B.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2010 Cadence Design Systems, Inc.
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
//----------------------------------------------------------------------

#ifndef UVM_SEQUENCE_DEFINES_H_
#define UVM_SEQUENCE_DEFINES_H_

//----------------------------------------------------------------------
// MACRO: UVM_DO
//
//! This macro takes as an argument a #uvm_sequence_item variable or object.
//! In the case of a #sequence_item, the item is sent to the driver through
//! the associated sequencer.
//! In the case of a #sequence, the sub-sequence is started using
//! uvm_sequence_base::start() with call_pre_post set to false.
//! The item, or the sequence unless its member do_not_randomize is set,
//! is randomized with its public member function randomize(), if it has
//! one, before it is sent or started.
//----------------------------------------------------------------------

#define UVM_DO(SEQ_OR_ITEM) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, -1, "")

//----------------------------------------------------------------------
// MACRO: UVM_DO_PRI
//
//! This is the same as macro #UVM_DO except that the sequence item or
//! sequence is executed with the priority specified in the argument
//----------------------------------------------------------------------

#define UVM_DO_PRI(SEQ_OR_ITEM, PRIORITY) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, PRIORITY, "")

//----------------------------------------------------------------------
// MACRO: UVM_DO_WITH
//
//! This is the same as macro #UVM_DO except that the constraint block in the 2nd
//! argument is applied to the item or sequence in a randomize with statement
//! before execution.
//! In this implementation the constraints are not applied: the item or
//! sequence is randomized as in #UVM_DO, and a warning RNDCSTR is given
//! once for each use of the macro with constraints.
//----------------------------------------------------------------------

#define UVM_DO_WITH(SEQ_OR_ITEM, CONSTRAINTS) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, -1, CONSTRAINTS)

//----------------------------------------------------------------------
// MACRO: UVM_DO_PRI_WITH
//
//! This is the same as macro #UVM_DO_PRI except that the given constraint
//! block is applied to the item or sequence in a randomize with statement
//! before execution.
//----------------------------------------------------------------------

#define UVM_DO_PRI_WITH(SEQ_OR_ITEM, PRIORITY, CONSTRAINTS) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, PRIORITY, CONSTRAINTS)

//----------------------------------------------------------------------
// MACRO: UVM_DO_ON
//
//! This is the same as #UVM_DO except that it also sets the parent sequence to
//! the sequence in which the macro is invoked, and it sets the sequencer to the
//! specified \p SEQR argument.
//----------------------------------------------------------------------

#define UVM_DO_ON(SEQ_OR_ITEM, SEQR) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, -1, "")

//----------------------------------------------------------------------
// MACRO: UVM_DO_ON_PRI
//
//! This is the same as #UVM_DO_PRI except that it also sets the parent sequence
//! to the sequence in which the macro is invoked, and it sets the sequencer to
//! the specified \p SEQR argument.
//----------------------------------------------------------------------

#define UVM_DO_ON_PRI(SEQ_OR_ITEM, SEQR, PRIORITY) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, PRIORITY, "")

//----------------------------------------------------------------------
// MACRO: UVM_DO_ON_WITH
//
//! This is the same as #UVM_DO_WITH except that it also sets the parent
//! sequence to the sequence in which the macro is invoked, and it sets the
//! sequencer to the specified \p SEQR argument.
//----------------------------------------------------------------------

#define UVM_DO_ON_WITH(SEQ_OR_ITEM, SEQR, CONSTRAINTS) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, -1, CONSTRAINTS)

//----------------------------------------------------------------------
// MACRO: UVM_DO_ON_PRI_WITH
//
//! This is the same as #UVM_DO_PRI_WITH except that it also sets the parent
//! sequence to the sequence in which the macro is invoked, and it sets the
//! sequencer to the specified \p SEQR argument.
//----------------------------------------------------------------------

#define UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, PRIORITY, CONSTRAINTS) \
  { \
  ::uvm::uvm_sequence_base* seq__; \
  UVM_CREATE_ON(SEQ_OR_ITEM, SEQR) \
  seq__ = dynamic_cast<uvm::uvm_sequence_base*>(SEQ_OR_ITEM); \
  if (seq__ == nullptr) start_item(SEQ_OR_ITEM, PRIORITY); \
  if ((seq__ == nullptr || !seq__->do_not_randomize) && \
      !SEQ_OR_ITEM->m_uvm_randomize()) \
  { UVM_WARNING("RNDFLD", "Randomization failed in uvm_do_with action"); } \
  { \
    static bool warned__ = false; \
    if (!warned__ && std::string(#CONSTRAINTS) != "\"\"") \
    { \
      warned__ = true; \
      UVM_WARNING("RNDCSTR", "The constraints of uvm_do_with are not applied: " #CONSTRAINTS); \
    } \
  } \
  if (seq__ == nullptr) finish_item(SEQ_OR_ITEM, PRIORITY); \
  else seq__->start(SEQR, this, PRIORITY, 0); \
  } \

//----------------------------------------------------------------------
// MACRO: UVM_CREATE
//
//! This action creates the item or sequence using the factory.  It intentionally
//! does zero processing.  After this action completes, the user can manually set
//! values, manipulate #rand_mode and #constraint_mode, etc.
//----------------------------------------------------------------------

#define UVM_CREATE(SEQ_OR_ITEM) \
  UVM_CREATE_ON(SEQ_OR_ITEM, m_sequencer)

//----------------------------------------------------------------------
// MACRO: UVM_CREATE_ON(SEQ_OR_ITEM, SEQR)
//
//! This is the same as #UVM_CREATE except that it also sets the parent sequence
//! to the sequence in which the macro is invoked, and it sets the sequencer to
//! the specified \p SEQR argument.
//! This macro is used by the #UVM_DO macro.
//----------------------------------------------------------------------

#define UVM_CREATE_ON(SEQ_OR_ITEM, SEQR) \
  ::uvm::uvm_object_wrapper* objw__; \
  objw__ = SEQ_OR_ITEM->get_type(); \
  SEQ_OR_ITEM = dynamic_cast< uvm_typeof(SEQ_OR_ITEM) >(create_item(objw__, SEQR, #SEQ_OR_ITEM));


//----------------------------------------------------------------------
// MACRO: UVM_DECLARE_P_SEQUENCER
//
//! This macro is used to declare a variable p_sequencer whose type is
//! specified by the argument \p SEQR
//----------------------------------------------------------------------

#define UVM_DECLARE_P_SEQUENCER(SEQR) \
  SEQR* p_sequencer; \
  void m_set_p_sequencer() { \
    p_sequencer = dynamic_cast<SEQR*>(::uvm::uvm_sequence_item::m_sequencer); \
    if(!p_sequencer) { \
      std::ostringstream msg; \
      msg << ": Error casting p_sequencer, please verify that this sequence/sequence item " \
          << "is intended to execute on this type of sequencer."; \
      ::uvm::uvm_report_fatal("DCLPSQ", msg.str()); \
    } \
  }


//----------------------------------------------------------------------
// MACRO: UVM_SEQUENCE_LIBRARY_UTILS
//
//! Declares the infrastructure needed to define extensions to the
//! uvm_sequence_library class. Use this macro in the class of the
//! sequence library, which must also call init_sequence_library() in
//! its constructor:
//!
//!   class my_seq_lib : public uvm::uvm_sequence_library<my_item>
//!   {
//!    public:
//!     UVM_OBJECT_UTILS(my_seq_lib);
//!     UVM_SEQUENCE_LIBRARY_UTILS(my_seq_lib);
//!
//!     my_seq_lib( const std::string& name = "" )
//!     : uvm::uvm_sequence_library<my_item>(name)
//!     { init_sequence_library(); }
//!   };
//----------------------------------------------------------------------

#define UVM_SEQUENCE_LIBRARY_UTILS(TYPE) \
  static std::vector< ::uvm::uvm_object_wrapper*>& m_typewide_sequences() \
  { \
    static std::vector< ::uvm::uvm_object_wrapper*> typewide_sequences; \
    return typewide_sequences; \
  } \
  static std::vector< ::uvm::uvm_object_wrapper* (*)()>& m_typewide_getters() \
  { \
    static std::vector< ::uvm::uvm_object_wrapper* (*)()> typewide_getters; \
    return typewide_getters; \
  } \
  static bool m_add_typewide_sequence( ::uvm::uvm_object_wrapper* (*getter)() ) \
  { \
    TYPE::m_typewide_getters().push_back(getter); \
    return true; \
  } \
  static void add_typewide_sequence( ::uvm::uvm_object_wrapper* seq_type ) \
  { \
    std::vector< ::uvm::uvm_object_wrapper*>& typewide = TYPE::m_typewide_sequences(); \
    for (unsigned int i = 0; i < typewide.size(); i++) \
      if (typewide[i] == seq_type) return; \
    if (TYPE::m_check(seq_type, nullptr)) \
      typewide.push_back(seq_type); \
  } \
  static void add_typewide_sequences( const std::vector< ::uvm::uvm_object_wrapper*>& seq_types ) \
  { \
    for (unsigned int i = 0; i < seq_types.size(); i++) \
      TYPE::add_typewide_sequence(seq_types[i]); \
  } \
  void init_sequence_library() \
  { \
    this->m_init_from(TYPE::m_typewide_sequences(), TYPE::m_typewide_getters()); \
  }

//----------------------------------------------------------------------
// MACRO: UVM_ADD_TO_SEQ_LIB
//
//! Adds the given sequence TYPE to the given sequence library LIBTYPE.
//! Use this macro in the class of the sequence, after UVM_OBJECT_UTILS.
//! It may be used more than once to add the sequence to several
//! libraries. The sequence becomes available to all instances of the
//! library which are created afterwards.
//!
//! The class of the library shall be defined before the class of the
//! sequence, and the macro shall be on a line of its own: it defines
//! members which are named after the line it is on. It needs C++17.
//----------------------------------------------------------------------

#define M_UVM_SEQ_LIB_CONCAT_(A, B) A##B
#define M_UVM_SEQ_LIB_CONCAT(A, B) M_UVM_SEQ_LIB_CONCAT_(A, B)

#define UVM_ADD_TO_SEQ_LIB(TYPE, LIBTYPE) \
  static ::uvm::uvm_object_wrapper* M_UVM_SEQ_LIB_CONCAT(m_uvm_seq_lib_wrapper_, __LINE__)() \
  { \
    return ::uvm::uvm_object_registry<TYPE>::get(); \
  } \
  static inline const bool M_UVM_SEQ_LIB_CONCAT(m_uvm_seq_lib_added_, __LINE__) = \
    LIBTYPE::m_add_typewide_sequence(&M_UVM_SEQ_LIB_CONCAT(m_uvm_seq_lib_wrapper_, __LINE__));


#endif /* UVM_SEQUENCE_DEFINES_H_ */
