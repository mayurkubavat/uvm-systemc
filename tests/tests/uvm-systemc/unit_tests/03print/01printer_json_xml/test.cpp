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

// Unit test for IEEE 1800.2-2020 uvm_printer (uvm_policy inheritance,
// element tree, copy ctor, UVM_PRINT field_op), uvm_printer_element,
// uvm_printer_element_proxy, uvm_json_printer, and uvm_xml_printer.

#include <systemc>
#include <uvm>

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace uvm;

class elem_visitor : public uvm_visitor<uvm_printer_element>
{
 public:
  std::vector<std::string> visited_names;

  explicit elem_visitor( const std::string& name = "elem_visitor" )
  : uvm_visitor<uvm_printer_element>(name) {}

  virtual void visit( uvm_printer_element* node )
  {
    if (node != nullptr)
      visited_names.push_back(node->get_element_name());
  }
};

class prt_sub_obj : public uvm_object
{
 public:
  int code;
  std::string note;

  explicit prt_sub_obj( const std::string& name = "prt_sub_obj" )
  : uvm_object(name), code(0), note("") {}
  virtual ~prt_sub_obj() {}

  virtual void do_print( const uvm_printer& printer ) const
  {
    printer.print_field_int("code", code, 32, UVM_DEC);
    printer.print_string("note", note);
  }

  UVM_OBJECT_UTILS(prt_sub_obj);
};

class prt_packet : public uvm_object
{
 public:
  int addr;
  std::string empty_str;
  std::string special_str;
  std::vector<int> data;
  std::vector<int> empty_arr;
  prt_sub_obj* sub;
  prt_packet* next;
  bool throw_in_print;

  explicit prt_packet( const std::string& name = "prt_packet" )
  : uvm_object(name),
    addr(0),
    empty_str(""),
    special_str(""),
    sub(nullptr),
    next(nullptr),
    throw_in_print(false) {}
  virtual ~prt_packet() {}

  virtual void do_print( const uvm_printer& printer ) const
  {
    if (throw_in_print)
      throw std::runtime_error("boom_print");

    printer.print_field_int("addr", addr, 32, UVM_HEX);
    printer.print_string("empty_str", empty_str);
    printer.print_string("special_str", special_str);

    printer.print_array_header("data", static_cast<int>(data.size()), "da(int)");
    for (std::size_t i = 0; i < data.size(); ++i)
    {
      std::ostringstream idx;
      idx << "[" << i << "]";
      printer.print_field_int(idx.str(), data[i], 32, UVM_DEC);
    }
    if (data.size() > 3)
      printer.print_array_range(1, 2);
    printer.print_array_footer(static_cast<int>(data.size()));

    printer.print_array_header("empty_arr", 0, "da(int)");
    printer.print_array_footer(0);

    printer.print_array_header("assoc", 2, "aa(int)");
    printer.print_field_int("[key_a]", 10, 32, UVM_DEC);
    printer.print_field_int("[key_b]", 20, 32, UVM_DEC);
    printer.print_array_footer(2);

    if (sub != nullptr)
      printer.print_object("sub", *sub);
    if (next != nullptr)
      printer.print_object("next", *next);
  }

  UVM_OBJECT_UTILS(prt_packet);
};

class prt_op_item : public uvm_object
{
 public:
  int f1;
  int f2;
  mutable int exec_count;
  mutable int do_print_count;
  bool disable_hook;

  explicit prt_op_item( const std::string& name = "prt_op_item" )
  : uvm_object(name),
    f1(1),
    f2(2),
    exec_count(0),
    do_print_count(0),
    disable_hook(false) {}
  virtual ~prt_op_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op == nullptr || op->get_op_type() != UVM_PRINT)
      return;
    exec_count++;
    uvm_printer* p = dynamic_cast<uvm_printer*>(op->get_policy());
    if (p != nullptr)
      p->print_field_int("f1", f1, 32, UVM_DEC);
    if (disable_hook)
      op->disable_user_hook();
  }

  virtual void do_print( const uvm_printer& printer ) const
  {
    do_print_count++;
    printer.print_field_int("f2", f2, 32, UVM_DEC);
  }

  UVM_OBJECT_UTILS(prt_op_item);
};

