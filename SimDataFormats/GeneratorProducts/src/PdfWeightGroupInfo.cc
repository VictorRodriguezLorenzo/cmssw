#include "SimDataFormats/GeneratorProducts/interface/PdfWeightGroupInfo.h"
#include <set>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>

namespace gen {
  void PdfWeightGroupInfo::copy(const PdfWeightGroupInfo& other) {
    WeightGroupInfo::copy(other);
    uncertaintyType_ = other.uncertaintyType();
    hasAlphasVars_ = other.hasAlphasVariations();
    alphasUpIndex_ = other.alphasUpIndex();
    alphasDownIndex_ = other.alphasDownIndex();
    parentLhapdfId_ = other.parentLhapdfId();
    parentLhapdfSize_ = other.parentLhapdfSize_;
    parentLhapdfError_ = other.parentLhapdfError_;
    lhaids_ = other.lhaids_;
  }

  PdfWeightGroupInfo* PdfWeightGroupInfo::clone() const { return new PdfWeightGroupInfo(*this); }
  void PdfWeightGroupInfo::addLhaid(int lhaid) {
    lhaids_.push_back(lhaid);

    if (parentLhapdfId_ < 0 || parentLhapdfSize_ == static_cast<size_t>(-1)) {
      setIsWellFormed(false);
      return;
    }

    const size_t nMembers = parentLhapdfSize_;
    const std::set<int> uniqueIds(lhaids_.begin(), lhaids_.end());
    // Reject duplicates and IDs outside the parent PDF set.
    if (uniqueIds.size() != lhaids_.size()) {
      setIsWellFormed(false);
      return;
    }

    for (const int id : uniqueIds) {
      if (id < parentLhapdfId_ || static_cast<size_t>(id - parentLhapdfId_) >= nMembers) {
        setIsWellFormed(false);
        return;
      }
    }

    const bool complete = uniqueIds.size() == nMembers;
    const bool variationsOnly =
        nMembers > 1 && uniqueIds.size() == nMembers - 1 && uniqueIds.count(parentLhapdfId_) == 0;

    setIsWellFormed(complete || variationsOnly);
  }

  void PdfWeightGroupInfo::updateAlphasInfo() {
    hasAlphasVars_ = false;
    alphasUpIndex_ = -1;
    alphasDownIndex_ = -1;

    if (parentLhapdfId_ < 0 || parentLhapdfSize_ == static_cast<size_t>(-1) || parentLhapdfSize_ < 3)
      return;

    if (parentLhapdfError_.find("+as") == std::string::npos)
      return;

    const int id1 = parentLhapdfId_ + static_cast<int>(parentLhapdfSize_) - 2;
    const int id2 = parentLhapdfId_ + static_cast<int>(parentLhapdfSize_) - 1;

    const auto it1 = std::find(lhaids_.begin(), lhaids_.end(), id1);
    const auto it2 = std::find(lhaids_.begin(), lhaids_.end(), id2);

    if (it1 == lhaids_.end() || it2 == lhaids_.end())
      return;

    if (std::count(lhaids_.begin(), lhaids_.end(), id1) != 1 || std::count(lhaids_.begin(), lhaids_.end(), id2) != 1)
      return;

    try {
      std::unique_ptr<LHAPDF::PDF> pdf1(LHAPDF::mkPDF(id1));
      std::unique_ptr<LHAPDF::PDF> pdf2(LHAPDF::mkPDF(id2));

      const double as1 = pdf1->alphasQ(91.1876);
      const double as2 = pdf2->alphasQ(91.1876);

      if (!std::isfinite(as1) || !std::isfinite(as2) || as1 == as2)
        return;

      const int index1 = std::distance(lhaids_.begin(), it1);
      const int index2 = std::distance(lhaids_.begin(), it2);

      alphasUpIndex_ = (as1 > as2) ? index1 : index2;
      alphasDownIndex_ = (as1 < as2) ? index1 : index2;
      hasAlphasVars_ = true;
    } catch (const std::exception&) {
      return;
    }
  }

  void PdfWeightGroupInfo::setParentLhapdfInfo(int lhaid) {
    parentLhapdfId_ = lhaid;

    LHAPDF::PDFSet pdfSet(LHAPDF::lookupPDF(lhaid).first);
    parentLhapdfSize_ = pdfSet.size();
    parentLhapdfError_ = pdfSet.errorType();

    std::string errorType = parentLhapdfError_;
    std::transform(
        errorType.begin(), errorType.end(), errorType.begin(), [](unsigned char c) { return std::tolower(c); });

    if (errorType.find("hessian") != std::string::npos) {
      uncertaintyType_ = kHessianUnc;
    } else if (errorType.find("replica") != std::string::npos || errorType.find("replicas") != std::string::npos) {
      uncertaintyType_ = kMonteCarloUnc;
    } else {
      uncertaintyType_ = kUnknownUnc;
    }
  }

}  // namespace gen
