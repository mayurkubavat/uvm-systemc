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

// Reproduces and verifies Fixes 4, 5, and 6:
// - Fix 5: response_queue_depth == 8 default, -1 unlimited, overflow error &
//   suppression, response_handler callback, dropped response on null sequence,
//   clear_response_queue(), re-start()/destruction, zero-leak live_count and
//   Apple malloc_zone_statistics checks.
// - Fix 6: symmetric begin_child_tr() / end_tr() with UVM_LOW +
//   uvm_text_tr_database (LINK RELATION=child, is_active/begin/end_time),
//   and m_tr_handle reset on sequence re-start as virtual sequence.
// - Fix 4: is_blocked() / has_lock() return false on unstarted and
//   start(nullptr) virtual sequences even when a real sequencer is locked.

#include <systemc>
#include <uvm>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#if defined(__APPLE__)
#  if defined(__has_feature)
#    if !__has_feature(address_sanitizer)
#      include <malloc/malloc.h>
#      define UVM_TEST_CHECK_MALLOC_ZONE 1
#    endif
#  else
#    include <malloc/malloc.h>
#    define UVM_TEST_CHECK_MALLOC_ZONE 1
#  endif
#endif

using namespace uvm;

class req_item : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(req_item);

  int value;
  bool use_rsp_export;
  bool send_rsp;
  int rsp_count;
  bool force_bad_seq_id;

  req_item( const std::string& name = "req_item" )
  : uvm_sequence_item(name),
    value(0),
    use_rsp_export(false),
    send_rsp(false),
    rsp_count(1),
    force_bad_seq_id(false)
  {}
};

class tracked_rsp : public uvm_sequence_item
{
 public:
  UVM_OBJECT_UTILS(tracked_rsp);

  static int live_count;
  int value;

  explicit tracked_rsp( const std::string& name = "tracked_rsp" )
  : uvm_sequence_item(name), value(0)
  {
    live_count++;
  }

  tracked_rsp( const tracked_rsp& other )
  : uvm_sequence_item(other), value(other.value)
  {
    live_count++;
  }

  tracked_rsp& operator=( const tracked_rsp& other )
  {
    if (this != &other)
    {
      uvm_sequence_item::operator=(other);
      value = other.value;
    }
    return *this;
  }

  void set_seq_id_for_test( int id )
  {
    set_sequence_id(id);
  }

  virtual ~tracked_rsp()
  {
    live_count--;
  }
};

int tracked_rsp::live_count = 0;

class test_driver : public uvm_driver<req_item, tracked_rsp>
{
 public:
  UVM_COMPONENT_UTILS(test_driver);

  test_driver( uvm_component_name name ) : uvm_driver<req_item, tracked_rsp>(name) {}

  virtual void run_phase( uvm_phase& phase )
  {
    req_item req;

    while (true)
    {
      seq_item_port->get_next_item(req);
      sc_core::wait(5, sc_core::SC_NS);
      seq_item_port->item_done();

      if (req.send_rsp)
      {
        for (int i = 0; i < req.rsp_count; i++)
        {
          tracked_rsp rsp("rsp");
          rsp.set_id_info(req);
          if (req.force_bad_seq_id)
            rsp.set_seq_id_for_test(999999);
          rsp.value = req.value * 10 + i;
          if (req.use_rsp_export)
            rsp_port.write(rsp);
          else
            seq_item_port->put_response(rsp);
        }
      }
    }
  }
};

class rsp_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(rsp_seq);

  int count;
  bool send_rsp;
  int rsp_count;
  bool use_rsp_export;
  bool drain_responses;
  bool force_bad_seq_id;
  std::vector<int> handler_values;

  rsp_seq( const std::string& name = "rsp_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name),
    count(1),
    send_rsp(true),
    rsp_count(1),
    use_rsp_export(false),
    drain_responses(false),
    force_bad_seq_id(false)
  {}

  virtual void response_handler( const uvm_sequence_item* response )
  {
    const tracked_rsp* r = dynamic_cast<const tracked_rsp*>(response);
    if (r != nullptr)
      handler_values.push_back(r->value);
  }

  virtual void body()
  {
    for (int i = 0; i < count; i++)
    {
      req_item* it = req_item::type_id::create("it");
      it->value = i + 1;
      it->send_rsp = send_rsp;
      it->rsp_count = rsp_count;
      it->use_rsp_export = use_rsp_export;
      it->force_bad_seq_id = force_bad_seq_id;
      start_item(it);
      finish_item(it);
      req_item::type_id::destroy(it);

      if (drain_responses)
      {
        for (int j = 0; j < rsp_count; j++)
        {
          tracked_rsp r;
          get_response(&r);
        }
      }
      else if (send_rsp && rsp_count > 0)
      {
        sc_core::wait(1, sc_core::SC_NS);
      }
    }
  }
};

