#include "SimDataFormats/GeneratorProducts/interface/ScaleWeightGroupInfo.h"

#include <iostream>
#include <stdexcept>

namespace gen {
  void ScaleWeightGroupInfo::copy(const ScaleWeightGroupInfo& other) {
    WeightGroupInfo::copy(other);
    muIndices_ = other.muIndices_;
    dynVec_ = other.dynVec_;
    dynNames_ = other.dynNames_;
    isFunctionalFormVar_ = other.isFunctionalFormVar_;
    containsCentral_ = other.containsCentral_;
    lhaid_ = other.lhaid_;
    weightIsCorrupt_ = other.weightIsCorrupt_;
  }

  ScaleWeightGroupInfo* ScaleWeightGroupInfo::clone() const { return new ScaleWeightGroupInfo(*this); }

  int ScaleWeightGroupInfo::addContainedId(int globalIndex, std::string id, std::string label, float muR, float muF) {
    int idxDiff = firstId_ - globalIndex;
    if (idxDiff > 0) {
      for (auto& entry : muIndices_) {
        if (entry != static_cast<size_t>(-1))
          entry += idxDiff;
      }
      for (auto& subVec : dynVec_) {
        for (auto& entry : subVec) {
          if (entry != static_cast<size_t>(-1))
            entry += idxDiff;
        }
      }
    }
    WeightGroupInfo::addContainedId(globalIndex, id, label);
    return setMuRMuFIndex(globalIndex, id, muR, muF);
  }

  int ScaleWeightGroupInfo::setMuRMuFIndex(int globalIndex, std::string id, float muR, float muF) {
    auto info = weightMetaInfoByGlobalIndex(id, globalIndex);
    int index = indexFromMus(muR, muF);
    if (!(isValidValue(muR) && isValidValue(muF))) {
      setWeightIsCorrupt();
      return 1;
    }
    if (index == Central_idx)
      containsCentral_ = true;
    muIndices_[index] = info.localIndex;

    for (int muidx : muIndices_) {
      if (muidx == -1) {
        return 2;
      }
    }
    isWellFormed_ = !weightIsCorrupt_;
    return 0;
  }

  void ScaleWeightGroupInfo::setDyn(
      int globalIndex, std::string id, float muR, float muF, size_t dynNum, std::string_view dynName) {
    if (!isValidValue(muR) || !isValidValue(muF)) {
      setWeightIsCorrupt();
      return;
    }

    const auto info = weightMetaInfoByGlobalIndex(id, globalIndex);
    const int index = indexFromMus(muR, muF);
    const size_t invalidIndex = static_cast<size_t>(-1);

    if (dynVec_[index].size() <= dynNum) {
      for (auto& dynIt : dynVec_)
        dynIt.resize(dynNum + 1, invalidIndex);
      dynNames_.resize(dynNum + 1);
    }

    if (!dynName.empty()) {
      if (!dynNames_[dynNum].empty() && dynNames_[dynNum] != dynName) {
        setWeightIsCorrupt();
        return;
      }
      dynNames_[dynNum] = std::string(dynName);
    }

    if (dynVec_[index][dynNum] != invalidIndex) {
      setWeightIsCorrupt();
      return;
    }

    dynVec_[index][dynNum] = info.localIndex;
  }

  size_t ScaleWeightGroupInfo::scaleIndex(float muR, float muF, std::string_view dynName) const {
    auto it = std::find(dynNames_.begin(), dynNames_.end(), dynName);
    if (it == dynNames_.end())
      return -1;
    else
      return scaleIndex(muR, muF, it - dynNames_.begin());
  }

  size_t ScaleWeightGroupInfo::scaleIndex(int index, std::string_view dynName) const {
    auto it = std::find(dynNames_.begin(), dynNames_.end(), dynName);
    if (it == dynNames_.end())
      return -1;
    else
      return scaleIndex(index, it - dynNames_.begin());
  }

  size_t ScaleWeightGroupInfo::scaleIndex(float muR, float muF, size_t dynNum) const {
    ;
    if (!(isValidValue(muR) && isValidValue(muF)) || dynNum + 1 > dynNames_.size())
      return -1;
    else
      return scaleIndex(indexFromMus(muR, muF), dynNum);
  }
  size_t ScaleWeightGroupInfo::scaleIndex(float muR, float muF) const {
    if (!(isValidValue(muR) && isValidValue(muF)))
      return -1;
    else
      return muIndices_.at(indexFromMus(muR, muF));
  }

  std::vector<std::string> ScaleWeightGroupInfo::dynNames() const {
    std::vector<std::string> returnVec;
    for (const auto& item : dynNames_) {
      if (!item.empty())
        returnVec.push_back(item);
    }
    return returnVec;
  }

  size_t ScaleWeightGroupInfo::getOrAddDynamicScaleIndex(const std::string& name) {
    auto it = std::find(dynNames_.begin(), dynNames_.end(), name);
    if (it != dynNames_.end())
      return std::distance(dynNames_.begin(), it);
    dynNames_.push_back(name);
    return dynNames_.size() - 1;
  }

}  // namespace gen
