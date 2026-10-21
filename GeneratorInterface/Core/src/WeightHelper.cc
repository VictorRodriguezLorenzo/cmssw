#include "GeneratorInterface/Core/interface/WeightHelper.h"

#include <regex>

#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

namespace gen {
  WeightHelper::WeightHelper() {}

  bool WeightHelper::isScaleWeightGroup(const ParsedWeight& weight) const {
    if (weight.groupname.find("scale_variation") != std::string::npos ||
        weight.groupname.find("Central scale variation") != std::string::npos)
      return true;
  
    if (weight.groupname.empty()) {
      const auto& content = weight.content;
      return content.find("dyn_scale_choice=") != std::string::npos ||
             content.find("DYN_SCALE=") != std::string::npos ||
             content.find("MUR=") != std::string::npos ||
             content.find("MUF=") != std::string::npos;
    }
  
    return false;
  }

  bool WeightHelper::isPdfWeightGroup(const ParsedWeight& weight) const {
    const std::string& name = weight.groupname;
    if (name.find("PDF_variation") != std::string::npos)
      return true;
    return LHAPDF::lookupLHAPDFID(name) != -1;
  }

  bool WeightHelper::isPartonShowerWeightGroup(const ParsedWeight& weight) const {
    const std::string& groupname = boost::to_lower_copy(weight.groupname);
    std::vector<std::string> psNames = {"isr", "fsr", "nominal", "baseline", "emission"};
    for (const auto& name : psNames) {
      if (groupname.find(name) != std::string::npos)
        return true;
    }
    return false;
  }

  bool WeightHelper::isOrphanPdfWeightGroup(ParsedWeight& weight) const {
    std::pair<std::string, int> pairLHA;
    try {
      pairLHA = LHAPDF::lookupPDF(stoi(searchAttributes("pdf", weight)));
    } catch (...) {
      return false;
    }

    if (!pairLHA.first.empty() && pairLHA.second == 0) {
      weight.groupname = std::string(pairLHA.first);
      return true;
    } else {
      return false;
    }
  }

  bool WeightHelper::isMEParamWeightGroup(const ParsedWeight& weight) const {
    return (weight.groupname.find("mg_reweighting") != std::string::npos ||
            weight.groupname.find("variation") != std::string::npos);
    // variation used for blanket of all variations, might need to change
  }

  std::string WeightHelper::searchAttributes(const std::string& label, const ParsedWeight& weight) const {
    std::string attribute = searchAttributesByTag(label, weight);
    return attribute.empty() ? searchAttributesByRegex(label, weight) : attribute;
  }

  std::string WeightHelper::searchAttributesByTag(const std::string& label, const ParsedWeight& weight) const {
    auto& attributes = weight.attributes;
    for (const auto& lab : attributeNames_.at(label)) {
      if (attributes.find(lab) != attributes.end()) {
        return boost::algorithm::trim_copy_if(attributes.at(lab), boost::is_any_of("\""));
      }
    }
    return "";
  }

  std::string WeightHelper::searchAttributesByRegex(const std::string& label, const ParsedWeight& weight) const {
    auto& content = weight.content;
    std::smatch match;
    for (const auto& lab : attributeNames_.at(label)) {
      std::regex floatExpr(lab + "\\s*=\\s*([0-9.]+(?:[eE][+-]?[0-9]+)?)");
      std::regex strExpr(lab + "\\s*=\\s*([^=]+)");
      if (std::regex_search(content, match, floatExpr)) {
        return boost::algorithm::trim_copy(match.str(1));
      } else if (std::regex_search(content, match, strExpr)) {
        return boost::algorithm::trim_copy(match.str(1));
      }
    }
    return "";
  }