class prt_elem_catcher : public uvm_report_catcher
{
 public:
  int prntelem_warn_count;

  prt_elem_catcher() : prntelem_warn_count(0) {}

  virtual action_e do_catch()
  {
    if (get_id() == "PRNTELEM" && get_severity() == UVM_WARNING)
    {
      prntelem_warn_count++;
      return CAUGHT;
    }
    return THROW;
  }
};

class prt_inner_arr_throw : public uvm_object
{
 public:
  explicit prt_inner_arr_throw( const std::string& name = "prt_inner_arr_throw" )
  : uvm_object(name) {}
  virtual ~prt_inner_arr_throw() {}

  virtual void do_print( const uvm_printer& printer ) const
  {
    printer.print_array_header("bad_arr", 1, "da(int)");
    throw std::runtime_error("boom_in_arr");
  }

  UVM_OBJECT_UTILS(prt_inner_arr_throw);
};

class prt_outer_catch : public uvm_object
{
 public:
  int before;
  int after_catch;
  prt_inner_arr_throw* inner;
  mutable bool caught_inner;

  explicit prt_outer_catch( const std::string& name = "prt_outer_catch" )
  : uvm_object(name), before(1), after_catch(2), inner(nullptr), caught_inner(false) {}
  virtual ~prt_outer_catch() {}

  virtual void do_print( const uvm_printer& printer ) const
  {
    printer.print_field_int("before", before, 32, UVM_DEC);
    if (inner != nullptr)
    {
      try
      {
        printer.print_object("in", *inner);
      }
      catch (const std::runtime_error&)
      {
        caught_inner = true;
      }
    }
    printer.print_field_int("after_catch", after_catch, 32, UVM_DEC);
  }

  UVM_OBJECT_UTILS(prt_outer_catch);
};

class printer_json_xml_test : public uvm_test
{
 public:
  bool all_ok;

  explicit printer_json_xml_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(printer_json_xml_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("PRINT_TEST", "FAILED: " + desc);
    }
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // 1. uvm_policy inheritance, abstractness, and factory registration
    check(std::is_base_of<uvm_policy, uvm_printer>::value,
          "uvm_printer inherits uvm_policy");
    check(!std::is_constructible<uvm_printer>::value,
          "uvm_printer is not directly default constructible");
    check(uvm_printer::get_type() == uvm_abstract_object_registry<uvm_printer>::get(),
          "uvm_printer uses abstract object registry");
    check(uvm_printer_element::get_type() != nullptr, "uvm_printer_element registered");
    check(uvm_printer_element_proxy::get_type() != nullptr, "uvm_printer_element_proxy registered");
    check(uvm_table_printer::get_type() != nullptr, "uvm_table_printer registered");
    check(uvm_tree_printer::get_type() != nullptr, "uvm_tree_printer registered");
    check(uvm_line_printer::get_type() != nullptr, "uvm_line_printer registered");
    check(uvm_json_printer::get_type() != nullptr, "uvm_json_printer registered");
    check(uvm_xml_printer::get_type() != nullptr, "uvm_xml_printer registered");

    check(uvm_printer::get_default() == uvm_default_printer,
          "uvm_printer::get_default() == uvm_default_printer");

