//----------------------------------------------------------------------
//   Copyright 2010 Mentor Graphics Corporation
//   Copyright 2011 Synopsys, Inc.
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

#ifndef UVM_SEQUENCE_LIBRARY_H_
#define UVM_SEQUENCE_LIBRARY_H_

#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_object.h"
#include "uvmsc/base/uvm_object_globals.h"
#include "uvmsc/base/uvm_root.h"
#include "uvmsc/conf/uvm_config_db.h"
#include "uvmsc/factory/uvm_factory.h"
#include "uvmsc/factory/uvm_object_registry.h"
#include "uvmsc/factory/uvm_object_wrapper.h"
#include "uvmsc/macros/uvm_message_defines.h"
#include "uvmsc/phasing/uvm_phase.h"
#include "uvmsc/print/uvm_printer.h"
#include "uvmsc/seq/uvm_sequence.h"
#include "uvmsc/seq/uvm_sequence_base.h"
#include "uvmsc/seq/uvm_sequence_item.h"
#include "uvmsc/seq/uvm_sequencer.h"

namespace uvm {

namespace detail {

// The proxy of an item type which is registered with the factory, and
// nullptr for a type which is not, as uvm_sequence_item itself.
template <typename T>
auto uvm_seq_lib_item_wrapper( int ) -> decltype( T::type_id::get(), (uvm_object_wrapper*)nullptr )
{
  return T::type_id::get();
}

template <typename T>
uvm_object_wrapper* uvm_seq_lib_item_wrapper( long )
{
  return nullptr;
}

} // namespace detail

//----------------------------------------------------------------------
// Enum: uvm_sequence_lib_mode
//
//! Specifies the random selection mode of a sequence library
//!
//! UVM_SEQ_LIB_RAND  - Random sequence selection
//! UVM_SEQ_LIB_RANDC - Random cyclic sequence selection
//! UVM_SEQ_LIB_ITEM  - Emit only items, no sequence execution
//! UVM_SEQ_LIB_USER  - Apply a user-defined random-selection algorithm
//----------------------------------------------------------------------

enum uvm_sequence_lib_mode
{
  UVM_SEQ_LIB_RAND,
  UVM_SEQ_LIB_RANDC,
  UVM_SEQ_LIB_ITEM,
  UVM_SEQ_LIB_USER
};

inline const char* uvm_sequence_lib_mode_name( uvm_sequence_lib_mode mode )
{
  switch (mode)
  {
    case UVM_SEQ_LIB_RAND:  return "UVM_SEQ_LIB_RAND";
    case UVM_SEQ_LIB_RANDC: return "UVM_SEQ_LIB_RANDC";
    case UVM_SEQ_LIB_ITEM:  return "UVM_SEQ_LIB_ITEM";
    case UVM_SEQ_LIB_USER:  return "UVM_SEQ_LIB_USER";
    default:                return "<unknown>";
  }
}

//----------------------------------------------------------------------
// CLASS: uvm_sequence_library_cfg
//
//! A convenient container class for configuring all the sequence
//! library parameters using a single set command.
//!
//!   uvm_sequence_library_cfg* cfg =
//!     new uvm_sequence_library_cfg("seqlib_cfg", UVM_SEQ_LIB_RANDC, 1000, 2000);
//!
//!   uvm_config_db<uvm_sequence_library_cfg*>::set(nullptr,
//!     "env.agent.sequencer.main_phase", "default_sequence.config", cfg);
//----------------------------------------------------------------------

class uvm_sequence_library_cfg : public uvm_object
{
 public:
  virtual const std::string get_type_name() const
  {
    return "uvm_sequence_library_cfg";
  }

  uvm_sequence_lib_mode selection_mode;
  unsigned int min_random_count;
  unsigned int max_random_count;