  void WeightHelper::updateScaleInfo(gen::ScaleWeightGroupInfo& scaleGroup, const ParsedWeight& weight) const {
    const std::string muRText = searchAttributes("mur", weight);
    const std::string muFText = searchAttributes("muf", weight);
    const std::string dynNumText = searchAttributes("dyn", weight);
    const std::string dynName = searchAttributes("dyn_name", weight);
    float muR = 1.0f, muF = 1.0f;
    try {
      size_t pos = 0;

      if (!muRText.empty()) {
        muR = std::stof(muRText, &pos);
        if (pos != muRText.size())
          throw std::invalid_argument("Invalid muR");
      }

      if (!muFText.empty()) {
        muF = std::stof(muFText, &pos);
        if (pos != muFText.size())
          throw std::invalid_argument("Invalid muF");
      }
    } catch (const std::exception& e) {
      if (debug_)
        std::cout << "Invalid scale weight: " << weight.content << " (" << e.what() << ")" << std::endl;
      scaleGroup.setWeightIsCorrupt();
      return;
    }

    if (!ScaleWeightGroupInfo::isValidValue(muR) || !ScaleWeightGroupInfo::isValidValue(muF)) {
      scaleGroup.setWeightIsCorrupt();
      return;
    }

    if (debug_)
      std::cout << "Scale weight " << weight.id << " | content=" << weight.content << " | muR=" << muR
                << " | muF=" << muF << " | dyn=" << dynNumText << " | dyn_name=" << dynName << std::endl;

    if (dynNumText.empty() && dynName.empty()) {
      scaleGroup.setMuRMuFIndex(weight.index, weight.id, muR, muF);
    } else {
      try {
        size_t dynNum = 0;

        if (!dynNumText.empty()) {
          size_t pos = 0;
          long parsed = std::stol(dynNumText, &pos);
          if (pos != dynNumText.size() || parsed < 0)
            throw std::invalid_argument("Invalid dynamic scale index");
          dynNum = static_cast<size_t>(parsed);
        } else {
          dynNum = scaleGroup.getOrAddDynamicScaleIndex(dynName);
        }

        scaleGroup.setDyn(weight.index, weight.id, muR, muF, dynNum, dynName);
      } catch (const std::exception& e) {
        if (debug_)
          std::cout << "Invalid dynamic scale: " << weight.content << " (" << e.what() << ")" << std::endl;
        scaleGroup.setWeightIsCorrupt();
      }
    }

    if (scaleGroup.lhaid() == -1) {
      const std::string lhaidText = searchAttributes("pdf", weight);

      if (!lhaidText.empty()) {
        try {
          size_t pos = 0;
          int lhaid = std::stoi(lhaidText, &pos);
          if (pos != lhaidText.size())
            throw std::invalid_argument("Invalid PDF ID");
          scaleGroup.setLhaid(lhaid);
        } catch (const std::exception&) {
          scaleGroup.setWeightIsCorrupt();
        }
      }
    }
  }

  int WeightHelper::lhapdfId(const ParsedWeight& weight, gen::PdfWeightGroupInfo& pdfGroup) const {
    const std::string pdfText = searchAttributes("pdf", weight);
    const std::string memberText = searchAttributes("member", weight);

    try {
      auto parseInteger = [](const std::string& text) {
        size_t pos = 0;
        int value = std::stoi(text, &pos);
        if (pos != text.size() || value < 0)
          throw std::invalid_argument("Invalid LHAPDF integer");
        return value;
      };

      if (!pdfText.empty()) {
        const int pdfId = parseInteger(pdfText);

        if (!memberText.empty()) {
          const int memberId = parseInteger(memberText);
          const auto pdfInfo = LHAPDF::lookupPDF(pdfId);
          if (pdfInfo.first.empty())
            throw std::invalid_argument("Unknown LHAPDF set");

          const int parentId = pdfId - pdfInfo.second;
          const int actualId = parentId + memberId;
          const auto memberInfo = LHAPDF::lookupPDF(actualId);
          if (memberInfo.first != pdfInfo.first || memberInfo.second != memberId)
            throw std::invalid_argument("PDF member is not part of the declared set");
          if (pdfInfo.second != 0 && pdfInfo.second != memberId)
            throw std::invalid_argument("Conflicting PDF and MemberID attributes");

          return actualId;
        }
        return pdfId;
      }

      if (!memberText.empty()) {
        if (pdfGroup.parentLhapdfId() >= 0)
          return pdfGroup.parentLhapdfId() + parseInteger(memberText);

        const int parentId = LHAPDF::lookupLHAPDFID(weight.groupname);
        if (parentId >= 0)
          return parentId + parseInteger(memberText);

        return -1;
      }
      if (!pdfGroup.lhaIds().empty())
        return pdfGroup.lhaIds().back() + 1;

      return LHAPDF::lookupLHAPDFID(weight.groupname);

    } catch (const std::exception& e) {
      if (debug_)
        std::cout << "Invalid PDF weight: " << weight.content << " (" << e.what() << ")" << std::endl;

      pdfGroup.setIsWellFormed(false);
      return -1;
    }
  }

