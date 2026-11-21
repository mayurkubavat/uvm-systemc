//----------------------------------------------------------------------
//   Copyright 2020 Robert Bosch GmbH
//   Copyright 2012-2020 NXP B.V.
//   Copyright 2007-2011 Mentor Graphics Corporation
//   Copyright 2007-2011 Cadence Design Systems, Inc.
//   Copyright 2010-2011 Synopsys, Inc.
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

#ifndef UVM_OBJECT_DEFINES_H_
#define UVM_OBJECT_DEFINES_H_

#include <type_traits>
#include <typeinfo>
#include <utility>

namespace uvm {

class uvm_object;
class uvm_object_wrapper;

// Implementation defined: m_uvm_randomize_object(obj, named) calls
// obj->randomize() of the class T if it is public, unambiguous, takes no
// arguments and returns bool or an integral type, and returns its result.
// Otherwise, if named is true (T has a single member named randomize of
// any kind which T can access, detected in the scope of T by
// UVM_OBJECT_UTILS), it warns once for the type and returns true;
// else it returns true. The detection does not derive from T, so final
// classes and classes with a final destructor compile; an overloaded or
// ambiguous randomize which cannot be called is not warned. If the object
// is of a class derived from T, that class does not use UVM_OBJECT_UTILS
// or UVM_OBJECT_PARAM_UTILS, so a randomize() it declares is only called
// if it overrides a virtual randomize() of T; this is warned once for the
// type as well.

struct m_uvm_randomize_none {};

// the type returned by randomize(), or m_uvm_randomize_none if it cannot
// be called
template <typename T>
auto m_uvm_randomize_result( int ) -> decltype(std::declval<T&>().randomize());

template <typename T>
m_uvm_randomize_none m_uvm_randomize_result( ... );

// true if randomize() can be called and returns bool or an integral type
template <typename T>
struct m_uvm_randomize_usable
  : std::integral_constant<bool, std::is_integral<typename std::decay<
      decltype(m_uvm_randomize_result<T>(0))>::type>::value> {};

enum m_uvm_randomize_issue
{
  M_UVM_RANDOMIZE_NOT_USABLE,
  M_UVM_RANDOMIZE_NO_UTILS
};

// warns once for each type (defined in uvm_sequence_item.cpp)
void m_uvm_randomize_warning( const uvm_object* obj,
                              const std::type_info& type,
                              m_uvm_randomize_issue issue );

template <typename T>
bool m_uvm_randomize_call( T* obj, std::false_type, bool named )
{
  if (named)
    m_uvm_randomize_warning(obj, typeid(T), M_UVM_RANDOMIZE_NOT_USABLE);
  return true;
}

template <typename T>
bool m_uvm_randomize_call( T* obj, std::true_type, bool )
{
  return static_cast<bool>(obj->randomize());
}

template <typename T, bool NAMED>
bool m_uvm_randomize_object( T* obj, std::integral_constant<bool, NAMED> )
{
  if (typeid(*obj) != typeid(T))
    m_uvm_randomize_warning(obj, typeid(*obj), M_UVM_RANDOMIZE_NO_UTILS);
  return m_uvm_randomize_call(obj,
    std::integral_constant<bool, m_uvm_randomize_usable<T>::value>(), NAMED);
}

} // namespace uvm

// -------------------------------------------
// MACRO: UVM_OBJECT_UTILS
//
//! This macro is used to register the #uvm_object
//! to the factory and to add some dedicated
//! member functions
// -------------------------------------------

#define UVM_OBJECT_UTILS(...) \
   M_UVM_REPORT_METHODS_REDIRECTION(__VA_ARGS__) \
   M_UVM_OBJECT_REGISTRY_INTERNAL_PARAM(__VA_ARGS__) \
   M_UVM_OBJECT_GET_OBJECT_TYPE_FUNC \
   M_UVM_OBJECT_GET_TYPE_NAME_FUNC_PARAM(__VA_ARGS__)

#define UVM_OBJECT_PARAM_UTILS(...) \
   M_UVM_REPORT_METHODS_REDIRECTION(__VA_ARGS__) \
   M_UVM_OBJECT_REGISTRY_INTERNAL_PARAM(__VA_ARGS__) \
   M_UVM_OBJECT_GET_TYPE_NAME_FUNC_PARAM(__VA_ARGS__)