class rec_item : public req_item
{
 public:
  UVM_OBJECT_UTILS(rec_item);

  rec_item( const std::string& name = "rec_item" ) : req_item(name) {}

  virtual void do_record( const uvm_recorder& recorder )
  {
    req_item::do_record(recorder);
    uvm_recorder* r = const_cast<uvm_recorder*>(&recorder);
    r->record_field_int("val", value, 32, UVM_DEC);
  }
};

class rec_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(rec_seq);

  uvm_sequencer_base* explicit_sqr;
  bool active_in_predo;
  bool active_in_postdo;
  bool begin_gt_zero;
  bool end_ge_begin;

  rec_seq( const std::string& name = "rec_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name),
    explicit_sqr(nullptr),
    active_in_predo(false),
    active_in_postdo(true),
    begin_gt_zero(false),
    end_ge_begin(false),
    m_cur_item(nullptr)
  {}

  virtual void pre_do( bool is_item )
  {
    if (is_item && m_cur_item != nullptr)
    {
      active_in_predo = m_cur_item->is_active();
      begin_gt_zero = (m_cur_item->get_begin_time() > sc_core::SC_ZERO_TIME);
    }
  }

  virtual void post_do( uvm_sequence_item* this_item )
  {
    if (m_cur_item != nullptr)
    {
      active_in_postdo = m_cur_item->is_active();
      end_ge_begin = (m_cur_item->get_end_time() >= m_cur_item->get_begin_time()) &&
                     (m_cur_item->get_end_time() > sc_core::SC_ZERO_TIME);
    }
  }

  virtual void body()
  {
    for (int i = 0; i < 2; i++)
    {
      rec_item* it = rec_item::type_id::create(i == 0 ? "item_a" : "item_b");
      it->value = 10 + i;
      it->send_rsp = false;
      m_cur_item = it;
      start_item(it, -1, explicit_sqr);
      sc_core::wait(2, sc_core::SC_NS);
      finish_item(it);
      m_cur_item = nullptr;
      rec_item::type_id::destroy(it);
    }
  }

 private:
  rec_item* m_cur_item;
};

class lock_holder_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(lock_holder_seq);

  sc_core::sc_event locked_ev;
  sc_core::sc_event release_ev;

  lock_holder_seq( const std::string& name = "lock_holder_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name) {}

  virtual void body()
  {
    m_sequencer->lock(this);
    locked_ev.notify();
    sc_core::wait(release_ev);
    m_sequencer->unlock(this);
  }
};

class vseq_checker : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(vseq_checker);

  uvm_sequencer<req_item, tracked_rsp>* real_sqr;
  bool vseq_blocked;
  bool vseq_has_lock;
  bool seqr_was_null;
  bool child_blocked_during_lock;

  vseq_checker( const std::string& name = "vseq_checker" )
  : uvm_sequence<req_item, tracked_rsp>(name),
    real_sqr(nullptr),
    vseq_blocked(true),
    vseq_has_lock(true),
    seqr_was_null(false),
    child_blocked_during_lock(false)
  {}

  virtual void body()
  {
    seqr_was_null = (get_sequencer() == nullptr);
    vseq_blocked = is_blocked();
    vseq_has_lock = has_lock();

    rsp_seq* child = rsp_seq::type_id::create("child");
    child->set_sequencer(real_sqr);
    child_blocked_during_lock = child->is_blocked();
    rsp_seq::type_id::destroy(child);
  }
};

static std::vector<std::string> collect_normalized_db( const std::string& filename )
{
  std::ifstream in(filename.c_str());
  std::map<std::string, std::string> names;
  std::regex handle("(TXH1?:|TXH2:|STREAM:)([0-9]+)");
  std::regex inst_id("(NAME:inst_id VALUE:)([0-9]+)");
  std::string line;
  std::vector<std::string> lines;

  while (std::getline(in, line))
  {
    std::string out;
    std::smatch m;
    std::string rest = line;

    while (std::regex_search(rest, m, handle))
    {
      std::string& name = names[m[2].str()];
      if (name.empty())
      {
        std::ostringstream str;
        str << "h" << names.size();
        name = str.str();
      }
      out += m.prefix().str() + m[1].str() + name;
      rest = m.suffix().str();
    }
    out += rest;
    lines.push_back(std::regex_replace(out, inst_id, "$1<id>"));
  }
  return lines;
}

