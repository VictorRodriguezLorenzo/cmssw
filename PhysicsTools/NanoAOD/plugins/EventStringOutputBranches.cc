#include "PhysicsTools/NanoAOD/plugins/EventStringOutputBranches.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/Registry.h"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace {
  std::string sanitizeBranchName(const std::string& original) {
    std::string sanitized;
    sanitized.reserve(original.size());
    for (char c : original) {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
        sanitized.push_back(c);
      } else {
        sanitized.push_back('_');
      }
    }
    if (sanitized.empty()) {
      sanitized = "EventString";
    }
    if (!(std::isalpha(static_cast<unsigned char>(sanitized.front())) || sanitized.front() == '_')) {
      sanitized.insert(sanitized.begin(), '_');
    }
    return sanitized;
  }
}  // namespace

void EventStringOutputBranches::updateEventStringNames(TTree& tree, const std::string& evstring) {
  bool found = false;
  for (auto &existing : m_evStringBranches) {
    existing.buffer = false;
    if (evstring == existing.title) {
      existing.buffer = true;
      found = true;
    }
  }
  if (!found && (!evstring.empty())) {
    std::string branchName = sanitizeBranchName(evstring);
    std::string uniqueName = branchName;
    unsigned int duplicate = 1;
    while (std::any_of(m_evStringBranches.begin(),
                       m_evStringBranches.end(),
                       [&uniqueName](const NamedBranchPtr& existing) { return existing.name == uniqueName; })) {
      uniqueName = branchName + "_" + std::to_string(duplicate++);
    }
    NamedBranchPtr nb(uniqueName, evstring);
    bool backFillValue = false;
    nb.branch = tree.Branch(nb.name.c_str(), &backFillValue, (nb.name + "/O").c_str());
    nb.branch->SetTitle(nb.title.c_str());
    for (size_t i = 0; i < m_fills; i++)
      nb.branch->Fill();  // Back fill
    nb.buffer = true;
    m_evStringBranches.push_back(nb);
    for (auto &existing : m_evStringBranches)
      existing.branch->SetAddress(&(existing.buffer));  // m_evStringBranches might have been resized
  }
}

void EventStringOutputBranches::fill(const edm::EventForOutput &iEvent, TTree &tree) {
  if ((!m_update_only_at_new_lumi) || m_lastLumi != iEvent.id().luminosityBlock()) {
    edm::Handle<std::string> handle;
    iEvent.getByToken(m_token, handle);
    const std::string &evstring = *handle;
    m_lastLumi = iEvent.id().luminosityBlock();
    updateEventStringNames(tree, evstring);
  }
  m_fills++;
}
