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

// Unit test for IEEE 1800.2-2020 uvm_packer:
// uvm_policy inheritance, factory registration, get_default/set_default,
// flush, recursion policy, get_packed_*/set_packed_* (bits/bytes/ints/longints),
// zero-bit & partial-word stale-bit safety, endianness symmetry,
// pack_object_with_meta/unpack_object_with_meta, is_null bounds check,
// cycle detection, exception safety, and uvm_field_op (UVM_PACK/UVM_UNPACK).

#include <systemc>
#include <uvm>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace uvm;

static bool g_packer_default_nonnull_before_sc_main = (uvm_default_packer != nullptr);

class packer_msg_catcher : public uvm_report_catcher
{
 public:
  int cycfnd_count;
  int pckszerr_count;
  int unpckerr_count;

  packer_msg_catcher()
  : cycfnd_count(0), pckszerr_count(0), unpckerr_count(0) {}

  virtual action_e do_catch()
  {
    const std::string id = get_id();
    if (id == "CYCFND" && get_severity() == UVM_WARNING)
    {
      cycfnd_count++;
      return CAUGHT;
    }
    if (id == "PCKSZERR" && get_severity() == UVM_ERROR)
    {
      pckszerr_count++;
      return CAUGHT;
    }
    if (id == "UNPCKERR" && get_severity() == UVM_ERROR)
    {
      unpckerr_count++;
      return CAUGHT;
    }
    return THROW;
  }
};

class pck_leaf : public uvm_object
{
 public:
  int a;
  std::string s;

  explicit pck_leaf( const std::string& name = "pck_leaf" )
  : uvm_object(name), a(0), s("") {}
  virtual ~pck_leaf() {}

  virtual void do_pack( uvm_packer& packer ) const
  {
    packer.pack_field_int(a, 32);
    packer.pack_string(s);
  }

  virtual void do_unpack( uvm_packer& packer )
  {
    a = static_cast<int>(packer.unpack_field_int(32).to_int());
    s = packer.unpack_string();
  }

  UVM_OBJECT_UTILS(pck_leaf);
};

class pck_leaf_derived : public pck_leaf
{
 public:
  int extra;

  explicit pck_leaf_derived( const std::string& name = "pck_leaf_derived" )
  : pck_leaf(name), extra(0) {}
  virtual ~pck_leaf_derived() {}

  virtual void do_pack( uvm_packer& packer ) const
  {
    pck_leaf::do_pack(packer);
    packer.pack_field_int(extra, 16);
  }

  virtual void do_unpack( uvm_packer& packer )
  {
    pck_leaf::do_unpack(packer);
    extra = static_cast<int>(packer.unpack_field_int(16).to_int());
  }

  UVM_OBJECT_UTILS(pck_leaf_derived);
};

class pck_holder : public uvm_object
{
 public:
  int hdr;
  pck_leaf* child;

  explicit pck_holder( const std::string& name = "pck_holder" )
  : uvm_object(name), hdr(0), child(nullptr) {}
  virtual ~pck_holder() {}

  virtual void do_pack( uvm_packer& packer ) const
  {
    packer.pack_field_int(hdr, 16);
    packer.pack_object_with_meta(child);
  }

  virtual void do_unpack( uvm_packer& packer )
  {
    hdr = static_cast<int>(packer.unpack_field_int(16).to_int());
    packer.unpack_object_with_meta(child);
  }

  UVM_OBJECT_UTILS(pck_holder);
};

class pck_cycle_node : public uvm_object
{
 public:
  int tag;
  pck_cycle_node* next;

  explicit pck_cycle_node( const std::string& name = "pck_cycle_node" )
  : uvm_object(name), tag(0), next(nullptr) {}
  virtual ~pck_cycle_node() {}

  virtual void do_pack( uvm_packer& packer ) const
  {
    packer.pack_field_int(tag, 32);
    if (next != nullptr)
      packer.pack_object(*next);
  }

