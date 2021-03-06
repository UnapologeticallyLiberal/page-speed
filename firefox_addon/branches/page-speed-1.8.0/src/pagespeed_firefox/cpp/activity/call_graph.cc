/**
 * Copyright 2008-2009 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// CallGraph implementation.  See call_graph.h for details.

#include "call_graph.h"

#include "base/logging.h"
#include "call_graph_visitor_interface.h"
#include "profile.pb.h"
#include "timer.h"

namespace activity {

CallGraph::CallGraph(Profile *profile, Timer *timer)
    : profile_(profile),
      timer_(timer) {
}

CallGraph *CallGraph::CreateSnapshot() const {
  CallGraph *graph = new CallGraph(profile_, timer_);
  graph->call_trees_ = call_trees_;
  graph->working_set_ = working_set_;
  return graph;
}

bool CallGraph::OnFunctionEntry(int32 tag) {
  CallTree* node = NULL;
  if (working_set_.empty()) {
    node = profile_->add_call_tree();
  } else {
    node = working_set_.back()->add_children();
  }

  node->set_function_tag(tag);
  node->set_entry_time_usec(timer_->GetElapsedTimeUsec());
  working_set_.push_back(node);
  return true;
}

bool CallGraph::OnFunctionExit(int32 tag) {
  if (working_set_.empty()) {
    LOG(DFATAL) << "Working set is empty.";
    return false;
  }

  CallTree* node = working_set_.back();
  if (node->function_tag() != tag) {
    // Special case: due to
    // https://bugzilla.mozilla.org/show_bug.cgi?id=546637, the entry
    // and exit callbacks are mismatched by one when we're coming out
    // of a paused profiler. Thus we need to pop one node in the event
    // that we find a mismatch. This is hacky and should be removed
    // once the mozilla bug is fixed.
    node->set_exit_time_usec(timer_->GetElapsedTimeUsec());
    working_set_.pop_back();
    if (working_set_.empty()) {
      LOG(DFATAL) << "Working set is empty.";
      return false;
    }
    node = working_set_.back();
  }

  if (node->function_tag() != tag) {
    LOG(DFATAL) << "Unable to match tags.";
    // At this point the call stack is in an inconsistent state, so we
    // clear it and signal to the caller that we failed to update our
    // internal state appropriately.
    working_set_.clear();
    return false;
  }
  node->set_exit_time_usec(timer_->GetElapsedTimeUsec());
  working_set_.pop_back();

  if (working_set_.empty()) {
    call_trees_.push_back(node);
  }

  return true;
}

bool CallGraph::IsPartiallyConstructed() const {
  return !working_set_.empty();
}

void CallGraph::Traverse(CallGraphVisitorInterface* visitor) const {
  for (CallForest::const_iterator it = call_trees_.begin(),
           end = call_trees_.end();
       it != end;
       ++it) {
    std::vector<const CallTree*> stack;
    const CallTree* call_tree = *it;
    if (call_tree == NULL) {
      LOG(DFATAL) << "Unable to visit null call tree.";
      return;
    }
    CallGraphVisitorInterface::Traverse(visitor, *call_tree, &stack);
  }
}

}  // namespace activity