  explicit uvm_sequence_library_cfg( const std::string& name = "",
                                     uvm_sequence_lib_mode mode = UVM_SEQ_LIB_RAND,
                                     unsigned int min = 1,
                                     unsigned int max = 10 )
  : uvm_object(name),
    selection_mode(mode),
    min_random_count(min),
    max_random_count(max)
  {}

}; // class uvm_sequence_library_cfg

//----------------------------------------------------------------------
// CLASS: uvm_sequence_library<REQ,RSP>
//
//! The uvm_sequence_library is a sequence that contains a list of
//! registered sequence types. It can be configured to create and
//! execute these sequences any number of times using one of several
//! modes of operation, including a user-defined mode.
//!
//! When started (as any other sequence), the sequence library will
//! select and execute a number of sequences between min_random_count
//! and max_random_count from its sequences queue. If in
//! UVM_SEQ_LIB_ITEM mode, it will execute that many sequence items.
//!
//! As UVM-SystemC does not randomize objects, the number of sequences
//! is drawn when the library is started, and the sequences and items
//! it executes are not randomized. Every library draws from its own
//! generator, which starts from the same seed, so a run can be repeated
//! on any platform; m_set_seed() gives a library another seed.
//!
//! Other differences from UVM-SV: the configuration is read from the
//! database each time the library is started; print() does not list the
//! sequences which were added by type separately from those of the
//! library; and the library creates an object of a type to check that it
//! is a sequence when the type is added.
//!
//! To use the sequence library, derive from it with the macro
//! UVM_SEQUENCE_LIBRARY_UTILS, call init_sequence_library() in the
//! constructor, and add sequences to it with add_sequence(),
//! add_typewide_sequence() or the macro UVM_ADD_TO_SEQ_LIB.
//----------------------------------------------------------------------

template <typename REQ = uvm_sequence_item, typename RSP = REQ>
class uvm_sequence_library : public uvm_sequence<REQ,RSP>
{
 public:
  typedef uvm_sequence_library<REQ,RSP> this_type;

  //! A function which returns the proxy of a sequence type. Sequence types
  //! which are added while the program starts are kept as such functions,
  //! so that the factory is not used before it exists.
  typedef uvm_object_wrapper* (*m_wrapper_getter)();

  //--------------------------------------------------------------------
  // UVM Standard LRM API below
  //--------------------------------------------------------------------

  explicit uvm_sequence_library( const std::string& name = "" );

  //! Specifies the mode used to select sequences for execution.
  //!
  //! If you do not have access to an instance of the library, use the
  //! configuration resource interface: the sequence library looks for
  //! "default_sequence.selection_mode", "default_sequence.min_random_count",
  //! "default_sequence.max_random_count" and "default_sequence.config" in
  //! the scope of its sequencer and starting phase.
  uvm_sequence_lib_mode selection_mode;

  //! Sets the minimum number of items to execute. Default is 10.
  unsigned int min_random_count;

  //! Sets the maximum number of items to execute. Default is 10.
  unsigned int max_random_count;

  //! Specifies the number of sequences to execute when this library is
  //! started. It is drawn between min_random_count and max_random_count
  //! at the start of body().
  unsigned int sequence_count;

  virtual unsigned int select_sequence( unsigned int max );

  static void add_typewide_sequence( uvm_object_wrapper* seq_type );
  static void add_typewide_sequences( const std::vector<uvm_object_wrapper*>& seq_types );

  void add_sequence( uvm_object_wrapper* seq_type );
  virtual void add_sequences( const std::vector<uvm_object_wrapper*>& seq_types );
  virtual void remove_sequence( uvm_object_wrapper* seq_type );
  virtual void get_sequences( std::vector<uvm_object_wrapper*>& seq_types );
  virtual uvm_object_wrapper* get_sequence( unsigned int idx );

  void init_sequence_library();

  virtual void body();

  /////////////////////////////////////////////////////
  // Implementation-defined member functions below,
  // not part of UVM Class reference / LRM
  /////////////////////////////////////////////////////

  virtual const std::string get_type_name() const;

  virtual void do_print( const uvm_printer& printer ) const;

  virtual void execute( uvm_object_wrapper* wrap );

  static bool m_add_typewide_sequence( m_wrapper_getter getter );

