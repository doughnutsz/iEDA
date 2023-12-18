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

#include "Guide.hpp"
#include "GuideSeg.hpp"
#include "MTree.hpp"
#include "Net.hpp"
#include "TAPin.hpp"

namespace irt {

class TANet
{
 public:
  TANet() = default;
  ~TANet() = default;
  // getter
  Net* get_origin_net() { return _origin_net; }
  irt_int get_net_idx() const { return _net_idx; }
  ConnectType get_connect_type() const { return _connect_type; }
  std::vector<TAPin>& get_ta_pin_list() { return _ta_pin_list; }
  MTree<Guide>& get_gr_result_tree() { return _gr_result_tree; }
  MTree<GuideSeg>& get_guide_seg_tree() { return _guide_seg_tree; }
  std::vector<Segment<LayerCoord>>& get_ta_result_list() { return _ta_result_list; }
  // setter
  void set_origin_net(Net* origin_net) { _origin_net = origin_net; }
  void set_net_idx(const irt_int net_idx) { _net_idx = net_idx; }
  void set_connect_type(const ConnectType& connect_type) { _connect_type = connect_type; }
  void set_ta_pin_list(std::vector<TAPin>& ta_pin_list) { _ta_pin_list = ta_pin_list; }
  void set_gr_result_tree(const MTree<Guide>& gr_result_tree) { _gr_result_tree = gr_result_tree; }
  void set_guide_seg_tree(const MTree<GuideSeg>& guide_seg_tree) { _guide_seg_tree = guide_seg_tree; }
  void set_ta_result_list(std::vector<Segment<LayerCoord>>& ta_result_list) { _ta_result_list = ta_result_list; }

 private:
  Net* _origin_net = nullptr;
  irt_int _net_idx = -1;
  ConnectType _connect_type = ConnectType::kNone;
  std::vector<TAPin> _ta_pin_list;
  MTree<Guide> _gr_result_tree;
  MTree<GuideSeg> _guide_seg_tree;
  std::vector<Segment<LayerCoord>> _ta_result_list;
};

}  // namespace irt
