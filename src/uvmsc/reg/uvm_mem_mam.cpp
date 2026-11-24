//----------------------------------------------------------------------
//   Copyright 2004-2009 Synopsys, Inc.
//   Copyright 2010-2011 Mentor Graphics Corporation
//   Copyright 2010 Cadence Design Systems, Inc.
//   Copyright 2013 NXP B.V.
//   Copyright 2014 Fraunhofer-Gesellschaft zur Foerderung
//					der angewandten Forschung e.V.
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

#include <algorithm>
#include <limits>
#include <random>

#include "uvmsc/base/uvm_coreservice_t.h"
#include "uvmsc/base/uvm_default_coreservice_t.h"
#include "uvmsc/misc/uvm_misc.h"
#include "uvmsc/reg/uvm_mem.h"
#include "uvmsc/reg/uvm_mem_mam.h"
#include "uvmsc/reg/uvm_mem_region.h"
#include "uvmsc/reg/uvm_mem_mam_policy.h"
#include "uvmsc/reg/uvm_mem_mam_cfg.h"

namespace uvm {

//----------------------------------------------------------------------
// Constructor
//
//! Create a new manager instance
//!
//! Create an instance of a memory allocation manager
//! with the specified name and configuration.
//! This instance manages all memory region allocation within
//! the address range specified in the configuration descriptor.
//!
//! If a reference to a memory abstraction class is provided, the memory
//! locations within the regions can be accessed through the region
//! descriptor, using the uvm_mem_region::read() and
//! uvm_mem_region::write() methods.
//----------------------------------------------------------------------

uvm_mem_mam::uvm_mem_mam( const std::string& name,
                          uvm_mem_mam_cfg* cfg,
                          uvm_mem* mem )
{
  m_cfg = cfg;
  m_memory = mem;
  m_in_use.clear();

  default_alloc = new uvm_mem_mam_policy();
  m_for_each_idx = -1;
  m_name = name;
  m_rng_seeded = false;
  m_rng_global_seed = 0;
  m_lineno = 0;
  m_fname = "";
}

//----------------------------------------------------------------------
// Member function: reconfigure
//
//! Reconfigure the manager
//!
//! Modify the maximum and minimum addresses of the address space managed by
//! the allocation manager, allocation mode, or locality.
//! The number of bytes per memory location cannot be modified
//! once an allocation manager has been constructed.
//! All currently allocated regions must fall within the new address space.
//!
//! Returns the previous configuration.
//!
//! if no new configuration is specified, simply returns the current
//! configuration.
//----------------------------------------------------------------------

uvm_mem_mam_cfg* uvm_mem_mam::reconfigure( uvm_mem_mam_cfg* cfg )
{
  uvm_mem_mam_cfg* reconf;
  uvm_coreservice_t* cs = uvm_coreservice_t::get();
  uvm_root* top = cs->get_root();

  if (cfg == nullptr)
    return m_cfg;

  // Cannot reconfigure n_bytes
  if (cfg->n_bytes != m_cfg->n_bytes) // TODO was !==
  {
    std::ostringstream str;
    str << "Cannot reconfigure Memory Allocation Manager with a different number of bytes ("
        << cfg->n_bytes
        << " !== "
        << m_cfg->n_bytes
        << ").";
    top->uvm_report_error("uvm_mem_mam", str.str(), UVM_LOW);
    return m_cfg;
  }

  // All currently allocated regions must fall within the new space
  for( unsigned int i = 0; i < m_in_use.size(); i++)
  {
    if (m_in_use[i]->get_start_offset() < cfg->start_offset ||
        m_in_use[i]->get_end_offset() > cfg->end_offset)
    {
      std::ostringstream str;
      str << "Cannot reconfigure Memory Allocation Manager with a currently "
          << "allocated region outside of the managed address range (["
          << m_in_use[i]->get_start_offset()
          << ":"
          << m_in_use[i]->get_end_offset()
          << "] outside of ["
          << cfg->start_offset
          << ":"
          << cfg->end_offset
          << "]).";
      top->uvm_report_error("uvm_mem_mam", str.str(), UVM_LOW);
      return m_cfg;
    }
  }

  reconf = m_cfg;
  m_cfg = cfg;

  return reconf;
}

//----------------------------------------------------------------------
// Group: Memory Management
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Member function: reserve_region
//
//! Reserve a specific memory region
//!
//! Reserve a memory region of the specified number of bytes
//! starting at the specified offset.
//! A descriptor of the reserved region is returned.
//! If the specified region cannot be reserved, null is returned.
//!
//! It may not be possible to reserve a region because
//! it overlaps with an already-allocated region or
//! it lies outside the address range managed
//! by the memory manager.
//!
//! Regions can be reserved to create "holes" in the managed address space.
//----------------------------------------------------------------------

uvm_mem_region* uvm_mem_mam::reserve_region( unsigned long start_offset,
                                             unsigned int n_bytes,
                                             const std::string& fname,
                                             int lineno )
{
  unsigned long _end_offset;
  uvm_mem_region* _region;

  m_fname = fname;
  m_lineno = lineno;

  if (m_cfg == nullptr || m_cfg->n_bytes == 0)
  {
    UVM_ERROR("RegModel", "Memory Allocation Manager has invalid configuration");
    return nullptr;
  }

  if (n_bytes == 0)
  {
    UVM_ERROR("RegModel", "Cannot reserve 0 bytes");
    return nullptr;
  }

  if (start_offset < m_cfg->start_offset)
  {
    std::ostringstream str;
    str << "Cannot reserve before start of memory space: 0x"
        << std::hex << start_offset
        << " < 0x"
        << std::hex << m_cfg->start_offset;
    UVM_ERROR("RegModel", str.str());
    return nullptr;
  }

  unsigned long loc_span = (n_bytes - 1) / m_cfg->n_bytes;
  if (start_offset > m_cfg->end_offset || loc_span > (m_cfg->end_offset - start_offset))
  {
    std::ostringstream str;
    str << "Cannot reserve past end of memory space: 0x"
        << std::hex << start_offset
        << " + 0x" << loc_span
        << " > 0x"
        << std::hex << m_cfg->end_offset
        << ".";
    UVM_ERROR("RegModel", str.str());
    return nullptr;
  }

  unsigned int max_uint = std::numeric_limits<unsigned int>::max();
  if (loc_span >= max_uint ||
      (loc_span + 1) > (static_cast<unsigned long>(max_uint) / m_cfg->n_bytes))
  {
    UVM_ERROR("RegModel", "Reserved region size exceeds maximum representable byte/location count");
    return nullptr;
  }

  _end_offset = start_offset + loc_span;
  n_bytes = static_cast<unsigned int>((loc_span + 1) * m_cfg->n_bytes);

  std::ostringstream inf;
  inf << "Attempting to reserve [0x"
      << std::hex << start_offset
      << ":0x"
      << std::hex << _end_offset
      << "]...";
  UVM_INFO("RegModel", inf.str(), UVM_MEDIUM);

  for ( m_in_use_itt it = m_in_use.begin();
        it != m_in_use.end();
        it++ )
  {
    if (start_offset <= (*it)->get_end_offset() &&
        _end_offset >= (*it)->get_start_offset())
    {
      // Overlap!
      std::ostringstream str;
      str << "Cannot reserve [0x"
          << std::hex << start_offset
          << ":0x"
          << std::hex << _end_offset
          << "] because it overlaps with "
          << (*it)->convert2string()
          << ".";
      UVM_ERROR("RegModel", str.str());
      return nullptr;
    }

    // Regions are stored in increasing start offset
    if (start_offset < (*it)->get_start_offset())
    {
      _region = new uvm_mem_region(start_offset, _end_offset,
        _end_offset - start_offset + 1, n_bytes, this);
      m_in_use.insert(it, _region);
      return _region;
    }
  }

  _region = new uvm_mem_region(start_offset, _end_offset,
      _end_offset - start_offset + 1, n_bytes, this);
  m_in_use.push_back(_region);

  return _region;
}


//----------------------------------------------------------------------
// Member function: request_region
//
//! Request and reserve a memory region
//!
//! Request and reserve a memory region of the specified number
//! of bytes starting at a random location.
//! If an policy is specified, it is randomized to determine
//! the start offset of the region.
//! If no policy is specified, the policy found in
//! the uvm_mem_mam::default_alloc class property is randomized.
//!
//! A descriptor of the allocated region is returned.
//! If no region can be allocated, nullptr is returned.
//!
//! It may not be possible to allocate a region because
//! there is no area in the memory with enough consecutive locations
//! to meet the size requirements or
//! because there is another contradiction when randomizing
//! the policy.
//!
//! If the memory allocation is configured to THRIFTY or NEARBY,
//! a suitable region is first sought procedurally.
//----------------------------------------------------------------------


uvm_mem_region* uvm_mem_mam::request_region( unsigned int n_bytes,
                                             uvm_mem_mam_policy* alloc,
                                             const std::string& fname,
                                             int lineno )
{
  m_fname = fname;
  m_lineno = lineno;

  if (m_cfg == nullptr || m_cfg->n_bytes == 0)
  {
    UVM_ERROR("RegModel", "Memory Allocation Manager has invalid configuration");
    return nullptr;
  }

  if (n_bytes == 0)
  {
    UVM_ERROR("RegModel", "Cannot request 0 bytes");
    return nullptr;
  }

  if (alloc == nullptr)
    alloc = default_alloc;

  unsigned long loc_span = (n_bytes - 1) / m_cfg->n_bytes;
  unsigned int max_uint = std::numeric_limits<unsigned int>::max();
  if (loc_span >= max_uint ||
      (loc_span + 1) > (static_cast<unsigned long>(max_uint) / m_cfg->n_bytes))
  {
    UVM_ERROR("RegModel", "Requested region size exceeds maximum addressable locations");
    return nullptr;
  }

  alloc->len        = static_cast<unsigned int>(loc_span + 1);
  alloc->min_offset = m_cfg->start_offset;
  alloc->max_offset = m_cfg->end_offset;
  alloc->in_use     = m_in_use;

  alloc->pre_randomize();

  if (alloc->len == 0 || alloc->max_offset < alloc->min_offset ||
      (alloc->len - 1) > (alloc->max_offset - alloc->min_offset))
  {
    UVM_ERROR("RegModel", "Unable to randomize policy: requested size exceeds available range");
    return nullptr;
  }

  struct mam_gap_t
  {
    unsigned long start;
    unsigned long end;
    unsigned long span; // inclusive span: end - start (location count = span + 1)
    bool adj_left;
    bool adj_right;
  };

  std::vector<mam_gap_t> gaps;
  unsigned long cursor = alloc->min_offset;
  bool left_is_region = false;
  bool exhausted_space = false;

  for (unsigned int i = 0; i < m_in_use.size(); ++i)
  {
    unsigned long r_start = m_in_use[i]->get_start_offset();
    unsigned long r_end   = m_in_use[i]->get_end_offset();

    if (r_end < alloc->min_offset)
      continue;

    if (r_start > alloc->max_offset)
      break;

    if (r_start > cursor)
    {
      unsigned long g_end = r_start - 1;
      bool has_right_adj = true;
      if (g_end > alloc->max_offset)
      {
        g_end = alloc->max_offset;
        has_right_adj = false;
      }
      unsigned long g_span = g_end - cursor;
      if (g_span >= (alloc->len - 1))
        gaps.push_back({cursor, g_end, g_span, left_is_region, has_right_adj});
    }

    if (r_end >= cursor)
    {
      if (r_end >= alloc->max_offset || r_end == std::numeric_limits<unsigned long>::max())
      {
        exhausted_space = true;
        break;
      }
      cursor = r_end + 1;
      left_is_region = true;
    }
  }

  if (!exhausted_space && cursor <= alloc->max_offset)
  {
    unsigned long g_span = alloc->max_offset - cursor;
    if (g_span >= (alloc->len - 1))
      gaps.push_back({cursor, alloc->max_offset, g_span, left_is_region, false});
  }

  if (gaps.empty())
  {
    UVM_ERROR("RegModel", "Unable to randomize policy: no contiguous space available");
    return nullptr;
  }

  if (!m_rng_seeded || m_rng_global_seed != uvm_get_random_seed())
  {
    m_rng_global_seed = uvm_get_random_seed();
    unsigned long long seed_val = 0xcbf29ce484222325ULL ^ m_rng_global_seed;
    seed_val *= 0x100000001b3ULL;
    for (std::string::size_type i = 0; i < m_name.size(); ++i)
    {
      seed_val ^= static_cast<unsigned char>(m_name[i]);
      seed_val *= 0x100000001b3ULL;
    }
    m_rng.seed(seed_val);
    m_rng_seeded = true;
  }

  bool picked = false;
  unsigned long chosen_start = alloc->min_offset;

  if (m_cfg->locality == NEARBY && !m_in_use.empty())
  {
    std::vector<std::pair<unsigned long, unsigned long> > adj_candidates; // (gap_span, offset)
    unsigned long min_adj_span = std::numeric_limits<unsigned long>::max();
    for (unsigned int i = 0; i < gaps.size(); ++i)
    {
      if (gaps[i].adj_left)
      {
        adj_candidates.push_back(std::make_pair(gaps[i].span, gaps[i].start));
        min_adj_span = std::min(min_adj_span, gaps[i].span);
      }
      if (gaps[i].adj_right)
      {
        unsigned long off = gaps[i].end - (alloc->len - 1);
        if (!gaps[i].adj_left || off != gaps[i].start)
        {
          adj_candidates.push_back(std::make_pair(gaps[i].span, off));
          min_adj_span = std::min(min_adj_span, gaps[i].span);
        }
      }
    }

    if (!adj_candidates.empty())
    {
      std::vector<unsigned long> filtered;
      for (unsigned int i = 0; i < adj_candidates.size(); ++i)
      {
        if (m_cfg->mode != THRIFTY || adj_candidates[i].first == min_adj_span)
          filtered.push_back(adj_candidates[i].second);
      }
      std::uniform_int_distribution<std::size_t> dist(0, filtered.size() - 1);
      chosen_start = filtered[dist(m_rng)];
      picked = true;
    }
  }

  if (!picked)
  {
    unsigned long min_span = std::numeric_limits<unsigned long>::max();
    if (m_cfg->mode == THRIFTY)
    {
      for (unsigned int i = 0; i < gaps.size(); ++i)
        min_span = std::min(min_span, gaps[i].span);
    }

    std::vector<const mam_gap_t*> chosen_gaps;
    for (unsigned int i = 0; i < gaps.size(); ++i)
    {
      if (m_cfg->mode == THRIFTY && gaps[i].span != min_span)
        continue;
      chosen_gaps.push_back(&gaps[i]);
    }

    if (chosen_gaps.size() == 1)
    {
      unsigned long max_delta = chosen_gaps[0]->span - (alloc->len - 1);
      std::uniform_int_distribution<unsigned long> dist(0, max_delta);
      chosen_start = chosen_gaps[0]->start + dist(m_rng);
    }
    else
    {
      unsigned long long total_choices = 0;
      for (unsigned int i = 0; i < chosen_gaps.size(); ++i)
      {
        unsigned long long gap_choices =
            static_cast<unsigned long long>(chosen_gaps[i]->span - (alloc->len - 1)) + 1ULL;
        total_choices += gap_choices;
      }

      std::uniform_int_distribution<unsigned long long> dist(0, total_choices - 1);
      unsigned long long pick = dist(m_rng);
      for (unsigned int i = 0; i < chosen_gaps.size(); ++i)
      {
        unsigned long long gap_choices =
            static_cast<unsigned long long>(chosen_gaps[i]->span - (alloc->len - 1)) + 1ULL;
        if (pick < gap_choices)
        {
          chosen_start = chosen_gaps[i]->start + static_cast<unsigned long>(pick);
          break;
        }
        pick -= gap_choices;
      }
    }
  }

  alloc->start_offset = chosen_start;

  if (!alloc->randomize())
  {
    UVM_ERROR("RegModel", "Unable to randomize policy");
    return nullptr;
  }

  alloc->post_randomize();

  return reserve_region(alloc->start_offset, n_bytes, fname, lineno);
}


//----------------------------------------------------------------------
// Member function: release_region
//
//! Release the specified region
//!
//! Release a previously allocated memory region.
//! An error is issued if the
//! specified region has not been previously allocated or
//! is no longer allocated.
//----------------------------------------------------------------------

void uvm_mem_mam::release_region( uvm_mem_region* region )
{
  if (region == nullptr) return;

  for( m_in_use_itt it = m_in_use.begin();
       it != m_in_use.end();
       it++ )
  {
    if ((*it) == region)
    {
      delete (*it); // clear memory alloc
      m_in_use.erase(it); // delete entry from vector list
      return;
    }
  }
  UVM_ERROR("RegModel", "Attempting to release unallocated region\n" +
      region->convert2string());
}

//----------------------------------------------------------------------
// Member function: release_all_regions
//
// Forcibly release all allocated memory regions.
//----------------------------------------------------------------------

void uvm_mem_mam::release_all_regions()
{
  // TODO check correctness of clean-up
  for( m_in_use_itt it = m_in_use.begin();
       it != m_in_use.end();
       it++ )
    delete (*it); // clear memory alloc

  m_in_use.clear(); // clear vector list with pointers
}



//----------------------------------------------------------------------
// Group: Introspection
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Member function: convert2string
//
//! Image of the state of the manager
//!
//! Create a human-readable description of the state of
//! the memory manager and the currently allocated regions.
//----------------------------------------------------------------------

std::string uvm_mem_mam::convert2string()
{
  std::ostringstream str;

  str << "Allocated memory regions:\n";

  for( unsigned int i = 0; i < m_in_use.size(); i++ )
  {
     str << "   "<< m_in_use[i]->convert2string() << "\n";
  }

  return str.str();
}

//----------------------------------------------------------------------
// Member function: for_each
//
//! Iterate over all currently allocated regions
//!
//! If reset is TRUE, reset the iterator
//! and return the first allocated region.
//! Returns nullptr when there are no additional allocated
//! regions to iterate on.
//----------------------------------------------------------------------

uvm_mem_region* uvm_mem_mam::for_each( bool reset )
{
   if (reset)
     m_for_each_idx = -1;

   m_for_each_idx++;

   if (m_for_each_idx >= (int)m_in_use.size())
      return nullptr;

   return m_in_use[m_for_each_idx];
}

//----------------------------------------------------------------------
// Member function: get_memory
//
//! Get the managed memory implementation
//!
//! Return the reference to the memory abstraction class
//! for the memory implementing
//! the locations managed by this instance of the allocation manager.
//! Returns nullptr if no
//! memory abstraction class was specified at construction time.
//----------------------------------------------------------------------

uvm_mem* uvm_mem_mam::get_memory() const
{
  return m_memory;
}

//----------------------------------------------------------------------
// Destructor
//
// Implementation defined
//----------------------------------------------------------------------

uvm_mem_mam::~uvm_mem_mam()
{
  delete default_alloc;

  release_all_regions();
}

} // namespace uvm
