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

#include "ConnectType.hpp"
#include "DRBox.hpp"
#include "DRPin.hpp"
#include "GridMap.hpp"
#include "Guide.hpp"
#include "MTree.hpp"
#include "Net.hpp"
#include "PhysicalNode.hpp"
#include "Pin.hpp"
#include "TNode.hpp"

namespace irt {

class DRParameter
{
 public:
  DRParameter() = default;
  DRParameter(irt_int size, irt_int offset, irt_int fixed_rect_cost, irt_int routed_rect_cost, irt_int violation_cost, bool complete_ripup)
  {
    _size = size;
    _offset = offset;
    _fixed_rect_cost = fixed_rect_cost;
    _routed_rect_cost = routed_rect_cost;
    _violation_cost = violation_cost;
    _complete_ripup = complete_ripup;
  }
  ~DRParameter() = default;
  // getter
  irt_int get_size() const { return _size; }
  irt_int get_offset() const { return _offset; }
  irt_int get_fixed_rect_cost() const { return _fixed_rect_cost; }
  irt_int get_routed_rect_cost() const { return _routed_rect_cost; }
  irt_int get_violation_cost() const { return _violation_cost; }
  irt_int get_complete_ripup() const { return _complete_ripup; }
  // setter
  void set_size(const irt_int size) { _size = size; }
  void set_offset(const irt_int offset) { _offset = offset; }
  void set_fixed_rect_cost(const irt_int fixed_rect_cost) { _fixed_rect_cost = fixed_rect_cost; }
  void set_routed_rect_cost(const irt_int routed_rect_cost) { _routed_rect_cost = routed_rect_cost; }
  void set_violation_cost(const irt_int violation_cost) { _violation_cost = violation_cost; }
  void set_complete_ripup(const irt_int complete_ripup) { _complete_ripup = complete_ripup; }

 private:
  irt_int _size = -1;
  irt_int _offset = -1;
  irt_int _fixed_rect_cost = 0;
  irt_int _routed_rect_cost = 0;
  irt_int _violation_cost = 0;
  bool _complete_ripup = true;
};

}  // namespace irt