class full_child_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(full_child_seq);

  bool stall_in_predo;
  int item_val;

  full_child_seq( const std::string& name = "full_child_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name), stall_in_predo(true), item_val(77) {}

  virtual void pre_do( bool is_item )
  {
    if (is_item && stall_in_predo)
      sc_core::wait(50, sc_core::SC_NS);
  }

  virtual void body()
  {
    req_item it("full_it");
    it.value = item_val;
    it.send_rsp = false;
    start_item(&it);
    finish_item(&it);
  }
};

class full_parent_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(full_parent_seq);

  uvm_sequencer<req_item, tracked_rsp>* target_sqr;
  full_child_seq* c0;
  full_child_seq* c1;

  full_parent_seq( const std::string& name = "full_parent_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name), target_sqr(nullptr), c0(nullptr), c1(nullptr) {}

  virtual void body()
  {
    c0 = full_child_seq::type_id::create("fc0");
    c1 = full_child_seq::type_id::create("fc1");
    c0->stall_in_predo = true;
    c1->stall_in_predo = true;

    sc_core::sc_process_handle h0 = sc_core::sc_spawn([this]() { c0->start(target_sqr, this); });
    sc_core::sc_process_handle h1 = sc_core::sc_spawn([this]() { c1->start(target_sqr, this); });
    if (h0.valid() && !h0.terminated())
      sc_core::wait(h0.terminated_event());
    if (h1.valid() && !h1.terminated())
      sc_core::wait(h1.terminated_event());
  }
};

class overlap_start_item_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(overlap_start_item_seq);

  uvm_sequencer_base* second_sqr;
  rec_item it_a;
  rec_item it_b;

  overlap_start_item_seq( const std::string& name = "overlap_start_item_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name),
    second_sqr(nullptr),
    it_a("overlap_it_a"),
    it_b("overlap_it_b")
  {}

  virtual void body()
  {
    it_a.value = 301;
    it_b.value = 302;
    start_item(&it_a);
    start_item(&it_b, -1, second_sqr);
    sc_core::wait(50, sc_core::SC_NS);
  }
};

class abandoned_start_item_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(abandoned_start_item_seq);

  rec_item it;

  abandoned_start_item_seq( const std::string& name = "abandoned_start_item_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name), it("abandoned_it") {}

  virtual void body()
  {
    it.value = 401;
    start_item(&it);
    // Intentionally return without finish_item(&it)
  }
};

class dokill_frees_item_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(dokill_frees_item_seq);

  rec_item* cur_item;

  dokill_frees_item_seq( const std::string& name = "dokill_frees_item_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name), cur_item(nullptr) {}

  virtual void do_kill()
  {
    if (cur_item != nullptr)
    {
      rec_item::type_id::destroy(cur_item);
      cur_item = nullptr;
    }
  }

  virtual void body()
  {
    cur_item = rec_item::type_id::create("owned_it");
    cur_item->value = 501;
    start_item(cur_item);
    sc_core::wait(50, sc_core::SC_NS);
    finish_item(cur_item);
    rec_item::type_id::destroy(cur_item);
    cur_item = nullptr;
  }
};

class destroyed_item_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(destroyed_item_seq);

  bool heap_mode;

  destroyed_item_seq( const std::string& name = "destroyed_item_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name), heap_mode(false) {}

  virtual void body()
  {
    if (heap_mode)
    {
      rec_item* hit = new rec_item("heap_it");
      hit->value = 202;
      start_item(hit);
      delete hit;
    }
    else
    {
      {
        rec_item local_it("local_it");
        local_it.value = 101;
        start_item(&local_it);
      }
    }
    sc_core::wait(50, sc_core::SC_NS);
  }
};