  void WeightHelper::updatePdfInfo(gen::PdfWeightGroupInfo& pdfGroup, const ParsedWeight& weight) const {
    const int lhaid = lhapdfId(weight, pdfGroup);

    if (lhaid < 0) {
      pdfGroup.setIsWellFormed(false);
      return;
    }

    if (debug_)
      std::cout << "LHAID identified as " << lhaid << std::endl;

    if (pdfGroup.parentLhapdfId() < 0) {
      const auto pdfInfo = LHAPDF::lookupPDF(lhaid);

      if (pdfInfo.first.empty()) {
        pdfGroup.setIsWellFormed(false);
        return;
      }

      const int parentId = lhaid - pdfInfo.second;
      pdfGroup.setParentLhapdfInfo(parentId);

      std::string description;

      if (pdfGroup.uncertaintyType() == gen::kHessianUnc)
        description += "Hessian ";
      else if (pdfGroup.uncertaintyType() == gen::kMonteCarloUnc)
        description += "Monte Carlo ";

      description += "Uncertainty sets for LHAPDF set " + pdfInfo.first;
      description += " with LHAID = " + std::to_string(parentId) + "; ";

      pdfGroup.appendDescription(description);
    }

    pdfGroup.addLhaid(lhaid);
  }

  void WeightHelper::updatePartonShowerInfo(gen::PartonShowerWeightGroupInfo& psGroup,
                                            const ParsedWeight& weight) const {
    if (psGroup.nIdsContained() == DEFAULT_PSWEIGHT_LENGTH) {
      psGroup.setIsWellFormed(true);
      psGroup.cacheWeightIndicesByLabel();
    }
    if (weight.content.find(':') != std::string::npos && weight.content.find('=') != std::string::npos)
      psGroup.setNameIsPythiaSyntax(true);
  }

  bool WeightHelper::splitPdfWeight(ParsedWeight& weight, WeightGroupInfoContainer& weightGroups) const {
    if (weightGroups[weight.wgtGroup_idx]->weightType() == gen::WeightType::kPdfWeights) {
      auto& pdfGroup = *static_cast<gen::PdfWeightGroupInfo*>(weightGroups[weight.wgtGroup_idx].get());
      int lhaid = lhapdfId(weight, pdfGroup);
      if (lhaid > 0 && !pdfGroup.isIdInParentSet(lhaid) && pdfGroup.parentLhapdfId() > 0) {
        weightGroups.push_back(buildGroup(weight));
        weight.wgtGroup_idx++;
        return true;
      }
    }
    return false;
  }