#define UVM_OBJECT_ABSTRACT_UTILS(...) \
   M_UVM_REPORT_METHODS_REDIRECTION(__VA_ARGS__) \
   M_UVM_OBJECT_ABSTRACT_REGISTRY_INTERNAL_PARAM(__VA_ARGS__) \
   M_UVM_OBJECT_GET_OBJECT_TYPE_FUNC \
   M_UVM_OBJECT_GET_TYPE_NAME_FUNC_PARAM(__VA_ARGS__)

#define UVM_OBJECT_PARAM_ABSTRACT_UTILS(...) \
   M_UVM_REPORT_METHODS_REDIRECTION(__VA_ARGS__) \
   M_UVM_OBJECT_ABSTRACT_REGISTRY_INTERNAL_PARAM(__VA_ARGS__) \
   M_UVM_OBJECT_GET_TYPE_NAME_FUNC_PARAM(__VA_ARGS__)

// -------------------------------------------
// MACRO: M_UVM_REPORT_METHODS_REDIRECTION
//
// Implementation-defined macro  ----
// -------------------------------------------

#define M_UVM_REPORT_METHODS_REDIRECTION(...) \
    template<typename __PSI_TYPE>                                                                         \
    class IsSequenceItem {                                                                                \
        typedef char hasNoSequenceItem[1];                                                                \
        typedef char hasSequenceItem[2];                                                                  \
                                                                                                          \
        template<typename U>                                                                              \
        static hasSequenceItem& check(typename U::uvm_sequence_item*);                                    \
                                                                                                          \
        template<typename>                                                                                \
        static hasNoSequenceItem& check(...);                                                             \
                                                                                                          \
    public:                                                                                               \
        static const bool result = (sizeof(check<__PSI_TYPE>(0)) == sizeof(hasSequenceItem));             \
    };                                                                                                    \
                                                                                                          \
    /* uvm_report_enabled */                                                                              \
    template <typename __PSI_TYPE = __VA_ARGS__>                                                          \
    typename std::enable_if<IsSequenceItem<__PSI_TYPE>::result,bool>::type uvm_report_enabled(            \
            int verbosity,                                                                                \
            uvm::uvm_severity severity = uvm::UVM_INFO,                                                   \
            std::string id = "" ) const {                                                                 \
       return ((typename __PSI_TYPE::uvm_sequence_item*)this)->uvm_report_enabled(verbosity,severity,id); \
    }                                                                                                     \
                                                                                                          \
    template <typename __PSI_TYPE = __VA_ARGS__>                                                          \
    typename std::enable_if<!IsSequenceItem<__PSI_TYPE>::result,bool>::type uvm_report_enabled(           \
            int verbosity,                                                                                \
            uvm::uvm_severity severity = uvm::UVM_INFO,                                                   \
            std::string id = "" ) const {                                                                 \
       return uvm::uvm_report_enabled(verbosity,severity,id);                                             \
    }                                                                                                     \
                                                                                                          \
    /* uvm_report */                                                                                      \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<IsSequenceItem<__PSI_TYPE>::result>::type uvm_report(                         \
            uvm::uvm_severity severity,                                                                   \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = -1,                                                                           \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        ((typename __PSI_TYPE::uvm_sequence_item*)this)->uvm_report(                                      \
                severity,                                                                                 \
                id,                                                                                       \
                message,                                                                                  \
                verbosity,                                                                                \
                filename,                                                                                 \
                line,                                                                                     \
                context_name,                                                                             \
                report_enabled_checked                                                                    \
                );                                                                                        \
    }                                                                                                     \
                                                                                                          \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<!IsSequenceItem<__PSI_TYPE>::result>::type uvm_report(                        \
            uvm::uvm_severity severity,                                                                   \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = -1,                                                                           \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        uvm::uvm_report(severity,id,message,verbosity,filename,line,context_name,report_enabled_checked); \
    }                                                                                                     \
                                                                                                          \
    /* uvm_report_info */                                                                                 \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_info(                    \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_MEDIUM,                                                              \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        ((typename __PSI_TYPE::uvm_sequence_item*)this)->uvm_report_info(                                 \
                id,                                                                                       \
                message,                                                                                  \
                verbosity,                                                                                \
                filename,                                                                                 \
                line,                                                                                     \
                context_name,                                                                             \
                report_enabled_checked                                                                    \
                );                                                                                        \
    }                                                                                                     \
                                                                                                          \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<!IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_info(                   \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_NONE,                                                                \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        uvm::uvm_report_info(id,message,verbosity,filename,line,context_name,report_enabled_checked);     \
    }                                                                                                     \
                                                                                                          \
    /* uvm_report_warning */                                                                              \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_warning(                 \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_MEDIUM,                                                              \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        ((typename __PSI_TYPE::uvm_sequence_item*)this)->uvm_report_warning(                              \
                id,                                                                                       \
                message,                                                                                  \
                verbosity,                                                                                \
                filename,                                                                                 \
                line,                                                                                     \
                context_name,                                                                             \
                report_enabled_checked                                                                    \
                );                                                                                        \
    }                                                                                                     \
                                                                                                          \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<!IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_warning(                \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_NONE,                                                                \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        uvm::uvm_report_warning(id,message,verbosity,filename,line,context_name,report_enabled_checked);  \
    }                                                                                                     \
                                                                                                          \
    /* uvm_report_error */                                                                                \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_error(                   \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_LOW,                                                                 \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        ((typename __PSI_TYPE::uvm_sequence_item*)this)->uvm_report_error(                                \
                id,                                                                                       \
                message,                                                                                  \
                verbosity,                                                                                \
                filename,                                                                                 \
                line,                                                                                     \
                context_name,                                                                             \
                report_enabled_checked                                                                    \
                );                                                                                        \
    }                                                                                                     \
                                                                                                          \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<!IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_error(                  \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_NONE,                                                                \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        uvm::uvm_report_error(id,message,verbosity,filename,line,context_name,report_enabled_checked);    \
    }                                                                                                     \
                                                                                                          \
    /* uvm_report_fatal */                                                                                \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_fatal(                   \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_NONE,                                                                \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        ((typename __PSI_TYPE::uvm_sequence_item*)this)->uvm_report_fatal(                                \
                id,                                                                                       \
                message,                                                                                  \
                verbosity,                                                                                \
                filename,                                                                                 \
                line,                                                                                     \
                context_name,                                                                             \
                report_enabled_checked                                                                    \
                );                                                                                        \
    }                                                                                                     \
                                                                                                          \
    template<typename __PSI_TYPE = __VA_ARGS__>                                                           \
    typename std::enable_if<!IsSequenceItem<__PSI_TYPE>::result>::type uvm_report_fatal(                  \
            const std::string& id,                                                                        \
            const std::string& message,                                                                   \
            int verbosity = uvm::UVM_NONE,                                                                \
            const std::string& filename = "",                                                             \
            int line = 0,                                                                                 \
            const std::string& context_name = "",                                                         \
            bool report_enabled_checked = false ) const {                                                 \
        uvm::uvm_report_fatal(id,message,verbosity,filename,line,context_name,report_enabled_checked);    \
    }                                                                                                     \