  virtual void do_unpack( uvm_packer& packer )
  {
    tag = static_cast<int>(packer.unpack_field_int(32).to_int());
    if (next != nullptr)
      packer.unpack_object(*next);
  }

  UVM_OBJECT_UTILS(pck_cycle_node);
};

class pck_op_item : public uvm_object
{
 public:
  int v1;
  int v2;
  int exec_pack_count;
  int exec_unpack_count;
  int do_pack_count;
  int do_unpack_count;
  bool disable_hook;
  bool seen_policy_match;

  explicit pck_op_item( const std::string& name = "pck_op_item" )
  : uvm_object(name),
    v1(0),
    v2(0),
    exec_pack_count(0),
    exec_unpack_count(0),
    do_pack_count(0),
    do_unpack_count(0),
    disable_hook(false),
    seen_policy_match(false) {}
  virtual ~pck_op_item() {}

  virtual void do_execute_op( uvm_field_op* op )
  {
    if (op == nullptr)
      return;
    uvm_packer* p = dynamic_cast<uvm_packer*>(op->get_policy());
    if (p == nullptr)
      return;
    seen_policy_match = (p == op->get_policy());
    if (op->get_op_type() == UVM_PACK)
    {
      exec_pack_count++;
      p->pack_field_int(v1, 32);
      if (disable_hook)
        op->disable_user_hook();
    }
    else if (op->get_op_type() == UVM_UNPACK)
    {
      exec_unpack_count++;
      v1 = static_cast<int>(p->unpack_field_int(32).to_int());
      if (disable_hook)
        op->disable_user_hook();
    }
  }

  virtual void do_pack( uvm_packer& packer ) const
  {
    const_cast<pck_op_item*>(this)->do_pack_count++;
    packer.pack_field_int(v2, 32);
  }

  virtual void do_unpack( uvm_packer& packer )
  {
    do_unpack_count++;
    v2 = static_cast<int>(packer.unpack_field_int(32).to_int());
  }

  UVM_OBJECT_UTILS(pck_op_item);
};

class pck_throwing_item : public uvm_object
{
 public:
  bool throw_pack;
  bool throw_unpack;

  explicit pck_throwing_item( const std::string& name = "pck_throwing_item" )
  : uvm_object(name), throw_pack(false), throw_unpack(false) {}
  virtual ~pck_throwing_item() {}

  virtual void do_pack( uvm_packer& packer ) const
  {
    (void)packer;
    if (throw_pack)
      throw std::runtime_error("boom_pack");
  }

  virtual void do_unpack( uvm_packer& packer )
  {
    (void)packer;
    if (throw_unpack)
      throw std::runtime_error("boom_unpack");
  }

  UVM_OBJECT_UTILS(pck_throwing_item);
};

class packer_test : public uvm_test
{
 public:
  bool all_ok;

  explicit packer_test( uvm_component_name name )
  : uvm_test(name), all_ok(true) {}

  UVM_COMPONENT_UTILS(packer_test);

