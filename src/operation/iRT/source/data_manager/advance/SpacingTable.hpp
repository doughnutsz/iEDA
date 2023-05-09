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

#include "GridMap.hpp"
#include "RTU.hpp"

namespace irt {

class SpacingTable
{
 public:
  SpacingTable() = default;
  ~SpacingTable() = default;
  // getter
  std::vector<irt_int>& get_width_list() { return _width_list; }
  std::vector<irt_int>& get_parallel_length_list() { return _parallel_length_list; }
  GridMap<irt_int>& get_width_parallel_length_map() { return _width_parallel_length_map; }
  // setter
  void set_width_list(const std::vector<irt_int>& width_list) { _width_list = width_list; }
  void set_parallel_length_list(const std::vector<irt_int>& parallel_length_list) { _parallel_length_list = parallel_length_list; }
  void set_width_parallel_length_map(const GridMap<irt_int>& width_parallel_length_map)
  {
    _width_parallel_length_map = width_parallel_length_map;
  }
  // function

 private:
  std::vector<irt_int> _width_list;
  std::vector<irt_int> _parallel_length_list;
  GridMap<irt_int> _width_parallel_length_map;
};

}  // namespace irt
