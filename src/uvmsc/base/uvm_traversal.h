//----------------------------------------------------------------------
//   Copyright 2010-2018 Cadence Design Systems, Inc.
//   Copyright 2014-2024 NVIDIA Corporation
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

#ifndef UVM_TRAVERSAL_H_
#define UVM_TRAVERSAL_H_

#include <string>
#include <vector>

#include "uvmsc/base/uvm_object.h"

namespace uvm {

// forward class declarations
class uvm_component;
class uvm_root;

//----------------------------------------------------------------------
// CLASS: uvm_visitor<NODE>
//
//! The class uvm_visitor provides an abstract base class for a visitor.
//! The visitor visits instances of type NODE. For general information
//! regarding the visitor pattern see
//! http://en.wikipedia.org/wiki/Visitor_pattern
//!
//! As in the SystemVerilog API, where NODE is a class handle, the
//! member function visit takes a pointer to the node.
//----------------------------------------------------------------------

template <typename NODE = uvm_component>
class uvm_visitor : public uvm_object
{
 public:
  //! The constructor shall create a new visitor with the given name.
  explicit uvm_visitor( const std::string& name = "" )
  : uvm_object(name) {}

  //! This member function will be invoked by the visitor before the first
  //! NODE is visited.
  virtual void begin_v() {}

  //! This member function will be invoked by the visitor after the last
  //! node is visited.
  virtual void end_v() {}

  //! This member function will be invoked by the visitor for every visited
  //! node of the provided structure. The user is expected to provide the
  //! own functionality in this function.
  virtual void visit( NODE* node ) = 0;

  virtual ~uvm_visitor() {}
}; // class uvm_visitor

//----------------------------------------------------------------------
// CLASS: uvm_structure_proxy<STRUCTURE>
//
//! The uvm_structure_proxy is a wrapper and provides a set of elements of
//! the STRUCTURE to the caller on demand. This is to decouple the
//! retrieval of the STRUCTUREs subelements from the actual function being
//! invoked on STRUCTURE.
//----------------------------------------------------------------------

template <typename STRUCTURE = uvm_component>
class uvm_structure_proxy : public uvm_object
{
 public:
  //! The constructor shall create a new structure proxy with the given
  //! name.
  explicit uvm_structure_proxy( const std::string& name = "" )
  : uvm_object(name) {}

  //! This member function shall append the immediate children of the
  //! element s to the vector children.
  virtual void get_immediate_children( STRUCTURE* s,
                                       std::vector<STRUCTURE*>& children ) = 0;

  virtual ~uvm_structure_proxy() {}
}; // class uvm_structure_proxy

//----------------------------------------------------------------------
// CLASS: uvm_visitor_adapter<STRUCTURE,VISITOR>
//
//! The visitor adapter traverses the structure STRUCTURE and calls the
//! member function visit of the visitor VISITOR for each node of the
//! structure. The traversal order is defined by the derived classes.
//----------------------------------------------------------------------

template <typename STRUCTURE = uvm_component,
          typename VISITOR = uvm_visitor<STRUCTURE> >
class uvm_visitor_adapter : public uvm_object
{
 public:
  //! The constructor shall create a new visitor adapter with the given
  //! name.
  explicit uvm_visitor_adapter( const std::string& name = "" )
  : uvm_object(name) {}

  //! This member function shall traverse the structure s, using the
  //! proxy p to retrieve the children of each node, and call the member
  //! function visit of visitor v for each node. If invoke_begin_end is
  //! true, the member functions begin_v and end_v of the visitor are
  //! called before and after the traversal.
  virtual void accept( STRUCTURE* s,
                       VISITOR* v,
                       uvm_structure_proxy<STRUCTURE>* p,
                       bool invoke_begin_end = true ) = 0;

