target_sources (uvm-systemc PRIVATE
  uvm_comparer.cpp
  uvm_links.cpp
  uvm_packer.cpp
  uvm_recorder.cpp
  uvm_tr_database.cpp
  uvm_tr_stream.cpp
  uvm_comparer.h
  uvm_links.h
  uvm_packer.h
  uvm_policy.h
  uvm_recorder.h
  uvm_tr_database.h
  uvm_tr_stream.h)

install_headers (
  uvm_comparer.h
  uvm_links.h
  uvm_packer.h
  uvm_policy.h
  uvm_recorder.h
  uvm_tr_database.h
  uvm_tr_stream.h)