  //! Sets the seed of the random selection of this library
  void m_set_seed( unsigned int seed );

 protected:
  static bool m_check( uvm_object_wrapper* seq_type, this_type* lib );
  static bool m_static_check( uvm_object_wrapper* seq_type );
  bool m_dyn_check( uvm_object_wrapper* seq_type );
  void m_get_config();
  void m_init_from( std::vector<uvm_object_wrapper*>& typewide,
                    std::vector<m_wrapper_getter>& getters );

  //! Returns a random number between 0 and \p max. The numbers are the
  //! same on every platform.
  virtual unsigned int m_random( unsigned int max );

  static std::vector<uvm_object_wrapper*>& m_typewide_sequences();
  static std::vector<m_wrapper_getter>& m_typewide_getters();

  unsigned int sequences_executed;
  unsigned int counter;
  std::map<std::string, int> seqs_distrib;
  std::vector<uvm_object_wrapper*> sequences;

 private:
  std::mt19937 m_rng;
  std::vector<unsigned int> m_randc_deck;

}; // class uvm_sequence_library


/////////////////////////////////////////
// Class implementation starts here
/////////////////////////////////////////

//----------------------------------------------------------------------
// Constructor
//
//! Create a new instance of this class
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
uvm_sequence_library<REQ,RSP>::uvm_sequence_library( const std::string& name )
: uvm_sequence<REQ,RSP>(name),
  selection_mode(UVM_SEQ_LIB_RAND),
  min_random_count(10),
  max_random_count(10),
  sequence_count(10),
  sequences_executed(0),
  counter(0),
  m_rng(1)
{
  init_sequence_library();
}

//----------------------------------------------------------------------
// member function: get_type_name (virtual)
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
const std::string uvm_sequence_library<REQ,RSP>::get_type_name() const
{
  return "uvm_sequence_library #(REQ,RSP)";
}

//----------------------------------------------------------------------
// member function: m_typewide_sequences (static)
//
// Implementation defined
// The sequence types which every instance of this library type starts with
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
std::vector<uvm_object_wrapper*>& uvm_sequence_library<REQ,RSP>::m_typewide_sequences()
{
  static std::vector<uvm_object_wrapper*> typewide_sequences;
  return typewide_sequences;
}

template <typename REQ, typename RSP>
std::vector<typename uvm_sequence_library<REQ,RSP>::m_wrapper_getter>&
uvm_sequence_library<REQ,RSP>::m_typewide_getters()
{
  static std::vector<m_wrapper_getter> typewide_getters;
  return typewide_getters;
}

//----------------------------------------------------------------------
// member function: select_sequence (virtual)
//
//! Generates an index used to select the next sequence to execute.
//! Overrides must return a value between 0 and \p max, inclusive.
//! Used only for UVM_SEQ_LIB_USER selection mode. The default
//! implementation returns 0, incrementing on successive calls, wrapping
//! back to 0 when reaching \p max.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
unsigned int uvm_sequence_library<REQ,RSP>::select_sequence( unsigned int max )
{
  unsigned int selection = counter;

  counter++;
  if (counter > max)
    counter = 0;

  return selection;
}

//----------------------------------------------------------------------
// member function: add_typewide_sequence (static)
//
//! Registers the provided sequence type with this sequence library
//! type. The sequence type will be available for selection by all
//! instances of this class. Sequence types already registered are
//! silently ignored.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::add_typewide_sequence( uvm_object_wrapper* seq_type )
{
  if (m_static_check(seq_type))
    m_typewide_sequences().push_back(seq_type);
}

//----------------------------------------------------------------------
// member function: add_typewide_sequences (static)
//
//! Registers the provided sequence types with this sequence library
//! type.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::add_typewide_sequences(
  const std::vector<uvm_object_wrapper*>& seq_types )
{
  for (unsigned int i = 0; i < seq_types.size(); i++)
    add_typewide_sequence(seq_types[i]);
}