class assign_active_item_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(assign_active_item_seq);

  bool kill_mode;
  rec_item it_b;
  int observed_inst_before;
  int observed_inst_after;
  int observed_val_after;
  bool active_after_finish;
  int handle_after_finish;

  assign_active_item_seq( const std::string& name = "assign_active_item_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name),
    kill_mode(false),
    it_b("active_B"),
    observed_inst_before(-1),
    observed_inst_after(-1),
    observed_val_after(-1),
    active_after_finish(true),
    handle_after_finish(-1)
  {}

  virtual void body()
  {
    rec_item src("src_template");
    src.value = kill_mode ? 902 : 901;

    it_b.value = 111;
    start_item(&it_b);
    observed_inst_before = it_b.get_inst_id();
    it_b = src;
    observed_inst_after = it_b.get_inst_id();
    observed_val_after = it_b.value;

    if (kill_mode)
    {
      sc_core::wait(50, sc_core::SC_NS);
    }
    else
    {
      finish_item(&it_b);
      active_after_finish = it_b.is_active();
      handle_after_finish = it_b.get_tr_handle();
    }
  }
};

class post_run_active_seq : public uvm_sequence<req_item, tracked_rsp>
{
 public:
  UVM_OBJECT_UTILS(post_run_active_seq);

  rec_item* ext_item;

  post_run_active_seq( const std::string& name = "post_run_active_seq" )
  : uvm_sequence<req_item, tracked_rsp>(name), ext_item(nullptr) {}

  virtual void body()
  {
    start_item(ext_item);
    sc_core::wait(1000, sc_core::SC_NS);
  }
};

static uvm_sequencer<req_item, tracked_rsp>* g_sqr_dtor1 = nullptr;
static post_run_active_seq* g_seq_dtor1 = nullptr;
static rec_item* g_item_dtor1 = nullptr;

static uvm_sequencer<req_item, tracked_rsp>* g_sqr_dtor2 = nullptr;
static post_run_active_seq* g_seq_dtor2 = nullptr;
static rec_item* g_item_dtor2 = nullptr;

class test : public uvm_test
{
 public:
  UVM_COMPONENT_UTILS(test);

  uvm_sequencer<req_item, tracked_rsp>* sqr;
  test_driver* drv;
  uvm_sequencer<req_item, tracked_rsp>* sqr_full;
  test_driver* drv_full;
  uvm_sequencer<req_item, tracked_rsp>* sqr_dtor1;
  test_driver* drv_dtor1;
  uvm_sequencer<req_item, tracked_rsp>* sqr_dtor2;
  test_driver* drv_dtor2;

  test( uvm_component_name name )
  : uvm_test(name),
    sqr(nullptr),
    drv(nullptr),
    sqr_full(nullptr),
    drv_full(nullptr),
    sqr_dtor1(nullptr),
    drv_dtor1(nullptr),
    sqr_dtor2(nullptr),
    drv_dtor2(nullptr)
  {}

  virtual void build_phase( uvm_phase& phase )
  {
    uvm_test::build_phase(phase);
    uvm_config_db<int>::set(this, "sqr", "recording_detail", UVM_LOW);
    uvm_config_db<int>::set(this, "sqr_full", "recording_detail", UVM_FULL);
    uvm_config_db<int>::set(this, "sqr_dtor1", "recording_detail", UVM_FULL);
    uvm_config_db<int>::set(this, "sqr_dtor2", "recording_detail", UVM_FULL);
    sqr = new uvm_sequencer<req_item, tracked_rsp>("sqr");
    drv = test_driver::type_id::create("drv", this);
    sqr_full = new uvm_sequencer<req_item, tracked_rsp>("sqr_full");
    drv_full = test_driver::type_id::create("drv_full", this);
    sqr_dtor1 = new uvm_sequencer<req_item, tracked_rsp>("sqr_dtor1");
    drv_dtor1 = test_driver::type_id::create("drv_dtor1", this);
    sqr_dtor2 = new uvm_sequencer<req_item, tracked_rsp>("sqr_dtor2");
    drv_dtor2 = test_driver::type_id::create("drv_dtor2", this);
    g_sqr_dtor1 = sqr_dtor1;
    g_sqr_dtor2 = sqr_dtor2;
  }

  virtual void connect_phase( uvm_phase& phase )
  {
    drv->seq_item_port.connect(sqr->seq_item_export);
    drv->rsp_port.connect(sqr->rsp_export);
    drv_full->seq_item_port.connect(sqr_full->seq_item_export);
    drv_full->rsp_port.connect(sqr_full->rsp_export);
    drv_dtor1->seq_item_port.connect(sqr_dtor1->seq_item_export);
    drv_dtor1->rsp_port.connect(sqr_dtor1->rsp_export);
    drv_dtor2->seq_item_port.connect(sqr_dtor2->seq_item_export);
    drv_dtor2->rsp_port.connect(sqr_dtor2->rsp_export);
  }