// -------------------------------------------
// MACRO: M_UVM_OBJECT_REGISTRY_INTERNAL
//
// Implementation-defined macro  ----
// -------------------------------------------

#define M_UVM_OBJECT_REGISTRY_INTERNAL_PARAM(...) \
  static const std::string m_register_type_name() { \
    return std::string(#__VA_ARGS__); } \
  typedef ::uvm::uvm_object_registry<__VA_ARGS__ > type_id; \
  static type_id* get_type() { return type_id::get(); }

#define M_UVM_OBJECT_ABSTRACT_REGISTRY_INTERNAL_PARAM(...) \
  static const std::string m_register_type_name() { \
    return std::string(#__VA_ARGS__); } \
  typedef ::uvm::uvm_abstract_object_registry<__VA_ARGS__ > type_id; \
  static type_id* get_type() { return type_id::get(); }

#define M_UVM_OBJECT_GET_OBJECT_TYPE_FUNC \
  virtual const ::uvm::uvm_object_wrapper* get_object_type() const { \
    return type_id::get(); \
  }

// -------------------------------------------
// MACRO: M_UVM_GET_TYPE_NAME_FUNC
//
// Implementation-defined macro
// -------------------------------------------

#define M_UVM_OBJECT_GET_TYPE_NAME_FUNC_PARAM(...) \
  virtual const std::string get_type_name() const { \
     return #__VA_ARGS__; \
  } \
  virtual const char* kind() const { \
    return #__VA_ARGS__; \
  } \
  template <typename M_UVM_T> \
  static auto m_uvm_randomize_named( int ) \
    -> decltype((void)&M_UVM_T::randomize, std::true_type()); \
  template <typename M_UVM_T> \
  static std::false_type m_uvm_randomize_named( ... ); \
  virtual bool m_uvm_randomize() { \
    typedef typename std::remove_pointer<decltype(this)>::type m_uvm_this_t; \
    return ::uvm::m_uvm_randomize_object(this, \
      decltype(m_uvm_randomize_named<m_uvm_this_t>(0))()); \
  } \


#endif /* UVM_OBJECT_DEFINES_H_ */
