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

#include <systemc>
#include <uvm>
#include <crave2uvm.h>

#include "testbench.h"

int sc_main(int, char*[])
{
  // read the settings of CRAVE; the file is created if it does not exist
  crave::init("crave.cfg");

  // use a fixed seed to get the same packets in every run
  crave::set_global_seed(20260101);

  // instantiate testbench
  testbench tb("tb");

  uvm::run_test();

  return 0;
}