  virtual void run_phase( uvm_phase& phase )
  {
    phase.raise_objection(this);

    // ------------------------------------------------------------------
    // Fix 5: response_queue_depth == 8 default and zero response leaks
    // ------------------------------------------------------------------
    {
      rsp_seq* s_def = rsp_seq::type_id::create("s_def");
      std::cout << "Fix5 default_depth=" << s_def->get_response_queue_depth() << std::endl;

      // 9th response triggers UVM_ERROR and is freed cleanly (1 seq member = 1 live)
      s_def->count = 1;
      s_def->rsp_count = 9;
      s_def->start(sqr);
      std::cout << "Fix5 after_overflow live_count=" << tracked_rsp::live_count << std::endl;

      // Re-start clears previous 8 queued items without leaking
      s_def->rsp_count = 0;
      s_def->send_rsp = false;
      s_def->set_response_queue_error_report_disabled(true);
      s_def->start(sqr);
      std::cout << "Fix5 after_restart_clear live_count=" << tracked_rsp::live_count << std::endl;

      // Unlimited (-1) + clear_response_queue() + disabled overflow report
      s_def->set_response_queue_depth(-1);
      s_def->send_rsp = true;
      s_def->rsp_count = 12;
      s_def->start(sqr);
      std::cout << "Fix5 unlimited_queued live_count=" << tracked_rsp::live_count << std::endl;
      s_def->clear_response_queue();
      std::cout << "Fix5 after_manual_clear live_count=" << tracked_rsp::live_count << std::endl;

      // use_response_handler(true) via both put_response and analysis_write (rsp_port)
      s_def->use_response_handler(true);
      s_def->set_response_queue_depth(8);
      s_def->rsp_count = 10;
      s_def->use_rsp_export = false;
      s_def->start(sqr);
      s_def->use_rsp_export = true;
      s_def->start(sqr);
      std::cout << "Fix5 handler_calls=" << s_def->handler_values.size()
                << " live_count=" << tracked_rsp::live_count << std::endl;

      // Dropped response when sequence_ptr == nullptr (both put_response and analysis_write)
      s_def->use_response_handler(false);
      s_def->force_bad_seq_id = true;
      s_def->rsp_count = 2;
      s_def->use_rsp_export = false;
      s_def->start(sqr);
      s_def->use_rsp_export = true;
      s_def->start(sqr);
      std::cout << "Fix5 null_seq_drop live_count=" << tracked_rsp::live_count << std::endl;

      // Destruction (m_clear) with non-empty queue
      s_def->force_bad_seq_id = false;
      s_def->rsp_count = 3;
      s_def->start(sqr);
      rsp_seq::type_id::destroy(s_def);
      std::cout << "Fix5 after_destroy live_count=" << tracked_rsp::live_count << std::endl;
    }

    // Stress loop + macOS malloc_zone_statistics check
    {
      rsp_seq* stress = rsp_seq::type_id::create("stress");
      stress->set_response_queue_error_report_disabled(true);

      // Warm-up
      stress->count = 1;
      stress->rsp_count = 10;
      stress->use_response_handler(true);
      stress->start(sqr);
      stress->use_response_handler(false);
      stress->start(sqr);
      stress->clear_response_queue();
      stress->handler_values.clear();

#ifdef UVM_TEST_CHECK_MALLOC_ZONE
      malloc_statistics_t stats_before{};
      malloc_zone_statistics(nullptr, &stats_before);
#endif
      const int live_before = tracked_rsp::live_count;

      for (int iter = 0; iter < 50; iter++)
      {
        stress->use_response_handler(true);
        stress->use_rsp_export = (iter % 2 == 1);
        stress->force_bad_seq_id = false;
        stress->rsp_count = 5;
        stress->start(sqr);

        stress->use_response_handler(false);
        stress->force_bad_seq_id = false;
        stress->rsp_count = 10; // overflows 2 each iter
        stress->start(sqr);
        stress->clear_response_queue();

        stress->force_bad_seq_id = true;
        stress->rsp_count = 2;
        stress->start(sqr);
      }
      stress->handler_values.clear();

      const int live_after = tracked_rsp::live_count;
      std::cout << "Fix5 stress_live_delta=" << (live_after - live_before) << std::endl;

#ifdef UVM_TEST_CHECK_MALLOC_ZONE
      malloc_statistics_t stats_after{};
      malloc_zone_statistics(nullptr, &stats_after);
      const long long heap_delta =
        static_cast<long long>(stats_after.size_in_use) -
        static_cast<long long>(stats_before.size_in_use);
      if (heap_delta > 64 * 1024)
        std::cout << "Fix5 ERROR: unexpected heap growth " << heap_delta << " bytes" << std::endl;
      else
        std::cout << "Fix5 heap_growth_ok=1" << std::endl;
#else
      std::cout << "Fix5 heap_growth_ok=1" << std::endl;
#endif
      rsp_seq::type_id::destroy(stress);
      std::cout << "Fix5 stress_destroyed live_count=" << tracked_rsp::live_count << std::endl;
    }

    // ------------------------------------------------------------------
    // Fix 6: symmetric begin_child_tr() / end_tr() & m_tr_handle reset
    // ------------------------------------------------------------------
    {
      sc_core::wait(5, sc_core::SC_NS);
      rec_seq* rseq = rec_seq::type_id::create("rseq");
      rseq->start(sqr);
      std::cout << "Fix6 run1 active_predo=" << rseq->active_in_predo
                << " active_postdo=" << rseq->active_in_postdo
                << " begin_gt_zero=" << rseq->begin_gt_zero
                << " end_ge_begin=" << rseq->end_ge_begin << std::endl;

      // Re-start as virtual sequence (m_tr_handle must be reset to 0, no ILLHNDL)
      rseq->explicit_sqr = sqr;
      rseq->start(nullptr);
      std::cout << "Fix6 run2_virtual active_predo=" << rseq->active_in_predo
                << " active_postdo=" << rseq->active_in_postdo << std::endl;
      rec_seq::type_id::destroy(rseq);
    }

    // ------------------------------------------------------------------
    // Fix 4: virtual sequence is_blocked() / has_lock() null-guard
    // ------------------------------------------------------------------
    {
      vseq_checker* vseq = vseq_checker::type_id::create("vseq");
      vseq->real_sqr = sqr;
      std::cout << "Fix4 unstarted blocked=" << vseq->is_blocked()
                << " has_lock=" << vseq->has_lock() << std::endl;

      lock_holder_seq* lseq = lock_holder_seq::type_id::create("lseq");
      sc_core::sc_process_handle hl = sc_core::sc_spawn([&]() { lseq->start(sqr); });
      sc_core::wait(lseq->locked_ev);

      vseq->start(nullptr);
      std::cout << "Fix4 virtual_during_lock seqr_null=" << vseq->seqr_was_null
                << " blocked=" << vseq->vseq_blocked
                << " has_lock=" << vseq->vseq_has_lock
                << " child_blocked=" << vseq->child_blocked_during_lock << std::endl;

      lseq->release_ev.notify();
      sc_core::wait(hl.terminated_event());
      lock_holder_seq::type_id::destroy(lseq);
      vseq_checker::type_id::destroy(vseq);
    }

    // ------------------------------------------------------------------
    // Fix 6 (UVM_FULL kill cleanup): parent->kill() with active children
    // must close & free all child & active-item recorders without leak
    // ------------------------------------------------------------------
    {
      int active_after_kill = 0;
      for (int iter = 0; iter < 5; iter++)
      {
        full_parent_seq* fp = full_parent_seq::type_id::create("fp");
        fp->target_sqr = sqr_full;
        sc_core::sc_process_handle hfp = sc_core::sc_spawn([&]() { fp->start(sqr_full); });
        sc_core::wait(2, sc_core::SC_NS);
        fp->kill();
        if (hfp.valid() && !hfp.terminated())
          sc_core::wait(hfp.terminated_event());
        if (fp->is_active() || fp->c0->is_active() || fp->c1->is_active())
          active_after_kill++;
        if (iter == 0)
        {
          // Reuse killed child on sqr_full without ILLHNDL
          fp->c0->stall_in_predo = false;
          fp->c0->start(sqr_full);
        }
        full_child_seq::type_id::destroy(fp->c0);
        full_child_seq::type_id::destroy(fp->c1);
        full_parent_seq::type_id::destroy(fp);
      }
      std::cout << "Fix6 UVM_FULL kill active_after_kill=" << active_after_kill << std::endl;
    }

    // ------------------------------------------------------------------
    // Finding 1: overlapping start_item(), abandoned start_item(),
    // do_kill() freeing active item, and item destroyed before kill()
    // ------------------------------------------------------------------
    {
      // Mode 3: overlapping start_item(itA) then start_item(itB, -1, sqr) -> kill()
      overlap_start_item_seq* oseq = overlap_start_item_seq::type_id::create("oseq");
      oseq->second_sqr = sqr;
      sc_core::sc_process_handle ho = sc_core::sc_spawn([&]() { oseq->start(sqr_full); });
      sc_core::wait(2, sc_core::SC_NS);
      oseq->kill();
      if (ho.valid() && !ho.terminated())
        sc_core::wait(ho.terminated_event());
      std::cout << "Fix6 overlap_kill itA_active=" << oseq->it_a.is_active()
                << " itB_active=" << oseq->it_b.is_active() << std::endl;
      overlap_start_item_seq::type_id::destroy(oseq);

      // Mode 4: abandoned start_item() before normal body() return
      abandoned_start_item_seq* aseq = abandoned_start_item_seq::type_id::create("aseq");
      aseq->start(sqr_full);
      std::cout << "Fix6 abandoned_finish active=" << aseq->it.is_active() << std::endl;
      abandoned_start_item_seq::type_id::destroy(aseq);

      // Mode 5: do_kill() deletes active item
      dokill_frees_item_seq* dseq = dokill_frees_item_seq::type_id::create("dseq");
      sc_core::sc_process_handle hd = sc_core::sc_spawn([&]() { dseq->start(sqr_full); });
      sc_core::wait(2, sc_core::SC_NS);
      dseq->kill();
      if (hd.valid() && !hd.terminated())
        sc_core::wait(hd.terminated_event());
      std::cout << "Fix6 dokill_freed_item ok=1 state=" << dseq->get_sequence_state() << std::endl;
      dokill_frees_item_seq::type_id::destroy(dseq);

      // Mode 1: local item destroyed between start_item() and finish_item() before kill()
      destroyed_item_seq* lseq = destroyed_item_seq::type_id::create("lseq");
      lseq->heap_mode = false;
      sc_core::sc_process_handle hl = sc_core::sc_spawn([&]() { lseq->start(sqr_full); });
      sc_core::wait(2, sc_core::SC_NS);
      lseq->kill();
      if (hl.valid() && !hl.terminated())
        sc_core::wait(hl.terminated_event());
      std::cout << "Fix6 local_destroyed_item ok=1 state=" << lseq->get_sequence_state() << std::endl;
      destroyed_item_seq::type_id::destroy(lseq);

      // Mode 2: heap item deleted between start_item() and finish_item() before kill()
      destroyed_item_seq* hseq = destroyed_item_seq::type_id::create("hseq");
      hseq->heap_mode = true;
      sc_core::sc_process_handle hh = sc_core::sc_spawn([&]() { hseq->start(sqr_full); });
      sc_core::wait(2, sc_core::SC_NS);
      hseq->kill();
      if (hh.valid() && !hh.terminated())
        sc_core::wait(hh.terminated_event());
      std::cout << "Fix6 heap_deleted_item ok=1 state=" << hseq->get_sequence_state() << std::endl;
      destroyed_item_seq::type_id::destroy(hseq);

      // Challenger Repro 2: assign_into_active_item (kill & finish_item) under UVM_FULL
      assign_active_item_seq* aiseq_kill = assign_active_item_seq::type_id::create("aiseq_kill");
      aiseq_kill->kill_mode = true;
      sc_core::sc_process_handle hak = sc_core::sc_spawn([&]() { aiseq_kill->start(sqr_full); });
      sc_core::wait(2, sc_core::SC_NS);
      aiseq_kill->kill();
      if (hak.valid() && !hak.terminated())
        sc_core::wait(hak.terminated_event());
      std::cout << "Fix6 assign_into_active_kill inst_same="
                << (aiseq_kill->observed_inst_before == aiseq_kill->observed_inst_after ? 1 : 0)
                << " val=" << aiseq_kill->observed_val_after
                << " active=" << aiseq_kill->it_b.is_active()
                << " tr_h=" << aiseq_kill->it_b.get_tr_handle()
                << " state=" << aiseq_kill->get_sequence_state() << std::endl;
      assign_active_item_seq::type_id::destroy(aiseq_kill);

      assign_active_item_seq* aiseq_fin = assign_active_item_seq::type_id::create("aiseq_fin");
      aiseq_fin->kill_mode = false;
      aiseq_fin->start(sqr_full);
      std::cout << "Fix6 assign_into_active_finish inst_same="
                << (aiseq_fin->observed_inst_before == aiseq_fin->observed_inst_after ? 1 : 0)
                << " val=" << aiseq_fin->observed_val_after
                << " active=" << aiseq_fin->active_after_finish
                << " tr_h=" << aiseq_fin->handle_after_finish << std::endl;
      assign_active_item_seq::type_id::destroy(aiseq_fin);
    }

    // Challenger Repro 1: sequences holding active start_item(ext_item) when run_test() returns
    {
      g_seq_dtor1 = new post_run_active_seq("post_seq1");
      g_item_dtor1 = new rec_item("post_item1");
      g_item_dtor1->value = 701;
      g_seq_dtor1->ext_item = g_item_dtor1;
      sc_core::sc_spawn([&]() { g_seq_dtor1->start(sqr_dtor1); });

      g_seq_dtor2 = new post_run_active_seq("post_seq2");
      g_item_dtor2 = new rec_item("post_item2");
      g_item_dtor2->value = 702;
      g_seq_dtor2->ext_item = g_item_dtor2;
      sc_core::sc_spawn([&]() { g_seq_dtor2->start(sqr_dtor2); });

      sc_core::wait(2, sc_core::SC_NS);
    }

    phase.drop_objection(this);
  }
};