//----------------------------------------------------------------------
// member function: m_add_typewide_sequence (static)
//
// Implementation defined
// Used by UVM_ADD_TO_SEQ_LIB while the program starts
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
bool uvm_sequence_library<REQ,RSP>::m_add_typewide_sequence( m_wrapper_getter getter )
{
  m_typewide_getters().push_back(getter);
  return true;
}

//----------------------------------------------------------------------
// member function: add_sequence
//
//! Registers the provided sequence type with this sequence library
//! instance. Sequence types already registered are silently ignored.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::add_sequence( uvm_object_wrapper* seq_type )
{
  if (m_dyn_check(seq_type))
    sequences.push_back(seq_type);
}

//----------------------------------------------------------------------
// member function: add_sequences (virtual)
//
//! Registers the provided sequence types with this sequence library
//! instance.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::add_sequences(
  const std::vector<uvm_object_wrapper*>& seq_types )
{
  for (unsigned int i = 0; i < seq_types.size(); i++)
    add_sequence(seq_types[i]);
}

//----------------------------------------------------------------------
// member function: remove_sequence (virtual)
//
//! Removes the given sequence type from this sequence library instance.
//! If the type was registered statically, the sequence queues of all
//! instances of this library will be updated accordingly. A warning is
//! issued if the sequence is not registered.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::remove_sequence( uvm_object_wrapper* seq_type )
{
  for (unsigned int i = 0; i < sequences.size(); i++)
  {
    if (sequences[i] == seq_type)
    {
      sequences.erase(sequences.begin() + i);
      m_randc_deck.clear();
      return;
    }
  }
}

//----------------------------------------------------------------------
// member function: get_sequences (virtual)
//
//! Appends to the provided \p seq_types array the list of registered
//! sequences.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::get_sequences(
  std::vector<uvm_object_wrapper*>& seq_types )
{
  for (unsigned int i = 0; i < sequences.size(); i++)
    seq_types.push_back(sequences[i]);
}

//----------------------------------------------------------------------
// member function: get_sequence (virtual)
//
//! Returns the sequence type registered at index \p idx.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
uvm_object_wrapper* uvm_sequence_library<REQ,RSP>::get_sequence( unsigned int idx )
{
  if (idx < sequences.size())
    return sequences[idx];

  std::ostringstream str;
  str << "idx " << idx << " > number of sequences in library";
  UVM_ERROR("SEQ_LIB/GET_SEQ", str.str());
  return nullptr;
}

//----------------------------------------------------------------------
// member function: init_sequence_library
//
//! All subtypes of this class must call init_sequence_library in their
//! constructor.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::init_sequence_library()
{
  m_init_from(m_typewide_sequences(), m_typewide_getters());
}

//----------------------------------------------------------------------
// member function: m_init_from
//
// Implementation defined
// Adds the type-wide sequences of a library type to this instance. The
// types which were added while the program started are resolved first.
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::m_init_from(
  std::vector<uvm_object_wrapper*>& typewide,
  std::vector<m_wrapper_getter>& getters )
{
  for (unsigned int i = 0; i < getters.size(); i++)
  {
    uvm_object_wrapper* seq_type = getters[i]();
    bool known = false;

    for (unsigned int j = 0; j < typewide.size(); j++)
      known = known || (typewide[j] == seq_type);

    if (!known && m_check(seq_type, nullptr))
      typewide.push_back(seq_type);
  }
  getters.clear();

  for (unsigned int i = 0; i < typewide.size(); i++)
  {
    bool known = false;

    for (unsigned int j = 0; j < sequences.size(); j++)
      known = known || (sequences[j] == typewide[i]);

    if (!known)
      sequences.push_back(typewide[i]);
  }
}

//----------------------------------------------------------------------
// member function: m_static_check (static)
//
// Implementation defined
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
bool uvm_sequence_library<REQ,RSP>::m_static_check( uvm_object_wrapper* seq_type )
{
  if (!m_check(seq_type, nullptr))
    return false;

  std::vector<uvm_object_wrapper*>& typewide = m_typewide_sequences();

  for (unsigned int i = 0; i < typewide.size(); i++)
    if (typewide[i] == seq_type)
      return false;

  return true;
}

