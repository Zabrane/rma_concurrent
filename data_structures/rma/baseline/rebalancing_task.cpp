/**
 * Copyright (C) 2018 Dean De Leo, email: dleo[at]cwi.nl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "rebalancing_task.hpp"

#include <cassert>
#include <mutex>
#include <thread>

#include "rma/common/static_index.hpp"

#include "gate.hpp"
#include "packed_memory_array.hpp"
#include "pointer.hpp"
#include "storage.hpp"

using namespace std;

namespace data_structures::rma::baseline {

extern mutex _debug_mutex; // PackedMemoryArray.cpp
#define DEBUG
#define COUT_DEBUG_FORCE(msg) { scoped_lock<mutex> lock(_debug_mutex); std::cout << "[RebalancingTask::" << __FUNCTION__ << "] [" << this_thread::get_id() << "] " << msg << std::endl; }
#if defined(DEBUG)
    #define COUT_DEBUG(msg) COUT_DEBUG_FORCE(msg)
#else
    #define COUT_DEBUG(msg)
#endif

RebalancingTask::RebalancingTask(PackedMemoryArray* pma, RebalancingMaster* master, Gate* gate) : m_pma(pma), m_master(master), m_plan(pma->memory_pool()){
    m_plan.m_cardinality_after = gate->m_cardinality;
    m_plan.m_window_start = gate->m_window_start;
    m_plan.m_window_length = gate->m_window_length;
    m_window_id = gate->lock_id();
    m_rebalancing_window_computed = false;
    m_ptr_locks = pma->m_locks.get_unsafe();
    m_ptr_index = pma->m_index.get_unsafe();
    m_ptr_storage = &(pma->m_storage);
    m_forced_resize = pma->m_storage.m_number_segments >= 2 * pma->balanced_thresholds_cutoff() && (2*pma->m_cardinality) < pma->m_storage.capacity();
}

bool RebalancingTask::ready_for_execution() const {
    return m_wait_to_complete.empty() && m_rebalancing_window_computed;
}

bool RebalancingTask::is_subset_of(size_t lock_start, size_t lock_length) const {
    size_t extent_end = lock_start + lock_length;
    return lock_start <= get_lock_start() && extent_end >= get_lock_end();
}

bool RebalancingTask::overlaps(size_t lock_start, size_t lock_length) const {
    size_t this_start = get_lock_start();
    size_t this_end = get_lock_end() -1; // inclusive
    size_t lock_end = lock_start + lock_length -1; // inclusive
    return (lock_start <= this_start && lock_end >= this_start) ||
            (lock_start <= this_end && lock_end >= this_end);
}

bool RebalancingTask::is_superset_of(size_t lock_start, size_t lock_length) const {
    size_t extent_end = lock_start + lock_length;
    return get_lock_start() <= lock_start && extent_end <= get_lock_end();
}


int64_t RebalancingTask::get_lock_start() const noexcept {
    assert(get_window_start() % m_pma->get_segments_per_lock() == 0);
    return get_window_start() / m_pma->get_segments_per_lock();
}

int64_t RebalancingTask::get_lock_length() const noexcept {
    assert(get_window_length() % m_pma->get_segments_per_lock() == 0);
    return get_window_length() / m_pma->get_segments_per_lock();
}

int64_t RebalancingTask::get_lock_end() const noexcept {
    return get_lock_start() + get_lock_length();
}

void RebalancingTask::set_lock_window(int64_t lock_start, int64_t lock_length) noexcept {
    auto segments_per_lock = m_pma->get_segments_per_lock();
    m_plan.m_window_start = lock_start * segments_per_lock;
    m_plan.m_window_length = lock_length * segments_per_lock;
}

int64_t RebalancingTask::get_window_start() const noexcept {
    return m_plan.m_window_start;
}

int64_t RebalancingTask::get_window_length() const noexcept {
    return m_plan.m_window_length;
}

int64_t RebalancingTask::get_window_end() const noexcept {
    return get_window_start() + get_window_length();
}

std::ostream& operator<<(std::ostream& out, const RebalancingTask* task){
    if(task == nullptr){
        out << "{TASK: nullptr}";
        return out;
    } else {
        return operator<<(out, *task);
    }
}
std::ostream& operator<<(std::ostream& out, const RebalancingTask& task){
    out << "{TASK locks: [" << task.get_lock_start() << ", " << task.get_lock_end() << ") plan: " << task.m_plan << ", "
            "window_id: " << task.m_window_id << ", waiting for lock ID to rebalance first: " << task.m_blocked_on_lock <<
            ", rebalancing window computed? " << task.m_rebalancing_window_computed;
    if(task.m_wait_to_complete.empty()){
        out << ", no locks to wait";
    } else {
        out << ", waiting on locks: ";
        for(size_t i = 0; i < task.m_wait_to_complete.size(); i++){
            if(i > 0) out << ", ";
            out << task.m_wait_to_complete[i].m_lock_id << " (cardinality: " << task.m_wait_to_complete[i].m_cardinality << ")";
        }
    }
    out << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, const RebalancingTask::SubTask& subtask){
    out << "{SUBTASK position_start: " << subtask.m_position_start << ", position_end: " << subtask.m_position_end <<
            ", partition_start_id: " << subtask.m_partition_start_id << ", partition_start_offset: " << subtask.m_partition_start_offset <<
            ", partition_end_id: " << subtask.m_partition_end_id << ", partition_end_offset: " << subtask.m_partition_end_offset <<
            ", input_extent_start: " << subtask.m_input_extent_start << ", input_extent_end: " << subtask.m_input_extent_end <<
            ", output_extent_start: " << subtask.m_output_extent_start << ", output_extent_end: " << subtask.m_output_extent_end << "}";
    return out;
}

} // namespace