int sc_main( int, char*[] )
{
  const std::string db_file = "seq_recording_test_db.log";
  std::remove(db_file.c_str());

  uvm_text_tr_database* db = dynamic_cast<uvm_text_tr_database*>(
    uvm_coreservice_t::get()->get_default_tr_database());
  db->set_file_name(db_file);

  run_test("test");

  // Challenger Repro 1: post-run_test() destruction orders while start_item(ext_item) is active
  delete g_sqr_dtor1;
  delete g_seq_dtor1;
  delete g_item_dtor1;
  g_sqr_dtor1 = nullptr;
  g_seq_dtor1 = nullptr;
  g_item_dtor1 = nullptr;
  std::cout << "Fix6 dtor_sqr_before_seq_item ok=1" << std::endl;

  delete g_seq_dtor2;
  delete g_item_dtor2;
  delete g_sqr_dtor2;
  g_seq_dtor2 = nullptr;
  g_item_dtor2 = nullptr;
  g_sqr_dtor2 = nullptr;
  std::cout << "Fix6 dtor_seq_before_item ok=1" << std::endl;

  db->close_db();
  std::vector<std::string> db_lines = collect_normalized_db(db_file);
  bool has_child_link = false;
  int open_count = 0;
  int close_count = 0;
  int free_count = 0;
  bool in_full_kill_section = false;
  for (size_t i = 0; i < db_lines.size(); i++)
  {
    if (db_lines[i].find("RELATION=child") != std::string::npos)
      has_child_link = true;
    if (db_lines[i].find("OPEN_RECORDER") != std::string::npos)
      open_count++;
    if (db_lines[i].find("CLOSE_RECORDER") != std::string::npos)
      close_count++;
    if (db_lines[i].find("FREE_RECORDER") != std::string::npos)
      free_count++;
    if (db_lines[i].find("NAME:fp ") != std::string::npos)
      in_full_kill_section = true;
    if (!in_full_kill_section)
      std::cout << db_lines[i] << std::endl;
  }
  std::cout << "Fix6 has_child_link=" << has_child_link << std::endl;
  std::cout << "Fix6 recorder_balanced=" << ((open_count == close_count && open_count == free_count) ? 1 : 0)
            << " open=" << open_count << " close=" << close_count << " free=" << free_count << std::endl;
  if (open_count != close_count || open_count != free_count)
  {
    for (size_t i = 0; i < db_lines.size(); i++)
    {
      if (db_lines[i].find("OPEN_RECORDER") != std::string::npos)
      {
        size_t pos = db_lines[i].find("TXH:");
        if (pos != std::string::npos)
        {
          size_t end = db_lines[i].find(' ', pos);
          std::string txh = db_lines[i].substr(pos, end - pos);
          bool closed = false;
          for (size_t j = i + 1; j < db_lines.size(); j++)
          {
            if (db_lines[j].find("CLOSE_RECORDER") != std::string::npos &&
                db_lines[j].find(txh + " ") != std::string::npos)
            {
              closed = true;
              break;
            }
          }
          if (!closed)
            std::cout << "UNCLOSED: " << db_lines[i] << std::endl;
        }
      }
    }
  }
  std::remove(db_file.c_str());
  return 0;
}
