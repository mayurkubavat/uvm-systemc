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

// Regression test for uvm_queue<T>::insert() and pop_back().
// insert() places an item before the item at the given index, and
// ignores an index outside the queue with a warning; pop_back()
// removes and returns the last item.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>

using namespace uvm;

static int errors = 0;

template <typename T>
static void show( uvm_queue<T>& q, const std::string& expected )
{
  std::ostringstream s;
  for (int i = 0; i < q.size(); i++)
    s << (i ? " " : "") << q.get(i);
  std::cout << q.get_name() << ": [" << s.str() << "]" << std::endl;
  if (s.str() != expected)
  {
    errors++;
    UVM_ERROR("QUEUE", "expected [" + expected + "]");
  }
}

template <typename T>
static void check( const T& got, const T& expected, const std::string& what )
{
  std::cout << what << " = " << got << std::endl;
  if (got != expected)
  {
    errors++;
    UVM_ERROR("QUEUE", what);
  }
}

int sc_main(int, char*[])
{
  uvm_queue<int> q("q");
  q.push_back(1);
  q.push_back(2);
  q.push_back(3);
  show(q, "1 2 3");

  q.insert(1, 10);
  show(q, "1 10 2 3");
  q.insert(0, 0);
  show(q, "0 1 10 2 3");
  q.insert(4, 20);
  show(q, "0 1 10 2 20 3");

  // out of range: ignored with a warning
  q.insert(6, 99);
  q.insert(-1, 99);
  show(q, "0 1 10 2 20 3");

  check(q.pop_back(), 3, "pop_back");
  check(q.pop_front(), 0, "pop_front");
  check(q.pop_back(), 20, "pop_back");
  show(q, "1 10 2");

  uvm_queue<std::string> s("s");
  s.push_back("b");
  s.insert(0, "a");
  s.push_back("c");
  show(s, "a b c");
  check(s.pop_back(), std::string("c"), "pop_back");
  check(s.pop_back(), std::string("b"), "pop_back");
  check(s.pop_back(), std::string("a"), "pop_back");
  check(s.pop_back(), std::string(""), "pop_back on empty queue");

  if (errors == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