//----------------------------------------------------------------------
// member function: m_dyn_check
//
// Implementation defined
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
bool uvm_sequence_library<REQ,RSP>::m_dyn_check( uvm_object_wrapper* seq_type )
{
  if (!m_check(seq_type, this))
    return false;

  for (unsigned int i = 0; i < sequences.size(); i++)
    if (sequences[i] == seq_type)
      return false;

  return true;
}

//----------------------------------------------------------------------
// member function: m_check (static)
//
// Implementation defined
// Checks that the type is a sequence
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
bool uvm_sequence_library<REQ,RSP>::m_check( uvm_object_wrapper* seq_type,
                                             this_type* lib )
{
  if (seq_type == nullptr)
    return false;

  uvm_object* obj = seq_type->create_object();
  uvm_sequence_base* seq = dynamic_cast<uvm_sequence_base*>(obj);

  if (seq == nullptr)
  {
    const std::string name =
      (lib == nullptr) ? std::string("uvm_sequence_library #(REQ,RSP)") : lib->get_full_name();

    uvm_coreservice_t::get()->get_root()->uvm_report_error("SEQLIB/BAD_SEQ_TYPE",
      "Object '" + ((obj != nullptr) ? obj->get_type_name() : std::string("<null>")) +
      "' is not a sequence. Cannot add to sequence library '" + name + "'");
    delete obj;
    return false;
  }

  delete obj;
  return true;
}

//----------------------------------------------------------------------
// member function: m_set_seed / m_random
//
// Implementation defined
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::m_set_seed( unsigned int seed )
{
  m_rng.seed(seed);
  m_randc_deck.clear();
}

template <typename REQ, typename RSP>
unsigned int uvm_sequence_library<REQ,RSP>::m_random( unsigned int max )
{
  // The engine gives the same numbers everywhere; the distributions of
  // the standard library do not.
  if (max == 0xffffffffu)
    return static_cast<unsigned int>(m_rng());

  return static_cast<unsigned int>(m_rng() % (max + 1u));
}

//----------------------------------------------------------------------
// member function: m_get_config
//
// Implementation defined
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::m_get_config()
{
  uvm_sequence_library_cfg* cfg = nullptr;
  std::string phase_name;
  uvm_phase* starting_phase = this->get_starting_phase();

  if (starting_phase != nullptr)
    phase_name = starting_phase->get_name() + "_phase";

  if (uvm_config_db<uvm_sequence_library_cfg*>::get(this->m_sequencer, phase_name,
        "default_sequence.config", cfg) && cfg != nullptr)
  {
    selection_mode = cfg->selection_mode;
    min_random_count = cfg->min_random_count;
    max_random_count = cfg->max_random_count;
  }
  else
  {
    uvm_config_db<unsigned int>::get(this->m_sequencer, phase_name,
      "default_sequence.min_random_count", min_random_count);
    uvm_config_db<unsigned int>::get(this->m_sequencer, phase_name,
      "default_sequence.max_random_count", max_random_count);
    uvm_config_db<uvm_sequence_lib_mode>::get(this->m_sequencer, phase_name,
      "default_sequence.selection_mode", selection_mode);
  }

  if (max_random_count == 0)
  {
    std::ostringstream str;
    str << "max_random_count (" << max_random_count << ") zero. Nothing will be done.";
    UVM_WARNING("SEQLIB/MAX_ZERO", str.str());

    if (min_random_count > max_random_count)
      min_random_count = max_random_count;
  }
  else if (min_random_count > max_random_count)
  {
    std::ostringstream str;
    str << "min_random_count (" << min_random_count
        << ") greater than max_random_count (" << max_random_count
        << "). Setting min to max.";
    UVM_ERROR("SEQLIB/MIN_GT_MAX", str.str());
    min_random_count = max_random_count;
  }
  else if (selection_mode == UVM_SEQ_LIB_ITEM)
  {
    if (dynamic_cast<uvm_sequencer<REQ,RSP>*>(this->m_sequencer) == nullptr)
    {
      UVM_ERROR("SEQLIB/VIRT_SEQ", std::string("selection_mode cannot be UVM_SEQ_LIB_ITEM when ") +
        "running as a virtual sequence. Using UVM_SEQ_LIB_RAND mode");
      selection_mode = UVM_SEQ_LIB_RAND;
    }
  }
}