    // 2. uvm_printer_element API + uvm_printer_element_proxy visitor traversal
    {
      uvm_printer_element root("root");
      root.set("root", "my_type", "-", "val0");
      check(root.get_element_name() == "root" &&
            root.get_element_type_name() == "my_type" &&
            root.get_element_size() == "-" &&
            root.get_element_value() == "val0",
            "uvm_printer_element set/get methods");

      uvm_printer_element* c1 = new uvm_printer_element("c1");
      c1->set("c1", "int", "32", "42");
      uvm_printer_element* c2 = new uvm_printer_element("c2");
      c2->set("c2", "obj", "-", "-");
      uvm_printer_element* gc = new uvm_printer_element("gc");
      gc->set("gc", "string", "2", "hi");
      c2->add_child(gc);
      root.add_child(c1);
      root.add_child(c2);

      check(root.has_children(), "root has_children()");
      check(gc->get_parent() == c2 && c2->get_parent() == &root, "get_parent() chain");

      std::vector<uvm_printer_element*> imm, rec;
      root.get_children(imm, false);
      root.get_children(rec, true);
      check(imm.size() == 2 && imm[0] == c1 && imm[1] == c2,
            "get_children(recurse=false)");
      check(rec.size() == 3 && rec[0] == c1 && rec[1] == c2 && rec[2] == gc,
            "get_children(recurse=true)");

      uvm_printer_element_proxy proxy("proxy");
      elem_visitor vis("vis");
      uvm_top_down_visitor_adapter<uvm_printer_element> adapter("adapter");
      adapter.accept(&root, &vis, &proxy);
      check(vis.visited_names.size() == 4 &&
            vis.visited_names[0] == "root" &&
            vis.visited_names[1] == "c1" &&
            vis.visited_names[2] == "c2" &&
            vis.visited_names[3] == "gc",
            "uvm_printer_element_proxy works with uvm_top_down_visitor_adapter");

      root.clear_children();
      check(!root.has_children(), "clear_children() empties root");
    }

    // 3. Manual push_element/pop_element + printer copy ctor + uvm_report_message_element_container
    {
      uvm_table_printer tp1("tp1");
      tp1.push_element("top", "top_t", "-", "-");
      check(tp1.get_top_element() != nullptr && tp1.get_bottom_element() != nullptr &&
            tp1.get_top_element() == tp1.get_bottom_element(),
            "single pushed element is both top and bottom");
      tp1.push_element("leaf", "int", "32", "99");
      check(tp1.get_top_element()->get_element_name() == "top", "top remains top");
      check(tp1.get_bottom_element()->get_element_name() == "leaf", "bottom is leaf");
      tp1.pop_element();
      check(tp1.get_bottom_element()->get_element_name() == "top", "pop restores bottom");

      // Copy-construct uvm_printer while elements exist (must deep-clone without double free)
      uvm_table_printer tp2(tp1);
      tp1.pop_element();
      tp2.pop_element();
      std::string out1 = tp1.emit();
      std::string out2 = tp2.emit();
      check(out1.find("leaf") != std::string::npos && out2.find("leaf") != std::string::npos,
            "copy-constructed printer preserves rows and element tree safely");

      // Exercise uvm_report_message_element_container::do_print (uvm_printer lprinter = printer;)
      uvm_report_message_element_container cont("cont");
      std::string cont_s = cont.sprint();
      check(!cont_s.empty(),
            "uvm_report_message_element_container::sprint() works with uvm_printer copy");
    }

    // 4. uvm_object::sprint() UVM_PRINT uvm_field_op hook
    {
      prt_op_item item("item");
      uvm_json_printer jp("jp");
      std::string s1 = item.sprint(&jp);
      check(item.exec_count == 1 && item.do_print_count == 1,
            "UVM_PRINT invokes do_execute_op and do_print");
      check(s1.find("\"f1\"") != std::string::npos && s1.find("\"f2\"") != std::string::npos,
            "both do_execute_op and do_print fields emitted");

      item.disable_hook = true;
      item.exec_count = 0;
      item.do_print_count = 0;
      std::string s2 = item.sprint(&jp);
      check(item.exec_count == 1 && item.do_print_count == 0,
            "disable_user_hook skips do_print");
      check(s2.find("\"f1\"") != std::string::npos && s2.find("\"f2\"") == std::string::npos,
            "only do_execute_op field emitted when hook disabled");
    }