  void WeightHelper::cleanupOrphanCentralWeight(WeightGroupInfoContainer& weightGroups) const {
    auto centralIt = std::find_if(std::begin(weightGroups), std::end(weightGroups), [](auto& entry) {
      return entry->weightType() == gen::WeightType::kScaleWeights &&
             !(static_cast<ScaleWeightGroupInfo*>(entry.get())->containsCentralWeight());
    });
    if (centralIt == std::end(weightGroups))
      return;

    auto& centralWeight = *static_cast<gen::ScaleWeightGroupInfo*>(centralIt->get());

    std::vector<size_t> toRemove;
    for (size_t i = 0; i < weightGroups.size(); i++) {
      auto& group = weightGroups[i];
      if (group->weightType() == gen::WeightType::kPdfWeights) {
        auto& pdfGroup = *static_cast<gen::PdfWeightGroupInfo*>(group.get());
        // These are weights that contain nothing but a single central weight,
        // because some versions of madgraph write the central weight separately
        if (pdfGroup.nIdsContained() == 1 && pdfGroup.parentLhapdfId() == centralWeight.lhaid()) {
          toRemove.push_back(i);
          const auto& weightInfo = pdfGroup.weightMetaInfo(0);
          int ret = centralWeight.addContainedId(weightInfo.globalIndex, weightInfo.id, weightInfo.label, 1, 1);
          if (ret == 2 and debug_) {
            std::cout << "ScaleWeightGroupInfo::setMuRMuFIndex(): missing index, full list is: ";
            std::copy(centralWeight.muIndices().begin(),
                      centralWeight.muIndices().end(),
                      std::ostream_iterator<size_t>(std::cout, ","));
            std::cout << std::endl;
          }
        }
      }
    }
    // Indices are guaranteed to be unique, delete from high to low to avoid
    // changing indices
    std::sort(std::begin(toRemove), std::end(toRemove), std::greater<size_t>());
    for (auto i : toRemove) {
      weightGroups.erase(std::begin(weightGroups) + i);
    }
  }

  void WeightHelper::printWeights(const WeightGroupInfoContainer& weightGroups) const {
    // checks
    unsigned i = 0;
    for (const auto& group : weightGroups) {
      std::cout << std::boolalpha << i << ": " << group->name() << " ["
                << static_cast<std::underlying_type<gen::WeightType>::type>(group->weightType()) << "] ("
                << group->firstId() << "-" << group->lastId() << "): " << group->isWellFormed() << std::endl;
      if (group->weightType() == gen::WeightType::kScaleWeights) {
        const auto& groupScale = *static_cast<gen::ScaleWeightGroupInfo*>(group.get());
        std::cout << groupScale.centralIndex() << " ";
        std::cout << groupScale.muR1muF2Index() << " ";
        std::cout << groupScale.muR1muF05Index() << " ";
        std::cout << groupScale.muR2muF1Index() << " ";
        std::cout << groupScale.muR2muF2Index() << " ";
        std::cout << groupScale.muR2muF05Index() << " ";
        std::cout << groupScale.muR05muF1Index() << " ";
        std::cout << groupScale.muR05muF2Index() << " ";
        std::cout << groupScale.muR05muF05Index() << " \n";
        for (auto& name : groupScale.dynNames()) {
          std::cout << name << ": ";
          std::cout << groupScale.scaleIndex(1.0, 1.0, name) << " ";
          std::cout << groupScale.scaleIndex(1.0, 2.0, name) << " ";
          std::cout << groupScale.scaleIndex(1.0, 0.5, name) << " ";
          std::cout << groupScale.scaleIndex(2.0, 1.0, name) << " ";
          std::cout << groupScale.scaleIndex(2.0, 2.0, name) << " ";
          std::cout << groupScale.scaleIndex(2.0, 0.5, name) << " ";
          std::cout << groupScale.scaleIndex(0.5, 1.0, name) << " ";
          std::cout << groupScale.scaleIndex(0.5, 2.0, name) << " ";
          std::cout << groupScale.scaleIndex(0.5, 0.5, name) << "\n";
        }

      } else if (group->weightType() == gen::WeightType::kPdfWeights) {
        std::cout << group->description() << "\n";
      } else if (group->weightType() == gen::WeightType::kPartonShowerWeights) {
        const auto& groupPS = *static_cast<gen::PartonShowerWeightGroupInfo*>(group.get());
        std::vector<std::string> labels = groupPS.weightLabels();
        groupPS.printVariables();
      }
      ++i;
    }
  }

