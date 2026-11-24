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

// Regression test for uvm_packer::pack_field() and unpack_field().
// Both take a uvm_bitstream_t, so they must handle values wider than
// 64 bits (up to UVM_MAX_STREAMBITS), in both bit orders, without
// disturbing the fields packed next to them.

#include <systemc>
#include <uvm>

#include <iostream>
#include <string>
#include <vector>

using namespace uvm;

static int errors = 0;

static std::string hex( const uvm_bitstream_t& v )
{
  std::string s = v.to_string(sc_dt::SC_HEX_US, false);
  std::string::size_type pos = s.find_first_not_of('0');
  return (pos == std::string::npos) ? "0" : s.substr(pos);
}

class wide_obj : public uvm::uvm_object
{
 public:
  UVM_OBJECT_UTILS(wide_obj);

  bool big_endian;
  int size;
  uvm_bitstream_t value;
  uvm_integral_t head;
  uvm_integral_t tail;

  wide_obj( const std::string& name = "wide_obj" )
  : uvm::uvm_object(name), big_endian(false), size(0), value(0), head(0), tail(0) {}

  // the bit order is selected through the packer passed in
  virtual void do_pack( uvm::uvm_packer& packer ) const
  {
    packer.big_endian = big_endian;
    packer.pack_field_int(head, 4);
    packer.pack_field(value, size);
    packer.pack_field_int(tail, 8);
  }

  virtual void do_unpack( uvm::uvm_packer& packer )
  {
    packer.big_endian = big_endian;
    head = packer.unpack_field_int(4);
    value = packer.unpack_field(size);
    tail = packer.unpack_field_int(8);
  }
};

static void check( bool big_endian, const uvm_bitstream_t& value, int size )
{
  wide_obj a("a"), b("b");
  a.big_endian = b.big_endian = big_endian;
  a.size = b.size = size;
  a.value = value;
  a.head = 0x9;
  a.tail = 0x5a;

  std::vector<bool> bits;
  int n = a.pack(bits);
  b.unpack(bits);

  uvm_bitstream_t mask = 0;
  for (int i = 0; i < size; i++)
    mask[i] = 1;
  uvm_bitstream_t expected = value & mask;

  // the field follows the 4-bit head in the packed bits, LSB first
  // for little endian and MSB first for big endian
  bool layout = ((int)bits.size() >= size + 12);
  for (int i = 0; layout && i < size; i++)
  {
    bool exp_bit = big_endian ? expected[size-1-i].to_bool() : expected[i].to_bool();
    if (bits[4+i] != exp_bit)
      layout = false;
  }

  bool ok = (n == size + 12) && layout && (b.value == expected) &&
            (b.head == 0x9) && (b.tail == 0x5a);

  std::cout << (big_endian ? "big   " : "little") << " endian, "
            << size << " bits: packed " << n << " bits, value 0x"
            << (size <= 256 ? hex(b.value) : hex(b.value).substr(0, 8) + "...")
            << ", head 0x" << std::hex << b.head.to_uint64()
            << ", tail 0x" << b.tail.to_uint64() << std::dec
            << (ok ? " ok" : " MISMATCH") << std::endl;

  if (!ok)
  {
    errors++;
    UVM_ERROR("PACKFLD", "expected 0x" + hex(expected));
  }
}

int sc_main(int, char*[])
{
  uvm_bitstream_t wide = 0;
  wide = "0x0123456789abcdeffedcba98765432100f1e2d3c4b5a6978";

  for (int be = 0; be < 2; be++)
  {
    check(be, 0xbeef, 16);
    check(be, 0x0123456789abcdefULL, 64);
    check(be, wide, 72);
    check(be, wide, 128);
    check(be, wide, 192);
  }

  uvm_bitstream_t top = 1;
  top <<= (UVM_MAX_STREAMBITS - 1);
  uvm_bitstream_t edges = top | 1;
  check(true, edges, UVM_MAX_STREAMBITS);
  check(false, edges, UVM_MAX_STREAMBITS); // leaves the packer little endian

  if (errors == 0)
    std::cout << "** UVM TEST PASSED **" << std::endl;
  else
    std::cout << "!! UVM TEST FAILED !!" << std::endl;

  return 0;
}