    // 5. uvm_json_printer & uvm_xml_printer formatting, escaping, empty strings, arrays, cycles
    {
      uvm_json_printer* jdef = uvm_json_printer::get_default();
      check(jdef != nullptr && uvm_default_json_printer == jdef,
            "uvm_json_printer::get_default() non-null");
      uvm_xml_printer* xdef = uvm_xml_printer::get_default();
      check(xdef != nullptr && uvm_default_xml_printer == xdef,
            "uvm_xml_printer::get_default() non-null");

      prt_sub_obj sub("sub");
      sub.code = 55;
      sub.note = "sub<note>&\"ok\"";

      prt_packet pkt("pkt");
      pkt.addr = 0x1A2B;
      pkt.empty_str = "";
      pkt.special_str = std::string("a\"b\\c\nd\te\x01<tag>&'q'");
      pkt.data.push_back(10);
      pkt.data.push_back(20);
      pkt.data.push_back(30);
      pkt.data.push_back(40);
      pkt.sub = &sub;

      // JSON compact (indent=0) and pretty (indent=2)
      uvm_json_printer jp("jp");
      jp.knobs.indent = 0;
      std::string j_compact = pkt.sprint(&jp);
      check(j_compact.find('\n') == std::string::npos, "JSON indent=0 is single-line");
      check(j_compact.find("\"pkt\":{") != std::string::npos, "JSON root object key");
      check(j_compact.find("\"empty_str\":\"\"") != std::string::npos,
            "JSON empty_str is raw empty string, not quoted \"\\\"\\\"\"");
      check(j_compact.find("\\\"b\\\\c\\nd\\te\\u0001") != std::string::npos,
            "JSON RFC 8259 escapes quotes, backslash, newline, tab, control char");
      check(j_compact.find("\"data\":[") != std::string::npos, "JSON array syntax");
      check(j_compact.find("\"empty_arr\":[]") != std::string::npos, "JSON empty array []");
      check(j_compact.find("\"assoc\":[{\"key_a\":\"10\"},{\"key_b\":\"20\"}]") != std::string::npos,
            "JSON associative array preserves keys");
      check(j_compact.find("\"sub\":{\"code\":\"55\"") != std::string::npos,
            "JSON nested object");

      jp.knobs.indent = 2;
      jp.knobs.type_name = true;
      std::string j_pretty = pkt.sprint(&jp);
      check(j_pretty.find('\n') != std::string::npos &&
            j_pretty.find("\"__type__\"") != std::string::npos,
            "JSON pretty indent + type_name knob");

      // XML printer
      uvm_xml_printer xp("xp");
      std::string x_out = pkt.sprint(&xp);
      check(x_out.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") == 0,
            "XML starts with declaration");
      check(x_out.find("<object name=\"pkt\">") != std::string::npos, "XML object tag");
      check(x_out.find("<field name=\"empty_str\" value=\"\"/>") != std::string::npos,
            "XML empty_str is empty attribute, not \"\"");
      check(x_out.find("&quot;b\\c&#xA;d&#x9;e?&lt;tag&gt;&amp;&apos;q&apos;") != std::string::npos,
            "XML escapes special chars and sanitizes control char");
      check(x_out.find("<array name=\"empty_arr\"/>") != std::string::npos,
            "XML empty array tag");
      check(x_out.find("<range value=\"...\"/>") != std::string::npos,
            "XML array range tag");
      check(x_out.find("</object>") != std::string::npos, "XML closing object tag");

      // Multi-root emit (2 print_object calls before emit)
      uvm_json_printer jp_multi("jp_multi");
      jp_multi.knobs.indent = 0;
      jp_multi.print_object("s1", sub);
      jp_multi.print_object("s2", sub);
      std::string j_multi = jp_multi.emit();
      check(!j_multi.empty() && j_multi.front() == '[' && j_multi.back() == ']',
            "JSON multi-root wraps roots in array");

      uvm_xml_printer xp_multi("xp_multi");
      xp_multi.print_object("s1", sub);
      xp_multi.print_object("s2", sub);
      std::string x_multi = xp_multi.emit();
      check(x_multi.find("<uvm_printer>") != std::string::npos &&
            x_multi.find("</uvm_printer>") != std::string::npos,
            "XML multi-root wraps in <uvm_printer>");

      // Cycle & exception safety in print_object
      prt_packet c1("c1"), c2("c2");
      c1.next = &c2;
      c2.next = &c1;
      uvm_json_printer jp_cyc("jp_cyc");
      std::string j_cyc = c1.sprint(&jp_cyc);
      check(j_cyc.find("\"c1\"") != std::string::npos &&
            j_cyc.find("\"next\"") != std::string::npos,
            "cyclic print_object terminates cleanly");
      check(jp_cyc.get_active_object() == nullptr, "active object stack clean after cyclic print");

      prt_packet thr("thr");
      thr.throw_in_print = true;
      bool caught = false;
      try
      {
        (void)thr.sprint(&jp_cyc);
      }
      catch (const std::runtime_error&)
      {
        caught = true;
      }
      check(caught, "sprint propagates exception");
      check(jp_cyc.get_active_object() == nullptr && jp_cyc.get_top_element() == nullptr,
            "exception in do_print cleans up active stack and elements");
    }