//----------------------------------------------------------------------
// member function: body (virtual)
//
//! Selects and executes the sequences of the library
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::body()
{
  uvm_phase* starting_phase = this->get_starting_phase();
  const std::string phase_name =
    (starting_phase != nullptr) ? starting_phase->get_name() : std::string("unknown");

  if (this->m_sequencer == nullptr)
  {
    UVM_FATAL("SEQLIB/VIRT_SEQ", std::string("Sequence library 'm_sequencer' handle is null ") +
      " no current support for running as a virtual sequence.");
    return;
  }

  if (sequences.size() == 0)
  {
    UVM_ERROR("SEQLIB/NOSEQS", std::string("Sequence library does not contain any sequences. ") +
      "Did you forget to call init_sequence_library() in the constructor?");
    return;
  }

  m_get_config();

  // what the randomization of the library does in UVM-SV
  sequence_count = min_random_count + m_random(max_random_count - min_random_count);

  if (starting_phase != nullptr)
    starting_phase->raise_objection(this, "starting sequence library " +
      this->get_full_name() + " (" + this->get_type_name() + ")");

  {
    std::ostringstream str;
    str << "Starting sequence library " << this->get_type_name() << " in "
        << phase_name << " phase: " << sequence_count << " iterations in mode "
        << uvm_sequence_lib_mode_name(selection_mode);
    UVM_INFO("SEQLIB/START", str.str(), UVM_LOW);
  }

  switch (selection_mode)
  {
    case UVM_SEQ_LIB_RAND:
    {
      for (unsigned int i = 1; i <= sequence_count; i++)
        execute(sequences[m_random(static_cast<unsigned int>(sequences.size()) - 1)]);
      break;
    }

    case UVM_SEQ_LIB_RANDC:
    {
      // every sequence is selected once before any is selected again
      std::vector<uvm_object_wrapper*> q;

      for (unsigned int i = 1; i <= sequence_count; i++)
      {
        if (m_randc_deck.empty())
        {
          for (unsigned int j = 0; j < sequences.size(); j++)
            m_randc_deck.push_back(j);

          for (unsigned int j = static_cast<unsigned int>(m_randc_deck.size()) - 1; j > 0; j--)
            std::swap(m_randc_deck[j], m_randc_deck[m_random(j)]);
        }

        q.push_back(sequences[m_randc_deck.back()]);
        m_randc_deck.pop_back();
      }

      for (unsigned int i = 0; i < q.size(); i++)
        execute(q[i]);
      break;
    }

    case UVM_SEQ_LIB_ITEM:
    {
      for (unsigned int i = 1; i <= sequence_count; i++)
        execute(detail::uvm_seq_lib_item_wrapper<REQ>(0));
      break;
    }

    case UVM_SEQ_LIB_USER:
    {
      for (unsigned int i = 1; i <= sequence_count; i++)
      {
        unsigned int user_selection =
          select_sequence(static_cast<unsigned int>(sequences.size()) - 1);

        if (user_selection >= sequences.size())
        {
          UVM_ERROR("SEQLIB/USER_FAIL", "User sequence selection out of range");
          execute(detail::uvm_seq_lib_item_wrapper<REQ>(0));
        }
        else
          execute(sequences[user_selection]);
      }
      break;
    }

    default:
    {
      std::ostringstream str;
      str << "Unknown random sequence selection mode: " << selection_mode;
      UVM_FATAL("SEQLIB/RAND_MODE", str.str());
      break;
    }
  }

  UVM_INFO("SEQLIB/END", "Ending sequence library in phase " + phase_name, UVM_LOW);

  if (starting_phase != nullptr)
    starting_phase->drop_objection(this, "starting sequence library " +
      this->get_full_name() + " (" + this->get_type_name() + ")");
}

