target_sources (uvm-systemc PRIVATE
  uvm_tlm_gp.cpp
  uvm_tlm_time.cpp
  uvm_tlm2.h
  uvm_tlm2_defines.h
  uvm_tlm2_ifs.h
  uvm_tlm2_imps.h
  uvm_tlm_b_sockets.h
  uvm_tlm_gp.h
  uvm_tlm_nb_sockets.h
  uvm_tlm_sc_bridges.h
  uvm_tlm_time.h)

install_headers (
  uvm_tlm2.h
  uvm_tlm2_defines.h
  uvm_tlm2_ifs.h
  uvm_tlm2_imps.h
  uvm_tlm_b_sockets.h
  uvm_tlm_gp.h
  uvm_tlm_nb_sockets.h
  uvm_tlm_sc_bridges.h
  uvm_tlm_time.h)
