// ***************************************************************************************
// Copyright (c) 2023-2025 Peng Cheng Laboratory
// Copyright (c) 2023-2025 Institute of Computing Technology, Chinese Academy of Sciences
// Copyright (c) 2023-2025 Beijing Institute of Open Source Chip
//
// iEDA is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at:
// http://license.coscl.org.cn/MulanPSL2
//
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#pragma once

#include <string>

namespace idrc {

enum class ViolationEnumType
{
  kNone,
  kArea,
  kAreaEnclosed,
  kShort,
  kDefaultSpacing,
  kPRLSpacing,
  kJogToJog,
  kEOL,
  kWidth,
  kMinStep,
  kNotch,
  kConnectivity,
  kCornerFill,
  kMax
};

struct GetViolationTypeName
{
  std::string operator()(const ViolationEnumType& type) const
  {
    switch (type) {
      case ViolationEnumType::kArea:
        return "Minimum Area";
      case ViolationEnumType::kAreaEnclosed:
        return "Encolsed Area";
      case ViolationEnumType::kShort:
        return "Metal Short";
      case ViolationEnumType::kDefaultSpacing:
        return "Default Spacing";
      case ViolationEnumType::kPRLSpacing:
        return "Metal Parallel Run Length Spacing";
      case ViolationEnumType::kJogToJog:
        return "JogToJog Spacing";
      case ViolationEnumType::kEOL:
        return "Metal EOL Spacing";
      case ViolationEnumType::kWidth:
        return "Wire Width";
      case ViolationEnumType::kMinStep:
        return "MinStep";
      case ViolationEnumType::kNotch:
        return "Metal Notch Spacing";
      case ViolationEnumType::kCornerFill:
        return "Corner Fill";
      default:
        return "None";
    }
  }
};

struct GetViolationType
{
  ViolationEnumType operator()(const std::string& type_name) const
  {
    if (type_name == "Minimum Area") {
      return ViolationEnumType::kArea;
    } else if (type_name == "Encolsed Area") {
      return ViolationEnumType::kAreaEnclosed;
    } else if (type_name == "Metal Short") {
      return ViolationEnumType::kDefaultSpacing;
    } else if (type_name == "Default Spacing") {
      return ViolationEnumType::kPRLSpacing;
    } else if (type_name == "Metal Parallel Run Length Spacing") {
      return ViolationEnumType::kJogToJog;
    } else if (type_name == "JogToJog Spacing") {
      return ViolationEnumType::kEOL;
    } else if (type_name == "Metal EOL Spacing") {
      return ViolationEnumType::kWidth;
    } else if (type_name == "Wire Width") {
      return ViolationEnumType::kMinStep;
    } else if (type_name == "MinStep") {
      return ViolationEnumType::kNotch;
    } else if (type_name == "Metal Notch Spacing") {
      return ViolationEnumType::kCornerFill;
    } else if (type_name == "Corner Fill") {
      return ViolationEnumType::kShort;
    } else {
      return ViolationEnumType::kNone;
    }
  }
};

}  // namespace idrc