    // 6. Nested do_print throwing mid-print_array_header caught by outer do_print
    {
      prt_inner_arr_throw inner("inner");
      prt_outer_catch out("out");
      out.inner = &inner;

      uvm_json_printer jp_nest("jp_nest");
      jp_nest.knobs.indent = 0;
      std::string j_nest = out.sprint(&jp_nest);
      check(out.caught_inner, "outer do_print caught inner array throw");
      check(j_nest == "{\"out\":{\"before\":\"1\",\"in\":{\"bad_arr\":[]},\"after_catch\":\"2\"}}",
            "nested print_array_header throw restores outer element/array/scope stack in JSON");
      check(jp_nest.get_active_object() == nullptr && jp_nest.get_bottom_element() == nullptr,
            "nested throw leaves printer active/element stack balanced after sprint");

      out.caught_inner = false;
      uvm_xml_printer xp_nest("xp_nest");
      std::string x_nest = out.sprint(&xp_nest);
      std::size_t pos_close_in = x_nest.find("</object>");
      std::size_t pos_after = x_nest.find("name=\"after_catch\"");
      check(out.caught_inner && pos_close_in != std::string::npos &&
            pos_after != std::string::npos && pos_close_in < pos_after,
            "nested throw closes inner <object> before outer after_catch in XML");
    }

    // 7. uvm_printer_element::add_child() ancestor cycle guard
    {
      prt_elem_catcher elem_ctchr;
      uvm_report_cb::add(nullptr, &elem_ctchr);

      uvm_printer_element* ea = new uvm_printer_element("ea");
      uvm_printer_element* eb = new uvm_printer_element("eb");
      ea->add_child(ea);
      ea->add_child(eb);
      eb->add_child(ea);
      check(elem_ctchr.prntelem_warn_count == 2,
            "self-cycle and ancestor-cycle add_child emit PRNTELEM warnings");
      check(ea->get_parent() == nullptr && eb->get_parent() == ea &&
            !eb->has_children(),
            "cycle-creating add_child rejected safely");
      delete ea;

      uvm_report_cb::do_delete(nullptr, &elem_ctchr);
    }

    std::cout << "printer_json_xml_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);
    phase.drop_objection(this);
  }

  virtual void report_phase( uvm_phase& phase )
  {
    uvm_test::report_phase(phase);
    uvm_report_server* svr = uvm_coreservice_t::get()->get_report_server();
    if (all_ok &&
        svr->get_severity_count(UVM_WARNING) == 0 &&
        svr->get_severity_count(UVM_ERROR) == 0 &&
        svr->get_severity_count(UVM_FATAL) == 0)
      std::cout << "** UVM TEST PASSED **" << std::endl;
    else
      std::cout << "!! UVM TEST FAILED !!" << std::endl;
  }
};

int sc_main(int, char*[])
{
  run_test("printer_json_xml_test");
  return 0;
}
