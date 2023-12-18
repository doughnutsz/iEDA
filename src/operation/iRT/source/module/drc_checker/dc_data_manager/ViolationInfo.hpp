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

#include "LayerRect.hpp"
#include "RTU.hpp"
#include "RTUtil.hpp"

namespace irt {

class ViolationInfo
{
 public:
  ViolationInfo() = default;
  ~ViolationInfo() = default;
  // getter
  std::string get_rule_name() { return _rule_name; }
  LayerRect& get_violation_region() { return _violation_region; }
  bool get_is_routing() { return _is_routing; }
  std::set<BaseInfo, CmpBaseInfo>& get_base_info_set() { return _base_info_set; }
  // setter
  void set_rule_name(const std::string& rule_name) { _rule_name = rule_name; }
  void set_violation_region(const LayerRect& violation_region) { _violation_region = violation_region; }
  void set_is_routing(const bool is_routing) { _is_routing = is_routing; }
  void set_base_info_set(const std::set<BaseInfo, CmpBaseInfo>& base_info_set) { _base_info_set = base_info_set; }
  // function

 private:
  std::string _rule_name;
  LayerRect _violation_region;
  bool _is_routing = true;
  std::set<BaseInfo, CmpBaseInfo> _base_info_set;
};

}  // namespace irt