  virtual ~uvm_visitor_adapter() {}
}; // class uvm_visitor_adapter

//----------------------------------------------------------------------
// CLASS: uvm_top_down_visitor_adapter<STRUCTURE,VISITOR>
//
//! This uvm_top_down_visitor_adapter traverses the STRUCTURE s (and will
//! invoke the visitor) in a hierarchical fashion. During traversal s will
//! be visited before all subnodes of s will be visited.
//----------------------------------------------------------------------

template <typename STRUCTURE = uvm_component,
          typename VISITOR = uvm_visitor<STRUCTURE> >
class uvm_top_down_visitor_adapter
: public uvm_visitor_adapter<STRUCTURE, VISITOR>
{
 public:
  explicit uvm_top_down_visitor_adapter( const std::string& name = "" )
  : uvm_visitor_adapter<STRUCTURE, VISITOR>(name) {}

  virtual void accept( STRUCTURE* s,
                       VISITOR* v,
                       uvm_structure_proxy<STRUCTURE>* p,
                       bool invoke_begin_end = true )
  {
    std::vector<STRUCTURE*> c;

    if (invoke_begin_end)
      v->begin_v();

    v->visit(s);
    p->get_immediate_children(s, c);

    for (typename std::vector<STRUCTURE*>::iterator it = c.begin();
         it != c.end(); ++it)
      accept(*it, v, p, false);

    if (invoke_begin_end)
      v->end_v();
  }

  virtual ~uvm_top_down_visitor_adapter() {}
}; // class uvm_top_down_visitor_adapter

//----------------------------------------------------------------------
// CLASS: uvm_bottom_up_visitor_adapter<STRUCTURE,VISITOR>
//
//! This uvm_bottom_up_visitor_adapter traverses the STRUCTURE s (and will
//! invoke the visitor) in a hierarchical fashion. During traversal all
//! children of node s will be visited before s will be visited.
//----------------------------------------------------------------------

template <typename STRUCTURE = uvm_component,
          typename VISITOR = uvm_visitor<STRUCTURE> >
class uvm_bottom_up_visitor_adapter
: public uvm_visitor_adapter<STRUCTURE, VISITOR>
{
 public:
  explicit uvm_bottom_up_visitor_adapter( const std::string& name = "" )
  : uvm_visitor_adapter<STRUCTURE, VISITOR>(name) {}

  virtual void accept( STRUCTURE* s,
                       VISITOR* v,
                       uvm_structure_proxy<STRUCTURE>* p,
                       bool invoke_begin_end = true )
  {
    std::vector<STRUCTURE*> c;

    if (invoke_begin_end)
      v->begin_v();

    p->get_immediate_children(s, c);

    for (typename std::vector<STRUCTURE*>::iterator it = c.begin();
         it != c.end(); ++it)
      accept(*it, v, p, false);

    v->visit(s);

    if (invoke_begin_end)
      v->end_v();
  }

  virtual ~uvm_bottom_up_visitor_adapter() {}
}; // class uvm_bottom_up_visitor_adapter

//----------------------------------------------------------------------
// CLASS: uvm_by_level_visitor_adapter<STRUCTURE,VISITOR>
//
//! This uvm_by_level_visitor_adapter traverses the STRUCTURE s (and will
//! invoke the visitor) in a hierarchical fashion. During traversal will
//! visit all direct children of s before all grand-children are visited.
//----------------------------------------------------------------------

template <typename STRUCTURE = uvm_component,
          typename VISITOR = uvm_visitor<STRUCTURE> >
class uvm_by_level_visitor_adapter
: public uvm_visitor_adapter<STRUCTURE, VISITOR>
{
 public:
  explicit uvm_by_level_visitor_adapter( const std::string& name = "" )
  : uvm_visitor_adapter<STRUCTURE, VISITOR>(name) {}

  virtual void accept( STRUCTURE* s,
                       VISITOR* v,
                       uvm_structure_proxy<STRUCTURE>* p,
                       bool invoke_begin_end = true )
  {
    std::vector<STRUCTURE*> c;
    c.push_back(s);

    if (invoke_begin_end)
      v->begin_v();

    while (!c.empty())
    {
      std::vector<STRUCTURE*> q;
      for (typename std::vector<STRUCTURE*>::iterator it = c.begin();
           it != c.end(); ++it)
      {
        v->visit(*it);
        // a fresh vector for each node, as in UVM-SV, so that a proxy
        // which assigns the children instead of appending them works
        std::vector<STRUCTURE*> t;
        p->get_immediate_children(*it, t);
        q.insert(q.end(), t.begin(), t.end());
      }
      c.swap(q);
    }

    if (invoke_begin_end)
      v->end_v();
  }

  virtual ~uvm_by_level_visitor_adapter() {}
}; // class uvm_by_level_visitor_adapter

//----------------------------------------------------------------------
// CLASS: uvm_component_proxy
//
//! The class uvm_component_proxy provides the immediate children of a
//! component, as returned by the member function
//! uvm_component::get_children.
//----------------------------------------------------------------------

class uvm_component_proxy : public uvm_structure_proxy<uvm_component>
{
 public:
  explicit uvm_component_proxy( const std::string& name = "" );

  virtual void get_immediate_children( uvm_component* s,
                                       std::vector<uvm_component*>& children );

  virtual const std::string get_type_name() const;

  virtual ~uvm_component_proxy();
}; // class uvm_component_proxy

//----------------------------------------------------------------------
// CLASS: uvm_component_name_check_visitor
//
//! This specialized visitor analyzes the naming of the current component.
//! The established rule set ensures that a component name is usable in
//! hierarchical names and in regular expressions. Names which violate
//! the rules result in a warning with the message id UVM/COMP/NAME.
//! The visitor does not check the name of the uvm_root instance.
//----------------------------------------------------------------------

class uvm_component_name_check_visitor : public uvm_visitor<uvm_component>
{
 public:
  explicit uvm_component_name_check_visitor( const std::string& name = "" );

  //! This member function shall return the regular expression, which
  //! the name of each visited component shall match.
  virtual std::string get_name_constraint() const;

  virtual void visit( uvm_component* node );

  virtual void begin_v();

  virtual const std::string get_type_name() const;

  virtual ~uvm_component_name_check_visitor();

 private:
  uvm_root* m_root;
}; // class uvm_component_name_check_visitor

} // namespace uvm

#endif // UVM_TRAVERSAL_H_
