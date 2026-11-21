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

// Deleting a component that owns its own uvm_domain must unlink the domain
// and its terminal/schedule nodes from the common phase graph and domain
// registry, so subsequent phase lookups and run_test() do not read freed
// uvm_phase nodes.

#include <iostream>

#include <systemc>
#include <uvm>

using namespace uvm;

class owned_domain_comp : public uvm_component {
public:
  uvm_domain dom;

  owned_domain_comp(uvm_component_name name, const std::string& dom_name)
      : uvm_component(name), dom(dom_name) {
    set_domain(dom);
  }

  virtual void main_phase(uvm_phase& phase) {
    phase.raise_objection(this);
    sc_core::wait(10, sc_core::SC_NS);
    UVM_INFO("MAIN", "main phase of " + get_full_name() + " done", UVM_NONE);
    phase.drop_objection(this);
  }
};

int sc_main(int, char*[]) {
  // Create and delete a component that owns its own phase domain before run_test().
  owned_domain_comp* transient = new owned_domain_comp("transient", "transient_dom");
  delete transient;

  uvm_domain::domains_mapT domains;
  uvm_domain::get_domains(domains);
  std::cout << "transient_dom_registered=" << domains.count("transient_dom") << std::endl;

  // Trigger m_find_successor across the common domain graph after deletion.
  uvm_domain* common = uvm_domain::get_common_domain();
  uvm_phase* extract_node = common->find(uvm_extract_phase::get(), false);
  std::cout << "extract_found=" << (extract_node != nullptr ? 1 : 0) << std::endl;

  // Keep a surviving component with its own domain to run through all phases.
  owned_domain_comp kept("kept", "kept_dom");
  run_test();

  return 0;
}
