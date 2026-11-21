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

// Test of uvm_root::die called twice in one process: the exception which
// ends the first die() is caught, and a second fatal error still does not
// return to its caller and prints the report summary again.

#include <systemc>
#include <uvm>

#include <iostream>
#include <stdexcept>
#include <string>

int sc_main(int, char*[])
{
  for (int i = 1; i <= 2; i++)
  {
    try
    {
      UVM_FATAL("TEST", "fatal error " + std::to_string(i));
      std::cout << "error: UVM_FATAL " << i << " returned" << std::endl;
    }
    catch (const std::runtime_error& e)
    {
      std::cout << "caught " << i << ": " << e.what() << std::endl;
    }
  }
  return 0;
}
