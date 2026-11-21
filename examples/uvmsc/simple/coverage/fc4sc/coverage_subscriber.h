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

#ifndef COVERAGE_SUBSCRIBER_H_
#define COVERAGE_SUBSCRIBER_H_

#include <sstream>
#include <string>

#include <systemc>
#include <uvm>

#include <fc4sc.hpp>

#include "packet.h"

//----------------------------------------------------------------------
// Class: packet_coverage
//
// FC4SC covergroup which samples the fields of a packet.
//----------------------------------------------------------------------

class packet_coverage : public covergroup
{
 public:
  CG_CONS(packet_coverage)
  {}

  void sample( const packet& p )
  {
    addr = p.addr;
    kind = p.is_write ? 1 : 0;

    covergroup::sample();
  }

 private:
  int addr = 0;
  int kind = 0;

 public:
  COVERPOINT(int, addr_cvp, addr)
  {
    bin<int>("low", interval(0x00, 0x3f)),
    bin<int>("mid", interval(0x40, 0xbf)),
    bin<int>("high", interval(0xc0, 0xff))
  };

  COVERPOINT(int, kind_cvp, kind)
  {
    bin<int>("read", 0),
    bin<int>("write", 1)
  };

  cross<int, int> addr_kind_cross = cross<int, int>(this, "addr_kind_cross",
    &addr_cvp,
    &kind_cvp
  );
};

//----------------------------------------------------------------------
// Class: coverage_subscriber
//
// Subscriber which owns a covergroup, samples it for each packet it
// receives and stores the collected coverage as UCIS XML at the end of
// the test.
//----------------------------------------------------------------------

class coverage_subscriber : public uvm::uvm_subscriber<packet>
{
 public:
  packet_coverage cg;

  coverage_subscriber( uvm::uvm_component_name name )
  : uvm::uvm_subscriber<packet>(name),
    cg(get_full_name()),
    ucis_file("coverage_results.xml")
  {}

  UVM_COMPONENT_UTILS(coverage_subscriber);

  virtual void build_phase( uvm::uvm_phase& phase )
  {
    uvm::uvm_subscriber<packet>::build_phase(phase);

    uvm::uvm_config_db<std::string>::get(this, "", "ucis_file", ucis_file);
  }

  virtual void write( const packet& p )
  {
    cg.sample(p);
  }

  virtual void report_phase( uvm::uvm_phase& phase )
  {
    std::ostringstream str;
    str << "Coverage of " << cg.name() << ": "
        << cg.get_inst_coverage() << "%"
        << " (addr_cvp " << cg.addr_cvp.get_inst_coverage() << "%,"
        << " kind_cvp " << cg.kind_cvp.get_inst_coverage() << "%,"
        << " addr_kind_cross " << cg.addr_kind_cross.get_inst_coverage() << "%)";
    UVM_INFO("COVERAGE", str.str(), uvm::UVM_LOW);

    // The UCIS database holds all covergroups registered with FC4SC,
    // not just the one of this subscriber.
    xml_printer::coverage_save(ucis_file);
    UVM_INFO("COVERAGE", "UCIS coverage database saved to " + ucis_file,
      uvm::UVM_LOW);
  }

 private:
  std::string ucis_file;
};

#endif /* COVERAGE_SUBSCRIBER_H_ */
