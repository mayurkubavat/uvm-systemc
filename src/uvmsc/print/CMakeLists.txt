target_sources (uvm-systemc PRIVATE
  uvm_json_printer.cpp
  uvm_printer.cpp
  uvm_printer_element.cpp
  uvm_printer_globals.cpp
  uvm_table_printer.cpp
  uvm_tree_printer.cpp
  uvm_xml_printer.cpp
  uvm_json_printer.h
  uvm_line_printer.h
  uvm_print.h
  uvm_printer.h
  uvm_printer_element.h
  uvm_printer_globals.h
  uvm_printer_knobs.h
  uvm_table_printer.h
  uvm_tree_printer.h
  uvm_xml_printer.h)

install_headers (
  uvm_json_printer.h
  uvm_line_printer.h
  uvm_print.h
  uvm_printer.h
  uvm_printer_element.h
  uvm_printer_globals.h
  uvm_printer_knobs.h
  uvm_table_printer.h
  uvm_tree_printer.h
  uvm_xml_printer.h)