  void check( bool cond, const std::string& desc )
  {
    if (!cond)
    {
      all_ok = false;
      UVM_ERROR("PACKER_TEST", "FAILED: " + desc);
    }
  }

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);

    // 1. Inheritance, constructibility, factory utils, and defaults
    check(std::is_base_of<uvm_policy, uvm_packer>::value,
          "uvm_packer derives from uvm_policy");
    check(std::is_constructible<uvm_packer>::value,
          "uvm_packer is publicly default-constructible");
    check(std::is_constructible<uvm_packer, const std::string&>::value,
          "uvm_packer(name) is publicly constructible");
    check(g_packer_default_nonnull_before_sc_main,
          "uvm_default_packer non-null before sc_main");
    check(uvm_packer::get_default() != nullptr,
          "uvm_packer::get_default() != nullptr");
    check(uvm_default_packer == uvm_packer::get_default(),
          "uvm_default_packer == uvm_packer::get_default()");
    check(uvm_object::get_uvm_packer() == uvm_packer::get_default(),
          "uvm_object::get_uvm_packer() == uvm_packer::get_default()");

    uvm_packer* fact_p = uvm_packer::type_id::create("fact_p");
    check(fact_p != nullptr, "uvm_packer::type_id::create != nullptr");
    check(fact_p != nullptr && fact_p->get_type_name() == "uvm_packer",
          "uvm_packer get_type_name()");
    check(fact_p != nullptr && fact_p->get_object_type() == uvm_packer::get_type(),
          "uvm_packer get_object_type()");

    uvm_packer* orig_def = uvm_packer::get_default();
    uvm_packer::set_default(fact_p);
    check(uvm_packer::get_default() == fact_p && uvm_default_packer == fact_p,
          "set_default updates get_default() and uvm_default_packer");
    uvm_packer::set_default(nullptr);
    check(uvm_packer::get_default() == orig_def && uvm_default_packer == orig_def,
          "set_default(nullptr) restores builtin default");
    uvm_packer::type_id::destroy(fact_p);

    // 2. Recursion policy & flush() preserving flags/extensions
    uvm_packer p("p");
    check(p.get_recursion_policy() == UVM_DEFAULT_POLICY,
          "initial recursion policy is UVM_DEFAULT_POLICY");
    p.set_recursion_policy(UVM_REFERENCE);
    check(p.get_recursion_policy() == UVM_REFERENCE, "set/get UVM_REFERENCE");
    p.set_recursion_policy(UVM_DEEP);
    check(p.get_recursion_policy() == UVM_DEEP, "set/get UVM_DEEP");

    pck_leaf ext("ext");
    p.set_extension(&ext);
    p.use_metadata = true;
    p.big_endian = true;
    p.pack_field_int(0xABCD, 16);
    check(p.get_packed_size() == 16, "packed_size == 16 before flush");
    p.flush();
    check(p.get_packed_size() == 0, "flush clears packed_size");
    check(p.get_active_object() == nullptr, "flush clears active object");
    check(p.use_metadata == true && p.big_endian == true,
          "flush preserves use_metadata and big_endian");
    check(p.get_recursion_policy() == UVM_DEEP, "flush preserves recursion policy");
    check(p.get_extension(pck_leaf::get_type()) == &ext,
          "flush preserves policy extension");
    p.clear_extensions();

    // 3. Zero-bit get_packed_* / set_packed_* (no underflow) & partial-word stale bits
    p.use_metadata = false;
    p.big_endian = false;
    p.flush();
    {
      std::vector<bool> bits(3, true);
      std::vector<unsigned char> bytes(3, 0xFF);
      std::vector<unsigned int> ints(3, 0xFFFFFFFFu);
      std::vector<std::uint64_t> longints(3, 0xFFFFFFFFFFFFFFFFULL);

      p.get_packed_bits(bits);
      p.get_packed_bytes(bytes);
      p.get_packed_ints(ints);
      p.get_packed_longints(longints);
      check(bits.empty(), "get_packed_bits on 0-bit packer clears & returns empty");
      check(bytes.empty(), "get_packed_bytes on 0-bit packer clears & returns empty");
      check(ints.empty(), "get_packed_ints on 0-bit packer clears & returns empty");
      check(longints.empty(), "get_packed_longints on 0-bit packer clears & returns empty");

      p.pack_field_int(0xFF, 8);
      p.set_packed_bits(bits);
      check(p.get_packed_size() == 0, "set_packed_bits(empty) leaves size 0");
      p.pack_field_int(0xFF, 8);
      p.set_packed_bytes(bytes);
      check(p.get_packed_size() == 0, "set_packed_bytes(empty) leaves size 0");
      p.pack_field_int(0xFF, 8);
      p.set_packed_ints(ints);
      check(p.get_packed_size() == 0, "set_packed_ints(empty) leaves size 0");
      p.pack_field_int(0xFF, 8);
      p.set_packed_longints(longints);
      check(p.get_packed_size() == 0, "set_packed_longints(empty) leaves size 0");
    }

    // Partial-word stale-bit bleed guard: pack all-1s (128 bits), flush, pack 5 bits (0x05)
    {
      p.flush();
      p.big_endian = false;
      p.pack_field_int(0xFFFFFFFFFFFFFFFFULL, 64);
      p.pack_field_int(0xFFFFFFFFFFFFFFFFULL, 64);
      p.flush();
      p.pack_field_int(0x05, 5);
      std::vector<unsigned char> bytes;
      std::vector<unsigned int> ints;
      std::vector<std::uint64_t> longints;
      p.get_packed_bytes(bytes);
      p.get_packed_ints(ints);
      p.get_packed_longints(longints);
      check(bytes.size() == 1 && bytes[0] == 0x05,
            "partial trailing byte zero-padded (no stale bit bleed)");
      check(ints.size() == 1 && ints[0] == 0x05u,
            "partial trailing int zero-padded (no stale bit bleed)");
      check(longints.size() == 1 && longints[0] == 0x05ULL,
            "partial trailing longint zero-padded (no stale bit bleed)");
    }

    // 4. get_packed_* / set_packed_* round-trip for little & big endian
    for (int be = 0; be <= 1; ++be)
    {
      uvm_packer tx("tx"), rx("rx");
      tx.big_endian = (be == 1);
      rx.big_endian = (be == 1);
      tx.use_metadata = true;
      rx.use_metadata = true;

      pck_leaf src("src");
      src.a = 0x12345678;
      src.s = "hello";
      tx.pack_field_int(0xCAFEBABE12345678ULL, 64);
      tx.pack_field_int(0x5A, 7);
      tx.pack_object(src);

      std::vector<bool> bits;
      std::vector<unsigned char> bytes;
      std::vector<unsigned int> ints;
      std::vector<std::uint64_t> longints;
      tx.get_packed_bits(bits);
      tx.get_packed_bytes(bytes);
      tx.get_packed_ints(ints);
      tx.get_packed_longints(longints);

      // Bits round-trip
      rx.set_packed_bits(bits);
      check(rx.unpack_field_int(64).to_uint64() == 0xCAFEBABE12345678ULL,
            "bits round-trip 64b");
      check(rx.unpack_field_int(7).to_uint64() == 0x5AULL, "bits round-trip 7b");
      pck_leaf dst1("dst1");
      rx.unpack_object(dst1);
      check(dst1.a == 0x12345678 && dst1.s == "hello", "bits round-trip object");

      // Bytes round-trip
      rx.set_packed_bytes(bytes);
      check(rx.unpack_field_int(64).to_uint64() == 0xCAFEBABE12345678ULL,
            "bytes round-trip 64b");
      check(rx.unpack_field_int(7).to_uint64() == 0x5AULL, "bytes round-trip 7b");
      pck_leaf dst2("dst2");
      rx.unpack_object(dst2);
      check(dst2.a == 0x12345678 && dst2.s == "hello", "bytes round-trip object");

      // Ints round-trip
      rx.set_packed_ints(ints);
      check(rx.unpack_field_int(64).to_uint64() == 0xCAFEBABE12345678ULL,
            "ints round-trip 64b");
      check(rx.unpack_field_int(7).to_uint64() == 0x5AULL, "ints round-trip 7b");
      pck_leaf dst3("dst3");
      rx.unpack_object(dst3);
      check(dst3.a == 0x12345678 && dst3.s == "hello", "ints round-trip object");

      // Longints round-trip
      rx.set_packed_longints(longints);
      check(rx.unpack_field_int(64).to_uint64() == 0xCAFEBABE12345678ULL,
            "longints round-trip 64b");
      check(rx.unpack_field_int(7).to_uint64() == 0x5AULL, "longints round-trip 7b");
      pck_leaf dst4("dst4");
      rx.unpack_object(dst4);
      check(dst4.a == 0x12345678 && dst4.s == "hello", "longints round-trip object");
    }

    // 5. pack_object_with_meta / unpack_object_with_meta (no double-framing, null, auto-alloc, override)
    {
      pck_leaf item("item");
      item.a = 0x13572468;
      item.s = "meta";

      // Even when use_metadata == false, *_with_meta writes 4-bit metadata
      uvm_packer pm_false("pm_false");
      pm_false.use_metadata = false;
      pm_false.pack_object_with_meta(static_cast<const uvm_object*>(nullptr));
      check(pm_false.get_packed_size() == 4, "pack_object_with_meta(nullptr) writes 4 bits");
      check(pm_false.is_null(), "is_null() is true for nullptr meta");
      pck_leaf* out_null = &item;
      pm_false.unpack_object_with_meta(out_null);
      check(out_null == nullptr, "unpack_object_with_meta(nullptr meta) sets pointer to nullptr");

      // When use_metadata == true, *_with_meta must NOT double-frame (still 4 bits + payload)
      uvm_packer pm_true("pm_true");
      pm_true.use_metadata = true;
      pm_true.pack_object(item);
      unsigned int single_meta_sz = pm_true.get_packed_size();

      pm_true.flush();
      pm_true.pack_object_with_meta(&item);
      check(pm_true.get_packed_size() == single_meta_sz,
            "pack_object_with_meta does not double-frame when use_metadata == true");
      check(!pm_true.is_null(), "is_null() is false for non-null meta");

      pck_leaf* alloc_leaf = nullptr;
      pm_true.unpack_object_with_meta(alloc_leaf);
      check(alloc_leaf != nullptr && alloc_leaf != &item,
            "unpack_object_with_meta allocates T* when nullptr");
      check(alloc_leaf != nullptr && alloc_leaf->a == 0x13572468 && alloc_leaf->s == "meta",
            "unpack_object_with_meta populates allocated T*");
      delete alloc_leaf;

      // Nested holder with use_metadata == false
      pck_holder h_src("h_src");
      h_src.hdr = 0x42;
      h_src.child = &item;
      uvm_packer h_p("h_p");
      h_p.use_metadata = false;
      std::vector<unsigned int> h_ints;
      h_src.pack_ints(h_ints, &h_p);
      pck_holder h_dst("h_dst");
      h_dst.unpack_ints(h_ints, &h_p);
      check(h_dst.hdr == 0x42 && h_dst.child != nullptr &&
            h_dst.child->a == 0x13572468 && h_dst.child->s == "meta",
            "nested pack_object_with_meta/unpack_object_with_meta round-trip");
      delete h_dst.child;
      h_dst.child = nullptr;

      // UVM_REFERENCE writes 4-bit 0 meta
      uvm_packer ref_p("ref_p");
      ref_p.set_recursion_policy(UVM_REFERENCE);
      ref_p.pack_object_with_meta(&item);
      check(ref_p.get_packed_size() == 4 && ref_p.is_null(),
            "UVM_REFERENCE pack_object_with_meta writes 4-bit null meta");
    }

    // 6. Error guards: is_null() on empty packer, cycle detection, exception safety
    {
      packer_msg_catcher ctchr;
      uvm_report_cb::add(nullptr, &ctchr);

      uvm_packer empty_p("empty_p");
      check(empty_p.is_null(), "is_null() on empty packer returns true");
      check(ctchr.pckszerr_count == 1, "is_null() on empty packer emits PCKSZERR");

      uvm_object* untyped_null = nullptr;
      uvm_packer meta_one("meta_one");
      pck_leaf leaf("leaf");
      leaf.a = 7;
      leaf.s = "x";
      meta_one.pack_object_with_meta(&leaf);
      meta_one.unpack_object_with_meta(untyped_null);
      check(ctchr.unpckerr_count == 1,
            "unpack_object_with_meta(uvm_object*&) with nullptr reports UNPCKERR");

      // Cycle detection during pack & unpack
      pck_cycle_node n1("n1"), n2("n2");
      n1.tag = 1;
      n2.tag = 2;
      n1.next = &n2;
      n2.next = &n1;
      uvm_packer cyc_p("cyc_p");
      cyc_p.pack_object(n1);
      check(ctchr.cycfnd_count == 1, "pack_object detects cycle with CYCFND");
      cyc_p.unpack_object(n1);
      check(ctchr.cycfnd_count == 2, "unpack_object detects cycle with CYCFND");

      // Exception safety
      pck_throwing_item thr("thr");
      thr.throw_pack = true;
      uvm_packer exc_p("exc_p");
      std::vector<bool> bits;
      bool caught = false;
      try
      {
        thr.pack(bits, &exc_p);
      }
      catch (const std::runtime_error&)
      {
        caught = true;
      }
      check(caught, "pack() propagates exception");
      check(exc_p.get_active_object() == nullptr,
            "active object popped after pack() exception");
      check(!exc_p.is_active_object(&thr),
            "throwing object no longer active after pack() exception");

      thr.throw_pack = false;
      thr.pack(bits, &exc_p);
      thr.throw_unpack = true;
      caught = false;
      try
      {
        thr.unpack(bits, &exc_p);
      }
      catch (const std::runtime_error&)
      {
        caught = true;
      }
      check(caught, "unpack() propagates exception");
      check(exc_p.get_active_object() == nullptr,
            "active object popped after unpack() exception");

      uvm_report_cb::do_delete(nullptr, &ctchr);
    }

    // 7. uvm_field_op (UVM_PACK / UVM_UNPACK) integration & top-level no-meta framing
    {
      pck_op_item src("src"), dst("dst");
      src.v1 = 111;
      src.v2 = 222;
      uvm_packer op_p("op_p");
      op_p.use_metadata = true; // must NOT add 4-bit top-level meta in pack_ints

      std::vector<unsigned int> ints;
      int bit_sz = src.pack_ints(ints, &op_p);
      check(bit_sz == 64, "top-level pack_ints does not add 4-bit object meta (64 bits)");
      check(src.exec_pack_count == 1 && src.do_pack_count == 1 && src.seen_policy_match,
            "UVM_PACK calls do_execute_op and do_pack");
      dst.unpack_ints(ints, &op_p);
      check(dst.exec_unpack_count == 1 && dst.do_unpack_count == 1,
            "UVM_UNPACK calls do_execute_op and do_unpack");
      check(dst.v1 == 111 && dst.v2 == 222, "field_op + user hook round-trip");

      // disable_user_hook() skips do_pack / do_unpack
      pck_op_item src2("src2"), dst2("dst2");
      src2.v1 = 333;
      src2.v2 = 444;
      src2.disable_hook = true;
      dst2.disable_hook = true;
      ints.clear();
      bit_sz = src2.pack_ints(ints, &op_p);
      check(bit_sz == 32, "disable_user_hook skips do_pack (32 bits)");
      check(src2.exec_pack_count == 1 && src2.do_pack_count == 0,
            "do_pack skipped when user hook disabled");
      dst2.unpack_ints(ints, &op_p);
      check(dst2.exec_unpack_count == 1 && dst2.do_unpack_count == 0,
            "do_unpack skipped when user hook disabled");
      check(dst2.v1 == 333 && dst2.v2 == 0, "only do_execute_op field unpacked");
    }

    // 8. Signed sc_int<W> and sc_bigint<W> negative sign extension
    {
      uvm_packer sp("sp");

      sc_dt::sc_int<1> i1_in = -1, i1_out = 0;
      sc_dt::sc_int<8> i8_in = -42, i8_out = 12;
      sc_dt::sc_int<12> i12_in = -123, i12_out = 456;
      sc_dt::sc_int<64> i64_in = -0x123456789ABCDEF0LL, i64_out = 99;
      sc_dt::sc_int<12> i12_pos_in = 321, i12_pos_out = -1;
      sc_dt::sc_bigint<70> bi70_neg_in = -123456789012345LL, bi70_neg_out = 1;
      sc_dt::sc_bigint<70> bi70_wide_in = (sc_dt::sc_bigint<70>(-1) << 65) - 7, bi70_wide_out = 1;
      sc_dt::sc_bigint<70> bi70_pos_in = 987654321012345LL, bi70_pos_out = -1;

      sp << i1_in << i8_in << i12_in << i64_in << i12_pos_in
         << bi70_neg_in << bi70_wide_in << bi70_pos_in;
      sp >> i1_out >> i8_out >> i12_out >> i64_out >> i12_pos_out
         >> bi70_neg_out >> bi70_wide_out >> bi70_pos_out;

      check(i1_out == -1 && i1_out == i1_in, "sc_int<1> -1 sign-extended");
      check(i8_out == -42 && i8_out == i8_in, "sc_int<8> -42 sign-extended");
      check(i12_out == -123 && i12_out == i12_in, "sc_int<12> -123 sign-extended");
      check(i64_out == i64_in, "sc_int<64> negative round-trip");
      check(i12_pos_out == 321 && i12_pos_out == i12_pos_in,
            "sc_int<12> positive clears stale sign bits");
      check(bi70_neg_out == bi70_neg_in && bi70_neg_out < 0,
            "sc_bigint<70> negative sign-extended");
      check(bi70_wide_out == bi70_wide_in && bi70_wide_out < 0,
            "sc_bigint<70> wide negative sign-extended");
      check(bi70_pos_out == bi70_pos_in && bi70_pos_out > 0,
            "sc_bigint<70> positive round-trip");
    }

    // 9. Empty/truncated stream operator>> safety (null m_bits & bounds checks)
    {
      packer_msg_catcher ctchr;
      uvm_report_cb::add(nullptr, &ctchr);

      uvm_packer empty_op("empty_op");
      bool b = true;
      sc_dt::sc_logic lg = sc_dt::SC_LOGIC_1;
      sc_dt::sc_bv<8> bv(0xAA);
      sc_dt::sc_lv<8> lv("10101010");
      sc_dt::sc_int<8> si(5);
      sc_dt::sc_uint<8> su(5);
      sc_dt::sc_bigint<70> bi(5);
      sc_dt::sc_biguint<70> bu(5);
      std::string str = "abc";
      sc_core::sc_time tm(10, sc_core::SC_NS);
      double dbl = 1.5;
      char ch = 'z';

      empty_op >> b;
      empty_op >> lg;
      empty_op >> bv;
      empty_op >> lv;
      empty_op >> si;
      empty_op >> su;
      empty_op >> bi;
      empty_op >> bu;
      empty_op >> str;
      empty_op >> tm;
      empty_op >> dbl;
      empty_op >> ch;

      check(ctchr.pckszerr_count >= 12,
            "empty packer operator>> across all types emits PCKSZERR without crashing");

      // Truncated stream (only 3 bits packed)
      uvm_packer trunc_p("trunc_p");
      trunc_p.pack_field_int(0x5, 3);
      int before_trunc = ctchr.pckszerr_count;
      trunc_p >> bv;
      trunc_p >> lv;
      trunc_p >> si;
      trunc_p >> su;
      trunc_p >> bi;
      trunc_p >> bu;
      check(ctchr.pckszerr_count == before_trunc + 6,
            "truncated packer operator>> emits PCKSZERR for all sc_dt types");
      check(trunc_p.unpack_field_int(3).to_uint64() == 0x5ULL,
            "failed truncated unpacks do not advance unpack_index");

      // Exact-length unpack_string(num_chars) with use_metadata == false
      uvm_packer exact_str("exact_str");
      exact_str.use_metadata = false;
      exact_str.pack_string("xyz");
      int before_str = ctchr.pckszerr_count;
      std::string got_xyz = exact_str.unpack_string(3);
      check(got_xyz == "xyz" && ctchr.pckszerr_count == before_str,
            "exact-length unpack_string(3) emits no spurious PCKSZERR");

      uvm_report_cb::do_delete(nullptr, &ctchr);
    }

    std::cout << "packer_checks_ok=" << (all_ok ? 1 : 0) << std::endl;
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
  run_test("packer_test");
  return 0;
}