//----------------------------------------------------------------------
// member function: execute (virtual)
//
//! Creates and executes a sequence or a sequence item of the given type
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::execute( uvm_object_wrapper* wrap )
{
  if (wrap == nullptr)
  {
    // The item type is not registered with the factory. In UVM-SV this is
    // only the case for uvm_sequence_item itself.
    UVM_ERROR("SEQLIB/BASE_ITEM", "selection_mode cannot be UVM_SEQ_LIB_ITEM when " +
      std::string("the REQ type of the library is not registered with the factory, ") +
      "as the base type uvm_sequence_item");
    return;
  }

  uvm_factory* factory = uvm_coreservice_t::get()->get_factory();

  std::ostringstream name;
  name << wrap->get_type_name() << ":" << (sequences_executed + 1);

  uvm_object* obj = factory->create_object_by_type(wrap, this->get_full_name(), name.str());

  uvm_sequence_base* seq_base = dynamic_cast<uvm_sequence_base*>(obj);
  REQ* req_item = dynamic_cast<REQ*>(obj);

  if (seq_base == nullptr && req_item == nullptr)
  {
    UVM_ERROR("SEQLIB/WRONG_ITEM_TYPE", "The item created by '" + this->get_full_name() +
      "' when in 'UVM_SEQ_LIB_ITEM' mode doesn't match the REQ type which " +
      " was passed in to the uvm_sequence_library#(REQ[,RSP]), this can happen " +
      "if the REQ type which was passed in was a pure-virtual 'virtual class'." +
      "  Either configure the factory overrides to properly generate items for " +
      "this sequence library, or do not execute this sequence library in " +
      "UVM_SEQ_LIB_ITEM mode.");
    delete obj;
    return;
  }

  uvm_sequence_item* seq_or_item = dynamic_cast<uvm_sequence_item*>(obj);

  UVM_INFO("SEQLIB/EXEC", std::string("Executing ") +
    (seq_or_item->is_item() ? "item " : "sequence ") + seq_or_item->get_name() +
    " (" + seq_or_item->get_type_name() + ")", UVM_FULL);

  if (seq_base != nullptr)
    seq_base->start(this->m_sequencer, this);
  else
  {
    this->start_item(seq_or_item);
    this->finish_item(seq_or_item);
  }

  seqs_distrib[seq_or_item->get_type_name()]++;
  sequences_executed++;

  delete obj;
}

//----------------------------------------------------------------------
// member function: do_print (virtual)
//----------------------------------------------------------------------

template <typename REQ, typename RSP>
void uvm_sequence_library<REQ,RSP>::do_print( const uvm_printer& printer ) const
{
  printer.print_field_int("min_random_count", min_random_count, 32, UVM_DEC, ".", "int unsigned");
  printer.print_field_int("max_random_count", max_random_count, 32, UVM_DEC, ".", "int unsigned");
  printer.print_generic("selection_mode", "uvm_sequence_lib_mode", 32,
    uvm_sequence_lib_mode_name(selection_mode));
  printer.print_field_int("sequence_count", sequence_count, 32, UVM_DEC, ".", "int unsigned");

  printer.print_array_header("sequences", static_cast<int>(sequences.size()), "queue_object_types");
  for (unsigned int i = 0; i < sequences.size(); i++)
  {
    std::ostringstream idx;
    idx << "[" << i << "]";
    printer.print_generic(idx.str(), "uvm_object_wrapper", -1, sequences[i]->get_type_name());
  }
  printer.print_array_footer();

  printer.print_array_header("seqs_distrib", static_cast<int>(seqs_distrib.size()), "as_int_string");
  for (std::map<std::string, int>::const_iterator it = seqs_distrib.begin();
       it != seqs_distrib.end(); it++)
    printer.print_field_int("[" + it->first + "]", it->second, 32, UVM_DEC, ".", "int unsigned");
  printer.print_array_footer();
}

} // namespace uvm

#endif // UVM_SEQUENCE_LIBRARY_H_