  std::unique_ptr<WeightGroupInfo> WeightHelper::buildGroup(ParsedWeight& weight) const {
    if (debug_) {
      std::cout << "Building group for weight group " << weight.groupname << std::endl;
      std::cout << "    weight content is " << weight.content << std::endl;
    }
    if (isScaleWeightGroup(weight)) {
      if (debug_)
        std::cout << "Weight type is scale\n";
      return std::make_unique<ScaleWeightGroupInfo>(weight.groupname);
    } else if (isPdfWeightGroup(weight)) {
      if (debug_)
        std::cout << "Weight type is PDF\n";
      return std::make_unique<PdfWeightGroupInfo>(weight.groupname);
    } else if (isMEParamWeightGroup(weight)) {
      if (debug_)
        std::cout << "Weight type is MEParam\n";
      return std::make_unique<MEParamWeightGroupInfo>(weight.groupname);
    } else if (isPartonShowerWeightGroup(weight)) {
      if (debug_)
        std::cout << "Weight type is parton shower\n";
      return std::make_unique<PartonShowerWeightGroupInfo>("shower");
    } else if (isOrphanPdfWeightGroup(weight)) {
      if (debug_)
        std::cout << "Weight type is PDF\n";
      return std::make_unique<PdfWeightGroupInfo>(weight.groupname);
    }
    if (debug_) {
      std::cout << "Weight type is unknown\n";
      std::cout << "Group name is " << weight.groupname << std::endl;
    }

    return std::make_unique<UnknownWeightGroupInfo>(weight.groupname);
  }

  WeightGroupInfoContainer WeightHelper::buildGroups(std::vector<ParsedWeight>& parsedWeights,
                                                     bool addUnassociated) const {
    WeightGroupInfoContainer weightGroups;
    int groupOffset = 0;
    for (auto& weight : parsedWeights) {
      weight.wgtGroup_idx += groupOffset;
      if (debug_) {
        std::cout << "Building group for weight " << weight.content << std::endl;
        std::cout << "    group " << weight.groupname << std::endl;
        std::cout << "    group index " << weight.wgtGroup_idx << std::endl;
      }

      int numGroups = static_cast<int>(weightGroups.size());
      if (weight.wgtGroup_idx == numGroups) {
        weightGroups.push_back(buildGroup(weight));
      } else if (weight.wgtGroup_idx >= numGroups)
        throw cms::Exception("Invalid group index " + std::to_string(weight.wgtGroup_idx));

      // split PDF groups
      if (splitPdfWeight(weight, weightGroups))
        groupOffset++;

      auto& group = weightGroups[weight.wgtGroup_idx];
      group->addContainedId(weight.index, weight.id, weight.content);
      if (group->weightType() == gen::WeightType::kScaleWeights)
        updateScaleInfo(*static_cast<gen::ScaleWeightGroupInfo*>(group.get()), weight);
      else if (group->weightType() == gen::WeightType::kPdfWeights)
        updatePdfInfo(*static_cast<gen::PdfWeightGroupInfo*>(group.get()), weight);
      else if (group->weightType() == gen::WeightType::kPartonShowerWeights)
        updatePartonShowerInfo(*static_cast<gen::PartonShowerWeightGroupInfo*>(group.get()), weight);
    }

    for (auto& group : weightGroups) {
      if (group->weightType() == gen::WeightType::kPdfWeights) {
        auto& pdfGroup = *static_cast<gen::PdfWeightGroupInfo*>(group.get());
        pdfGroup.updateAlphasInfo();
      }
    }

    if (debug_) {
      std::cout << "Groups BEFORE cleanup:" << std::endl;
      printWeights(weightGroups);
    }
    cleanupOrphanCentralWeight(weightGroups);
    if (addUnassociated) {
      addUnassociatedGroup(weightGroups);
    }
    if (debug_) {
      std::cout << "Groups AFTER cleanup:" << std::endl;
      printWeights(weightGroups);
    }
    return weightGroups;
  }

}  // namespace gen
