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
#include "DetailedRouter.hpp"

#include "DRBox.hpp"
#include "DRBoxId.hpp"
#include "DRNet.hpp"
#include "DRNode.hpp"
#include "DetailedRouter.hpp"
#include "GDSPlotter.hpp"
#include "RTAPI.hpp"

namespace irt {

// public

void DetailedRouter::initInst()
{
  if (_dr_instance == nullptr) {
    _dr_instance = new DetailedRouter();
  }
}

DetailedRouter& DetailedRouter::getInst()
{
  if (_dr_instance == nullptr) {
    LOG_INST.error(Loc::current(), "The instance not initialized!");
  }
  return *_dr_instance;
}

void DetailedRouter::destroyInst()
{
  if (_dr_instance != nullptr) {
    delete _dr_instance;
    _dr_instance = nullptr;
  }
}

void DetailedRouter::route(std::vector<Net>& net_list)
{
  Monitor monitor;

  routeNetList(net_list);

  LOG_INST.info(Loc::current(), "The ", GetStageName()(Stage::kDetailedRouter), " completed!", monitor.getStatsInfo());
}

// private

DetailedRouter* DetailedRouter::_dr_instance = nullptr;

void DetailedRouter::routeNetList(std::vector<Net>& net_list)
{
  DRModel dr_model = init(net_list);
  iterative(dr_model);
  update(dr_model);
}

#if 1  // init

DRModel DetailedRouter::init(std::vector<Net>& net_list)
{
  DRModel dr_model = initDRModel(net_list);
  buildDRModel(dr_model);
  return dr_model;
}

DRModel DetailedRouter::initDRModel(std::vector<Net>& net_list)
{
  ScaleAxis& gcell_axis = DM_INST.getDatabase().get_gcell_axis();
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  DRModel dr_model;

  irt_int x_gcell_num = 0;
  for (ScaleGrid& x_grid : gcell_axis.get_x_grid_list()) {
    x_gcell_num += x_grid.get_step_num();
  }
  irt_int y_gcell_num = 0;
  for (ScaleGrid& y_grid : gcell_axis.get_y_grid_list()) {
    y_gcell_num += y_grid.get_step_num();
  }
  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();
  dr_box_map.init(x_gcell_num, y_gcell_num);
  for (irt_int x = 0; x < dr_box_map.get_x_size(); x++) {
    for (irt_int y = 0; y < dr_box_map.get_y_size(); y++) {
      DRBox& dr_box = dr_box_map[x][y];
      dr_box.set_base_region(RTUtil::getRealRect(PlanarCoord(x, y), gcell_axis));
      dr_box.set_top_layer_idx(routing_layer_list.back().get_layer_idx());
      dr_box.set_bottom_layer_idx(routing_layer_list.front().get_layer_idx());

      DRBoxId dr_box_id;
      dr_box_id.set_x(x);
      dr_box_id.set_y(y);
      dr_box.set_dr_box_id(dr_box_id);
    }
  }
  dr_model.set_dr_net_list(convertToDRNetList(net_list));

  return dr_model;
}

std::vector<DRNet> DetailedRouter::convertToDRNetList(std::vector<Net>& net_list)
{
  std::vector<DRNet> dr_net_list;
  dr_net_list.reserve(net_list.size());
  for (Net& net : net_list) {
    dr_net_list.emplace_back(convertToDRNet(net));
  }
  return dr_net_list;
}

DRNet DetailedRouter::convertToDRNet(Net& net)
{
  DRNet dr_net;
  dr_net.set_origin_net(&net);
  dr_net.set_net_idx(net.get_net_idx());
  for (Pin& pin : net.get_pin_list()) {
    dr_net.get_dr_pin_list().push_back(DRPin(pin));
  }
  dr_net.set_ta_result_tree(net.get_ta_result_tree());
  dr_net.set_dr_result_tree(net.get_ta_result_tree());
  return dr_net;
}

void DetailedRouter::buildDRModel(DRModel& dr_model)
{
  buildSchedule(dr_model);
  updateNetRectMap(dr_model);
  cutBlockageList(dr_model);
  updateNetPanelResultMap(dr_model);
  buildBoxScaleAxis(dr_model);
  buildDRTaskList(dr_model);
  buildDRBoxMap(dr_model);
}

void DetailedRouter::buildSchedule(DRModel& dr_model)
{
  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();

  irt_int box_size = dr_box_map.get_x_size() * dr_box_map.get_y_size();
  irt_int range = std::max(2, static_cast<irt_int>(std::sqrt(box_size / RTUtil::getBatchSize(box_size))));

  std::vector<std::vector<DRBoxId>> dr_box_id_comb_list;
  for (irt_int start_x = 0; start_x < range; start_x++) {
    for (irt_int start_y = 0; start_y < range; start_y++) {
      std::vector<DRBoxId> dr_box_id_list;
      for (irt_int x = start_x; x < dr_box_map.get_x_size(); x += range) {
        for (irt_int y = start_y; y < dr_box_map.get_y_size(); y += range) {
          dr_box_id_list.emplace_back(x, y);
        }
      }
      dr_box_id_comb_list.push_back(dr_box_id_list);
    }
  }
  dr_model.set_dr_box_id_comb_list(dr_box_id_comb_list);
}

void DetailedRouter::updateNetRectMap(DRModel& dr_model)
{
  std::vector<Blockage>& routing_blockage_list = DM_INST.getDatabase().get_routing_blockage_list();
  std::vector<Blockage>& cut_blockage_list = DM_INST.getDatabase().get_cut_blockage_list();

  for (Blockage& routing_blockage : routing_blockage_list) {
    LayerRect blockage_real_rect(routing_blockage.get_real_rect(), routing_blockage.get_layer_idx());
    addRectToEnv(dr_model, DRSourceType::kBlockAndPin, DRBoxId(), -1, blockage_real_rect, true);
  }
  for (Blockage& cut_blockage : cut_blockage_list) {
    LayerRect blockage_real_rect(cut_blockage.get_real_rect(), cut_blockage.get_layer_idx());
    addRectToEnv(dr_model, DRSourceType::kBlockAndPin, DRBoxId(), -1, blockage_real_rect, false);
  }
  for (DRNet& dr_net : dr_model.get_dr_net_list()) {
    for (DRPin& dr_pin : dr_net.get_dr_pin_list()) {
      for (EXTLayerRect& routing_shape : dr_pin.get_routing_shape_list()) {
        LayerRect shape_real_rect(routing_shape.get_real_rect(), routing_shape.get_layer_idx());
        addRectToEnv(dr_model, DRSourceType::kBlockAndPin, DRBoxId(), dr_net.get_net_idx(), shape_real_rect, true);
      }
      for (EXTLayerRect& cut_shape : dr_pin.get_cut_shape_list()) {
        LayerRect shape_real_rect(cut_shape.get_real_rect(), cut_shape.get_layer_idx());
        addRectToEnv(dr_model, DRSourceType::kBlockAndPin, DRBoxId(), dr_net.get_net_idx(), shape_real_rect, false);
      }
    }
  }
}

void DetailedRouter::addRectToEnv(DRModel& dr_model, DRSourceType dr_source_type, DRBoxId dr_box_id, irt_int net_idx, LayerRect real_rect,
                                  bool is_routing)
{
  ScaleAxis& gcell_axis = DM_INST.getDatabase().get_gcell_axis();
  EXTPlanarRect& die = DM_INST.getDatabase().get_die();

  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();

  ids::DRCRect ids_drc_rect = RTAPI_INST.convertToIDSRect(net_idx, real_rect, is_routing);
  for (const LayerRect& max_scope_real_rect : RTAPI_INST.getMaxScope(ids_drc_rect)) {
    LayerRect max_scope_regular_rect = RTUtil::getRegularRect(max_scope_real_rect, die.get_real_rect());
    PlanarRect max_scope_grid_rect = RTUtil::getClosedGridRect(max_scope_regular_rect, gcell_axis);
    for (irt_int x = max_scope_grid_rect.get_lb_x(); x <= max_scope_grid_rect.get_rt_x(); x++) {
      for (irt_int y = max_scope_grid_rect.get_lb_y(); y <= max_scope_grid_rect.get_rt_y(); y++) {
        DRBox& dr_box = dr_box_map[x][y];
        if (dr_source_type == DRSourceType::kBlockAndPin || dr_source_type == DRSourceType::kPanelResult) {
          dr_box_id = dr_box.get_dr_box_id();
        }
        if (is_routing) {
          dr_box.get_source_box_routing_net_rect_map()[dr_source_type][dr_box_id][real_rect.get_layer_idx()][net_idx].push_back(real_rect);
        } else {
          dr_box.get_source_box_cut_net_rect_map()[dr_source_type][dr_box_id][real_rect.get_layer_idx()][net_idx].push_back(real_rect);
        }
        void*& region_query = dr_box.get_source_box_region_query_map()[dr_source_type][dr_box_id];
        if (region_query == nullptr) {
          region_query = RTAPI_INST.initRegionQuery();
        }
        RTAPI_INST.addEnvRectList(region_query, ids_drc_rect);
      }
    }
  }
}

void DetailedRouter::cutBlockageList(DRModel& dr_model)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();

  for (irt_int x = 0; x < dr_box_map.get_x_size(); x++) {
    for (irt_int y = 0; y < dr_box_map.get_y_size(); y++) {
      DRBox& dr_box = dr_box_map[x][y];

      for (auto& [routing_layer_idx, net_rect_map] :
           dr_box.get_source_box_routing_net_rect_map()[DRSourceType::kBlockAndPin][dr_box.get_dr_box_id()]) {
        RoutingLayer& routing_layer = routing_layer_list[routing_layer_idx];

        std::vector<LayerRect> new_blockage_list;
        new_blockage_list.reserve(net_rect_map[-1].size());
        std::map<LayerRect, std::vector<PlanarRect>, CmpLayerRectByXASC> blockage_shape_list_map;

        for (LayerRect& blockage : net_rect_map[-1]) {
          bool is_cutting = false;
          for (auto& [net_idx, net_shape_list] : net_rect_map) {
            if (net_idx == -1) {
              continue;
            }
            for (LayerRect& net_shape : net_shape_list) {
              if (!RTUtil::isInside(blockage, net_shape)) {
                continue;
              }
              for (LayerRect& min_scope_net_shape : RTAPI_INST.getMinScope(RTAPI_INST.convertToIDSRect(net_idx, net_shape, true))) {
                PlanarRect enlarge_net_shape = RTUtil::getEnlargedRect(min_scope_net_shape, routing_layer.get_min_width());
                blockage_shape_list_map[blockage].push_back(enlarge_net_shape);
              }
              is_cutting = true;
            }
          }
          if (!is_cutting) {
            new_blockage_list.push_back(blockage);
          }
        }
        for (auto& [blockage, enlarge_net_shape_list] : blockage_shape_list_map) {
          for (PlanarRect& cutting_rect : RTUtil::getCuttingRectList(blockage, enlarge_net_shape_list)) {
            new_blockage_list.emplace_back(cutting_rect, blockage.get_layer_idx());
          }
        }
        net_rect_map[-1] = new_blockage_list;
      }
    }
  }
}

void DetailedRouter::updateNetPanelResultMap(DRModel& dr_model)
{
  for (DRNet& dr_net : dr_model.get_dr_net_list()) {
    for (TNode<RTNode>* ta_node_node : RTUtil::getNodeList(dr_net.get_ta_result_tree())) {
      if (ta_node_node->value().isDRNode()) {
        continue;
      }
      std::vector<Segment<LayerCoord>> segment_list;
      for (Segment<TNode<LayerCoord>*>& routing_segment : RTUtil::getSegListByTree(ta_node_node->value().get_routing_tree())) {
        segment_list.emplace_back(routing_segment.get_first()->value(), routing_segment.get_second()->value());
      }
      addRectToEnv(dr_model, DRSourceType::kPanelResult, DRBoxId(), dr_net.get_net_idx(), segment_list);
    }
  }
}

void DetailedRouter::addRectToEnv(DRModel& dr_model, DRSourceType dr_source_type, DRBoxId dr_box_id, irt_int net_idx,
                                  std::vector<Segment<LayerCoord>>& segment_list)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = DM_INST.getDatabase().get_layer_via_master_list();

  std::vector<ids::DRCRect> ids_drc_rect_list;
  for (Segment<LayerCoord>& segment : segment_list) {
    LayerCoord& first_coord = segment.get_first();
    LayerCoord& second_coord = segment.get_second();

    irt_int first_layer_idx = first_coord.get_layer_idx();
    irt_int second_layer_idx = second_coord.get_layer_idx();
    if (first_layer_idx != second_layer_idx) {
      RTUtil::sortASC(first_layer_idx, second_layer_idx);
      for (irt_int layer_idx = first_layer_idx; layer_idx < second_layer_idx; layer_idx++) {
        ViaMaster& via_master = layer_via_master_list[layer_idx].front();

        LayerRect& above_enclosure = via_master.get_above_enclosure();
        LayerRect offset_above_enclosure(RTUtil::getOffsetRect(above_enclosure, first_coord), above_enclosure.get_layer_idx());
        addRectToEnv(dr_model, dr_source_type, dr_box_id, net_idx, offset_above_enclosure, true);

        LayerRect& below_enclosure = via_master.get_below_enclosure();
        LayerRect offset_below_enclosure(RTUtil::getOffsetRect(below_enclosure, first_coord), below_enclosure.get_layer_idx());
        addRectToEnv(dr_model, dr_source_type, dr_box_id, net_idx, offset_below_enclosure, true);

        for (PlanarRect& cut_shape : via_master.get_cut_shape_list()) {
          LayerRect offset_cut_shape(RTUtil::getOffsetRect(cut_shape, first_coord), via_master.get_cut_layer_idx());
          addRectToEnv(dr_model, dr_source_type, dr_box_id, net_idx, offset_cut_shape, false);
        }
      }
    } else {
      irt_int half_width = routing_layer_list[first_layer_idx].get_min_width() / 2;
      LayerRect wire_rect(RTUtil::getEnlargedRect(first_coord, second_coord, half_width), first_layer_idx);
      addRectToEnv(dr_model, dr_source_type, dr_box_id, net_idx, wire_rect, true);
    }
  }
}

void DetailedRouter::buildBoxScaleAxis(DRModel& dr_model)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  std::map<PlanarCoord, std::vector<PlanarCoord>, CmpPlanarCoordByXASC> grid_ap_coord_map;
  for (DRNet& dr_net : dr_model.get_dr_net_list()) {
    for (DRPin& dr_pin : dr_net.get_dr_pin_list()) {
      for (AccessPoint& access_point : dr_pin.get_access_point_list()) {
        grid_ap_coord_map[access_point.get_grid_coord()].push_back(access_point.get_real_coord());
      }
    }
  }

  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();
  for (irt_int x = 0; x < dr_box_map.get_x_size(); x++) {
    for (irt_int y = 0; y < dr_box_map.get_y_size(); y++) {
      PlanarRect& box_region = dr_box_map[x][y].get_base_region();
      std::vector<irt_int> x_scale_list;
      std::vector<irt_int> y_scale_list;
      for (RoutingLayer& routing_layer : routing_layer_list) {
        std::vector<irt_int> x_list
            = RTUtil::getClosedScaleList(box_region.get_lb_x(), box_region.get_rt_x(), routing_layer.getXTrackGridList());
        x_scale_list.insert(x_scale_list.end(), x_list.begin(), x_list.end());
        std::vector<irt_int> y_list
            = RTUtil::getClosedScaleList(box_region.get_lb_y(), box_region.get_rt_y(), routing_layer.getYTrackGridList());
        y_scale_list.insert(y_scale_list.end(), y_list.begin(), y_list.end());
      }
      for (PlanarCoord& ap_coord : grid_ap_coord_map[PlanarCoord(x, y)]) {
        x_scale_list.push_back(ap_coord.get_x());
        y_scale_list.push_back(ap_coord.get_y());
      }
      ScaleAxis& box_scale_axis = dr_box_map[x][y].get_box_scale_axis();
      std::sort(x_scale_list.begin(), x_scale_list.end());
      x_scale_list.erase(std::unique(x_scale_list.begin(), x_scale_list.end()), x_scale_list.end());
      box_scale_axis.set_x_grid_list(RTUtil::makeScaleGridList(x_scale_list));

      std::sort(y_scale_list.begin(), y_scale_list.end());
      y_scale_list.erase(std::unique(y_scale_list.begin(), y_scale_list.end()), y_scale_list.end());
      box_scale_axis.set_y_grid_list(RTUtil::makeScaleGridList(y_scale_list));
    }
  }
}

void DetailedRouter::buildDRTaskList(DRModel& dr_model)
{
  Monitor monitor;

  std::vector<DRNet>& dr_net_list = dr_model.get_dr_net_list();

  irt_int batch_size = RTUtil::getBatchSize(dr_net_list.size());

  Monitor stage_monitor;
  for (size_t i = 0; i < dr_net_list.size(); i++) {
    buildDRTask(dr_model, dr_net_list[i]);
    if ((i + 1) % batch_size == 0) {
      LOG_INST.info(Loc::current(), "Extracting task from ", (i + 1), " nets", stage_monitor.getStatsInfo());
    }
  }
  LOG_INST.info(Loc::current(), "Extracting task from ", dr_net_list.size(), " nets", monitor.getStatsInfo());
}

void DetailedRouter::buildDRTask(DRModel& dr_model, DRNet& dr_net)
{
  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();

  for (auto& [dr_node_node, dr_task] : makeDRNodeTaskMap(dr_model, dr_net)) {
    PlanarCoord& grid_coord = dr_node_node->value().get_first_guide().get_grid_coord();
    DRBox& dr_box = dr_box_map[grid_coord.get_x()][grid_coord.get_y()];

    std::vector<DRTask>& dr_task_list = dr_box.get_dr_task_list();
    dr_task.set_origin_net_idx(dr_net.get_net_idx());
    dr_task.set_origin_node(dr_node_node);
    dr_task.set_task_idx(static_cast<irt_int>(dr_task_list.size()));
    buildBoundingBox(dr_box, dr_task);
    dr_task_list.push_back(dr_task);
  }
}

std::map<TNode<RTNode>*, DRTask> DetailedRouter::makeDRNodeTaskMap(DRModel& dr_model, DRNet& dr_net)
{
  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();
  MTree<RTNode>& dr_result_tree = dr_net.get_dr_result_tree();
  // dr_ta_list_map
  std::map<TNode<RTNode>*, std::vector<TNode<RTNode>*>> dr_ta_list_map;
  std::vector<Segment<TNode<RTNode>*>> segment_list = RTUtil::getSegListByTree(dr_result_tree);
  if (dr_result_tree.get_root() != nullptr && segment_list.empty()) {
    // local net
    dr_ta_list_map[dr_result_tree.get_root()] = {};
  }
  for (Segment<TNode<RTNode>*>& segment : segment_list) {
    TNode<RTNode>* dr_node_node = segment.get_first();
    TNode<RTNode>* ta_node_node = segment.get_second();
    if (dr_node_node->value().isTANode()) {
      std::swap(dr_node_node, ta_node_node);
    }
    dr_ta_list_map[dr_node_node].push_back(ta_node_node);
  }
  // dr_node_task_map
  std::map<TNode<RTNode>*, DRTask> dr_node_task_map;
  for (auto& [dr_node_node, ta_node_node_list] : dr_ta_list_map) {
    PlanarCoord& grid_coord = dr_node_node->value().get_first_guide().get_grid_coord();
    DRBox& dr_box = dr_box_map[grid_coord.get_x()][grid_coord.get_y()];

    std::vector<DRGroup>& dr_group_list = dr_node_task_map[dr_node_node].get_dr_group_list();
    for (irt_int pin_idx : dr_node_node->value().get_pin_idx_set()) {
      dr_group_list.push_back(makeDRGroup(dr_box, dr_net.get_dr_pin_list()[pin_idx]));
    }
    for (TNode<RTNode>* ta_node_node : ta_node_node_list) {
      dr_group_list.push_back(makeDRGroup(dr_box, ta_node_node));
    }
  }
  return dr_node_task_map;
}

DRGroup DetailedRouter::makeDRGroup(DRBox& dr_box, DRPin& dr_pin)
{
  PlanarRect& dr_base_region = dr_box.get_base_region();

  DRGroup dr_group;
  for (LayerCoord& real_coord : dr_pin.getRealCoordList()) {
    if (RTUtil::isInside(dr_base_region, real_coord)) {
      dr_group.get_coord_direction_map()[real_coord].insert({});
    }
  }
  return dr_group;
}

DRGroup DetailedRouter::makeDRGroup(DRBox& dr_box, TNode<RTNode>* ta_node_node)
{
  PlanarRect& dr_base_region = dr_box.get_base_region();
  ScaleAxis& box_scale_axis = dr_box.get_box_scale_axis();

  RTNode& ta_node = ta_node_node->value();
  irt_int ta_layer_idx = ta_node.get_first_guide().get_layer_idx();

  DRGroup dr_group;
  for (Segment<TNode<LayerCoord>*>& routing_segment : RTUtil::getSegListByTree(ta_node.get_routing_tree())) {
    Segment<PlanarCoord> cutting_segment(routing_segment.get_first()->value(), routing_segment.get_second()->value());
    if (!RTUtil::isOverlap(dr_base_region, cutting_segment)) {
      continue;
    }
    cutting_segment = RTUtil::getOverlap(dr_base_region, cutting_segment);
    PlanarCoord& first_coord = cutting_segment.get_first();
    irt_int first_x = first_coord.get_x();
    irt_int first_y = first_coord.get_y();
    PlanarCoord& second_coord = cutting_segment.get_second();
    Direction direction = RTUtil::getDirection(first_coord, second_coord);
    if (direction == Direction::kHorizontal) {
      for (irt_int x : RTUtil::getClosedScaleList(first_x, second_coord.get_x(), box_scale_axis.get_x_grid_list())) {
        dr_group.get_coord_direction_map()[LayerCoord(x, first_y, ta_layer_idx)].insert(direction);
      }
    } else if (direction == Direction::kVertical) {
      for (irt_int y : RTUtil::getClosedScaleList(first_y, second_coord.get_y(), box_scale_axis.get_y_grid_list())) {
        dr_group.get_coord_direction_map()[LayerCoord(first_x, y, ta_layer_idx)].insert(direction);
      }
    } else if (RTUtil::isProximal(first_coord, second_coord)) {
      LOG_INST.error(Loc::current(), "The ta segment is proximal!");
    }
  }
  return dr_group;
}

void DetailedRouter::buildBoundingBox(DRBox& dr_box, DRTask& dr_task)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  bool is_local_task = true;
  for (DRGroup& dr_group : dr_task.get_dr_group_list()) {
    for (auto& [coord, direction_set] : dr_group.get_coord_direction_map()) {
      if (!direction_set.empty()) {
        is_local_task = false;
        goto here;
      }
    }
  }
here:
  irt_int top_layer_idx = INT_MIN;
  irt_int bottom_layer_idx = INT_MAX;
  if (is_local_task) {
    top_layer_idx = routing_layer_list.back().get_layer_idx();
    bottom_layer_idx = routing_layer_list.front().get_layer_idx();
  } else {
    for (DRGroup& dr_group : dr_task.get_dr_group_list()) {
      for (auto& [coord, direction_set] : dr_group.get_coord_direction_map()) {
        top_layer_idx = std::max(top_layer_idx, coord.get_layer_idx());
        bottom_layer_idx = std::min(bottom_layer_idx, coord.get_layer_idx());
      }
    }
  }
  SpaceRegion& bounding_box = dr_task.get_bounding_box();
  bounding_box.set_base_region(dr_box.get_base_region());
  bounding_box.set_top_layer_idx(top_layer_idx);
  bounding_box.set_bottom_layer_idx(bottom_layer_idx);
}

void DetailedRouter::buildDRBoxMap(DRModel& dr_model)
{
  Monitor monitor;

  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();

  size_t total_box_num = 0;
  for (std::vector<DRBoxId>& dr_box_id_list : dr_model.get_dr_box_id_comb_list()) {
    Monitor stage_monitor;
#pragma omp parallel for
    for (DRBoxId& dr_box_id : dr_box_id_list) {
      DRBox& dr_box = dr_box_map[dr_box_id.get_x()][dr_box_id.get_y()];
      initLayerNodeMap(dr_box);
      buildNeighborMap(dr_box);
      checkDRBox(dr_box);
      saveDRBox(dr_box);
    }
    total_box_num += dr_box_id_list.size();
    LOG_INST.info(Loc::current(), "Built ", dr_box_id_list.size(), " boxes", stage_monitor.getStatsInfo());
  }
  LOG_INST.info(Loc::current(), "Built ", total_box_num, " boxes", monitor.getStatsInfo());
}

void DetailedRouter::initLayerNodeMap(DRBox& dr_box)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  PlanarCoord& real_lb = dr_box.get_base_region().get_lb();
  PlanarCoord& real_rt = dr_box.get_base_region().get_rt();
  ScaleAxis& box_scale_axis = dr_box.get_box_scale_axis();
  std::vector<irt_int> x_list = RTUtil::getClosedScaleList(real_lb.get_x(), real_rt.get_x(), box_scale_axis.get_x_grid_list());
  std::vector<irt_int> y_list = RTUtil::getClosedScaleList(real_lb.get_y(), real_rt.get_y(), box_scale_axis.get_y_grid_list());

  std::vector<GridMap<DRNode>>& layer_node_map = dr_box.get_layer_node_map();
  layer_node_map.resize(routing_layer_list.size());
  for (irt_int layer_idx = 0; layer_idx < static_cast<irt_int>(layer_node_map.size()); layer_idx++) {
    GridMap<DRNode>& dr_node_map = layer_node_map[layer_idx];
    dr_node_map.init(x_list.size(), y_list.size());
    for (size_t x = 0; x < x_list.size(); x++) {
      for (size_t y = 0; y < y_list.size(); y++) {
        DRNode& dr_node = dr_node_map[x][y];
        dr_node.set_x(x_list[x]);
        dr_node.set_y(y_list[y]);
        dr_node.set_layer_idx(layer_idx);
      }
    }
  }
}

void DetailedRouter::buildNeighborMap(DRBox& dr_box)
{
  irt_int bottom_routing_layer_idx = DM_INST.getConfig().bottom_routing_layer_idx;
  irt_int top_routing_layer_idx = DM_INST.getConfig().top_routing_layer_idx;

  std::vector<GridMap<DRNode>>& layer_node_map = dr_box.get_layer_node_map();
  for (irt_int layer_idx = 0; layer_idx < static_cast<irt_int>(layer_node_map.size()); layer_idx++) {
    bool routing_hv = true;
    if (layer_idx < bottom_routing_layer_idx || top_routing_layer_idx < layer_idx) {
      routing_hv = false;
    }
    GridMap<DRNode>& dr_node_map = layer_node_map[layer_idx];
    for (irt_int x = 0; x < dr_node_map.get_x_size(); x++) {
      for (irt_int y = 0; y < dr_node_map.get_y_size(); y++) {
        std::map<Orientation, DRNode*>& neighbor_ptr_map = dr_node_map[x][y].get_neighbor_ptr_map();
        if (routing_hv) {
          if (x != 0) {
            neighbor_ptr_map[Orientation::kWest] = &dr_node_map[x - 1][y];
          }
          if (x != (dr_node_map.get_x_size() - 1)) {
            neighbor_ptr_map[Orientation::kEast] = &dr_node_map[x + 1][y];
          }
          if (y != 0) {
            neighbor_ptr_map[Orientation::kSouth] = &dr_node_map[x][y - 1];
          }
          if (y != (dr_node_map.get_y_size() - 1)) {
            neighbor_ptr_map[Orientation::kNorth] = &dr_node_map[x][y + 1];
          }
        }
        if (layer_idx != 0) {
          neighbor_ptr_map[Orientation::kDown] = &layer_node_map[layer_idx - 1][x][y];
        }
        if (layer_idx != static_cast<irt_int>(layer_node_map.size()) - 1) {
          neighbor_ptr_map[Orientation::kUp] = &layer_node_map[layer_idx + 1][x][y];
        }
      }
    }
  }
}

void DetailedRouter::checkDRBox(DRBox& dr_box)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  DRBoxId& dr_box_id = dr_box.get_dr_box_id();
  if (dr_box_id.get_x() < 0 || dr_box_id.get_y() < 0) {
    LOG_INST.error(Loc::current(), "The grid coord is illegal!");
  }

  std::vector<DRTask>& dr_task_list = dr_box.get_dr_task_list();
  for (DRTask& dr_task : dr_task_list) {
    if (dr_task.get_origin_node() == nullptr) {
      LOG_INST.error(Loc::current(), "The origin node of dr task is nullptr!");
    }
    if (dr_task.get_task_idx() < 0) {
      LOG_INST.error(Loc::current(), "The idx of dr task is illegal!");
    }
    if (dr_task.get_origin_net_idx() < 0) {
      LOG_INST.error(Loc::current(), "The idx of origin net is illegal!");
    }
    for (DRGroup& dr_group : dr_task.get_dr_group_list()) {
      if (dr_group.get_coord_direction_map().empty()) {
        LOG_INST.error(Loc::current(), "The coord_direction_map is empty!");
      }
      for (auto& [coord, direction_set] : dr_group.get_coord_direction_map()) {
        irt_int layer_idx = coord.get_layer_idx();
        if (routing_layer_list.back().get_layer_idx() < layer_idx || layer_idx < routing_layer_list.front().get_layer_idx()) {
          LOG_INST.error(Loc::current(), "The layer idx of group coord is illegal!");
        }
        if (RTUtil::isInside(dr_box.get_base_region(), coord)) {
          continue;
        }
        LOG_INST.error(Loc::current(), "The coord (", coord.get_x(), ",", coord.get_y(), ") is out of box!");
      }
    }
  }
  PlanarCoord& real_lb = dr_box.get_base_region().get_lb();
  PlanarCoord& real_rt = dr_box.get_base_region().get_rt();
  ScaleAxis& box_scale_axis = dr_box.get_box_scale_axis();
  std::vector<irt_int> x_scale_list = RTUtil::getClosedScaleList(real_lb.get_x(), real_rt.get_x(), box_scale_axis.get_x_grid_list());
  std::vector<irt_int> y_scale_list = RTUtil::getClosedScaleList(real_lb.get_y(), real_rt.get_y(), box_scale_axis.get_y_grid_list());

  for (GridMap<DRNode>& dr_node_map : dr_box.get_layer_node_map()) {
    for (irt_int x_idx = 0; x_idx < dr_node_map.get_x_size(); x_idx++) {
      for (irt_int y_idx = 0; y_idx < dr_node_map.get_y_size(); y_idx++) {
        DRNode& dr_node = dr_node_map[x_idx][y_idx];
        if (!RTUtil::isInside(dr_box.get_base_region(), dr_node.get_planar_coord())) {
          LOG_INST.error(Loc::current(), "The dr node is out of box!");
        }
        for (auto& [orien, neighbor] : dr_node.get_neighbor_ptr_map()) {
          Orientation opposite_orien = RTUtil::getOppositeOrientation(orien);
          if (!RTUtil::exist(neighbor->get_neighbor_ptr_map(), opposite_orien)) {
            LOG_INST.error(Loc::current(), "The dr_node neighbor is not bidirection!");
          }
          if (neighbor->get_neighbor_ptr_map()[opposite_orien] != &dr_node) {
            LOG_INST.error(Loc::current(), "The dr_node neighbor is not bidirection!");
          }
          LayerCoord node_coord(dr_node.get_planar_coord(), dr_node.get_layer_idx());
          LayerCoord neighbor_coord(neighbor->get_planar_coord(), neighbor->get_layer_idx());
          if (RTUtil::getOrientation(node_coord, neighbor_coord) == orien) {
            continue;
          }
          LOG_INST.error(Loc::current(), "The neighbor orien is different with real region!");
        }
        irt_int node_x = dr_node.get_planar_coord().get_x();
        irt_int node_y = dr_node.get_planar_coord().get_y();
        for (auto& [orien, neighbor] : dr_node.get_neighbor_ptr_map()) {
          if (orien == Orientation::kUp || orien == Orientation::kDown) {
            continue;
          }
          PlanarCoord neighbor_coord(node_x, node_y);
          switch (orien) {
            case Orientation::kEast:
              if (x_scale_list[x_idx] != node_x || (x_idx + 1) >= static_cast<irt_int>(x_scale_list.size())) {
                LOG_INST.error(Loc::current(), "The adjacent scale does not exist!");
              }
              neighbor_coord.set_x(x_scale_list[x_idx + 1]);
              break;
            case Orientation::kWest:
              if (x_scale_list[x_idx] != node_x || (x_idx - 1) < 0) {
                LOG_INST.error(Loc::current(), "The adjacent scale does not exist!");
              }
              neighbor_coord.set_x(x_scale_list[x_idx - 1]);
              break;
            case Orientation::kNorth:
              if (y_scale_list[y_idx] != node_y || (y_idx + 1) >= static_cast<irt_int>(y_scale_list.size())) {
                LOG_INST.error(Loc::current(), "The adjacent scale does not exist!");
              }
              neighbor_coord.set_y(y_scale_list[y_idx + 1]);
              break;
            case Orientation::kSouth:
              if (y_scale_list[y_idx] != node_y || (y_idx - 1) < 0) {
                LOG_INST.error(Loc::current(), "The adjacent scale does not exist!");
              }
              neighbor_coord.set_y(y_scale_list[y_idx - 1]);
              break;
            default:
              break;
          }
          if (neighbor_coord == neighbor->get_planar_coord()) {
            continue;
          }
          LOG_INST.error(Loc::current(), "The neighbor coord is different with real coord!");
        }
      }
    }
  }
}

void DetailedRouter::saveDRBox(DRBox& dr_box)
{
}

#endif

#if 1  // iterative

void DetailedRouter::iterative(DRModel& dr_model)
{
  irt_int dr_outer_max_iter_num = DM_INST.getConfig().dr_outer_max_iter_num;

  for (irt_int iter = 1; iter <= dr_outer_max_iter_num; iter++) {
    Monitor iter_monitor;
    LOG_INST.info(Loc::current(), "****** Start Model Iteration(", iter, "/", dr_outer_max_iter_num, ") ******");

    dr_model.set_curr_iter(iter);
    resetDRModel(dr_model);
    routeDRModel(dr_model);
    reportDRModel(dr_model);

    LOG_INST.info(Loc::current(), "****** End Model Iteration(", iter, "/", dr_outer_max_iter_num, ")", iter_monitor.getStatsInfo(),
                  " ******");
  }
}

void DetailedRouter::resetDRModel(DRModel& dr_model)
{
}

void DetailedRouter::routeDRModel(DRModel& dr_model)
{
  Monitor monitor;

  size_t total_box_num = 0;
  for (std::vector<DRBoxId>& dr_box_id_list : dr_model.get_dr_box_id_comb_list()) {
    Monitor stage_monitor;
#pragma omp parallel for
    for (DRBoxId& dr_box_id : dr_box_id_list) {
      iterativeDRBox(dr_model, dr_box_id);
    }
    total_box_num += dr_box_id_list.size();
    LOG_INST.info(Loc::current(), "Routed ", dr_box_id_list.size(), " boxes", stage_monitor.getStatsInfo());
  }
  LOG_INST.info(Loc::current(), "Routed ", total_box_num, " boxes", monitor.getStatsInfo());
}

void DetailedRouter::iterativeDRBox(DRModel& dr_model, DRBoxId& dr_box_id)
{
  irt_int ta_inner_max_iter_num = DM_INST.getConfig().ta_inner_max_iter_num;

  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();
  DRBox& dr_box = dr_box_map[dr_box_id.get_x()][dr_box_id.get_y()];

  for (irt_int iter = 1; iter <= ta_inner_max_iter_num; iter++) {
    Monitor iter_monitor;
    if (omp_get_num_threads() == 1) {
      LOG_INST.info(Loc::current(), "****** Start Panel Iteration(", iter, "/", ta_inner_max_iter_num, ") ******");
    }
    dr_box.set_curr_iter(iter);
    sortDRBox(dr_box);
    resetDRBox(dr_box);
    routeDRBox(dr_box);
    processDRBox(dr_box);
    reportDRBox(dr_box);
    if (omp_get_num_threads() == 1) {
      LOG_INST.info(Loc::current(), "****** End Panel Iteration(", iter, "/", ta_inner_max_iter_num, ")", iter_monitor.getStatsInfo(),
                    " ******");
    }
  }
  updateDRBox(dr_model, dr_box);
}

void DetailedRouter::sortDRBox(DRBox& dr_box)
{
}

void DetailedRouter::resetDRBox(DRBox& dr_box)
{
}

void DetailedRouter::routeDRBox(DRBox& dr_box)
{
  Monitor monitor;

  std::vector<DRTask>& dr_task_list = dr_box.get_dr_task_list();

  irt_int batch_size = RTUtil::getBatchSize(dr_task_list.size());

  Monitor stage_monitor;
  for (size_t i = 0; i < dr_task_list.size(); i++) {
    routeDRTask(dr_box, dr_task_list[i]);
    if (omp_get_num_threads() == 1 && (i + 1) % batch_size == 0) {
      LOG_INST.info(Loc::current(), "Routed ", (i + 1), " tasks", stage_monitor.getStatsInfo());
    }
  }
  if (omp_get_num_threads() == 1) {
    LOG_INST.info(Loc::current(), "Routed ", dr_task_list.size(), " tasks", monitor.getStatsInfo());
  }
}

void DetailedRouter::routeDRTask(DRBox& dr_box, DRTask& dr_task)
{
  initSingleTask(dr_box, dr_task);
  while (!isConnectedAllEnd(dr_box)) {
    for (DRRouteStrategy dr_route_strategy : {DRRouteStrategy::kFullyConsider, DRRouteStrategy::kIgnoringBlockage}) {
      routeByStrategy(dr_box, dr_route_strategy);
    }
    updatePathResult(dr_box);
    updateDirectionSet(dr_box);
    resetStartAndEnd(dr_box);
    resetSinglePath(dr_box);
  }
  updateTaskResult(dr_box, dr_task);
  resetSingleTask(dr_box);
}

void DetailedRouter::initSingleTask(DRBox& dr_box, DRTask& dr_task)
{
  ScaleAxis& box_scale_axis = dr_box.get_box_scale_axis();
  std::vector<GridMap<DRNode>>& layer_node_map = dr_box.get_layer_node_map();

  // config
  dr_box.set_wire_unit(1);
  dr_box.set_corner_unit(1);
  dr_box.set_via_unit(1);
  // single task
  dr_box.set_dr_task_ref(&dr_task);
  dr_box.set_routing_region(dr_box.get_curr_bounding_box());
  {
    std::vector<std::vector<DRNode*>> node_comb_list;
    std::vector<DRGroup>& dr_group_list = dr_task.get_dr_group_list();
    for (DRGroup& dr_group : dr_group_list) {
      std::vector<DRNode*> node_comb;
      for (auto& [coord, direction_set] : dr_group.get_coord_direction_map()) {
        if (!RTUtil::existGrid(coord, box_scale_axis)) {
          LOG_INST.error(Loc::current(), "The coord can not find grid!");
        }
        PlanarCoord grid_coord = RTUtil::getGridCoord(coord, box_scale_axis);
        node_comb.push_back(&layer_node_map[coord.get_layer_idx()][grid_coord.get_x()][grid_coord.get_y()]);
      }
      node_comb_list.push_back(node_comb);
    }
    for (size_t i = 0; i < node_comb_list.size(); i++) {
      if (i == 0) {
        dr_box.get_start_node_comb_list().push_back(node_comb_list[i]);
      } else {
        dr_box.get_end_node_comb_list().push_back(node_comb_list[i]);
      }
    }
  }
  dr_box.get_path_node_list().clear();
  dr_box.get_single_task_visited_node_list().clear();
  dr_box.get_routing_segment_list().clear();
}

bool DetailedRouter::isConnectedAllEnd(DRBox& dr_box)
{
  return dr_box.get_end_node_comb_list().empty();
}

void DetailedRouter::routeByStrategy(DRBox& dr_box, DRRouteStrategy dr_route_strategy)
{
  if (dr_route_strategy == DRRouteStrategy::kFullyConsider) {
    routeSinglePath(dr_box);
  } else if (isRoutingFailed(dr_box)) {
    resetSinglePath(dr_box);
    dr_box.set_dr_route_strategy(dr_route_strategy);
    routeSinglePath(dr_box);
    dr_box.set_dr_route_strategy(DRRouteStrategy::kNone);
    if (!isRoutingFailed(dr_box)) {
      if (omp_get_num_threads() == 1) {
        LOG_INST.info(Loc::current(), "The task ", dr_box.get_curr_task_idx(), " reroute by ", GetDRRouteStrategyName()(dr_route_strategy),
                      " successfully!");
      }
    } else if (dr_route_strategy == DRRouteStrategy::kIgnoringBlockage) {
      LOG_INST.error(Loc::current(), "The task ", dr_box.get_curr_task_idx(), " reroute by ", GetDRRouteStrategyName()(dr_route_strategy),
                     " failed!");
    }
  }
}

void DetailedRouter::routeSinglePath(DRBox& dr_box)
{
  initPathHead(dr_box);
  while (!searchEnded(dr_box)) {
    expandSearching(dr_box);
    resetPathHead(dr_box);
  }
}

void DetailedRouter::initPathHead(DRBox& dr_box)
{
  std::vector<std::vector<DRNode*>>& start_node_comb_list = dr_box.get_start_node_comb_list();
  std::vector<DRNode*>& path_node_list = dr_box.get_path_node_list();

  for (std::vector<DRNode*>& start_node_comb : start_node_comb_list) {
    for (DRNode* start_node : start_node_comb) {
      start_node->set_estimated_cost(getEstimateCostToEnd(dr_box, start_node));
      pushToOpenList(dr_box, start_node);
    }
  }
  for (DRNode* path_node : path_node_list) {
    path_node->set_estimated_cost(getEstimateCostToEnd(dr_box, path_node));
    pushToOpenList(dr_box, path_node);
  }
  resetPathHead(dr_box);
}

bool DetailedRouter::searchEnded(DRBox& dr_box)
{
  std::vector<std::vector<DRNode*>>& end_node_comb_list = dr_box.get_end_node_comb_list();
  DRNode* path_head_node = dr_box.get_path_head_node();

  if (path_head_node == nullptr) {
    dr_box.set_end_node_comb_idx(-1);
    return true;
  }
  for (size_t i = 0; i < end_node_comb_list.size(); i++) {
    for (DRNode* end_node : end_node_comb_list[i]) {
      if (path_head_node == end_node) {
        dr_box.set_end_node_comb_idx(static_cast<irt_int>(i));
        return true;
      }
    }
  }
  return false;
}

void DetailedRouter::expandSearching(DRBox& dr_box)
{
  DRNode* path_head_node = dr_box.get_path_head_node();

  for (auto& [orientation, neighbor_node] : path_head_node->get_neighbor_ptr_map()) {
    if (neighbor_node == nullptr) {
      continue;
    }
    SpaceRegion& routing_region = dr_box.get_routing_region();
    if (!RTUtil::isInside(routing_region.get_base_region(), *neighbor_node)) {
      continue;
    }
    if (neighbor_node->get_layer_idx() < routing_region.get_bottom_layer_idx()) {
      continue;
    }
    if (routing_region.get_top_layer_idx() < neighbor_node->get_layer_idx()) {
      continue;
    }
    if (neighbor_node->isClose()) {
      continue;
    }
    if (!passChecking(dr_box, path_head_node, neighbor_node)) {
      continue;
    }
    if (neighbor_node->isOpen() && replaceParentNode(dr_box, path_head_node, neighbor_node)) {
      neighbor_node->set_known_cost(getKnowCost(dr_box, path_head_node, neighbor_node));
      neighbor_node->set_parent_node(path_head_node);
    } else if (neighbor_node->isNone()) {
      neighbor_node->set_known_cost(getKnowCost(dr_box, path_head_node, neighbor_node));
      neighbor_node->set_parent_node(path_head_node);
      neighbor_node->set_estimated_cost(getEstimateCostToEnd(dr_box, neighbor_node));
      pushToOpenList(dr_box, neighbor_node);
    }
  }
}

bool DetailedRouter::passChecking(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  return true;

  // Orientation orientation = RTUtil::getOrientation(*start_node, *end_node);
  // if (orientation == Orientation::kNone) {
  //   return true;
  // }
  // Orientation opposite_orientation = RTUtil::getOppositeOrientation(orientation);

  // DRNode* pre_node = nullptr;
  // DRNode* curr_node = start_node;

  // while (curr_node != end_node) {
  //   pre_node = curr_node;
  //   curr_node = pre_node->getNeighborNode(orientation);

  //   if (curr_node == nullptr) {
  //     return false;
  //   }
  //   if (pre_node->isOBS(dr_box.get_curr_task_idx(), orientation, dr_box.get_dr_route_strategy())) {
  //     return false;
  //   }
  //   if (curr_node->isOBS(dr_box.get_curr_task_idx(), opposite_orientation, dr_box.get_dr_route_strategy())) {
  //     return false;
  //   }
  // }
  // return true;

  //   DRRouteStrategy& dr_route_strategy = dr_box.get_dr_route_strategy();

  // std::vector<Segment<LayerCoord>> routing_segment_list = getRoutingSegmentListByPathHead(dr_box);
  // routing_segment_list.emplace_back(*start_node, *end_node);

  // std::vector<LayerRect> real_rect_list = DM_INST.getRealRectList(routing_segment_list);

  // bool pass_checking = true;
  // if (dr_route_strategy == DRRouteStrategy::kIgnoringBlockage) {
  //   return pass_checking;
  // }
  // if (pass_checking) {
  //   pass_checking = !RTAPI_INST.hasViolation(dr_box.get_source_region_query_map()[DRSourceType::kBlockage], real_rect_list);
  // }
  // if (dr_route_strategy == DRRouteStrategy::kIgnoringPanelResult) {
  //   return pass_checking;
  // }
  // if (pass_checking) {
  //   pass_checking = !RTAPI_INST.hasViolation(dr_box.get_source_region_query_map()[DRSourceType::kPanelResult], real_rect_list);
  // }
  // if (dr_route_strategy == DRRouteStrategy::kIgnoringOtherBoxResult) {
  //   return pass_checking;
  // }
  // if (pass_checking) {
  //   pass_checking = !RTAPI_INST.hasViolation(dr_box.get_source_region_query_map()[DRSourceType::kOtherBoxResult], real_rect_list);
  // }
  // if (dr_route_strategy == DRRouteStrategy::kIgnoringSelfBoxResult) {
  //   return pass_checking;
  // }
  // if (pass_checking) {
  //   pass_checking = !RTAPI_INST.hasViolation(dr_box.get_source_region_query_map()[DRSourceType::kSelfBoxResult], real_rect_list);
  // }
  // return pass_checking;
}

std::vector<Segment<LayerCoord>> DetailedRouter::getRoutingSegmentListByPathHead(DRBox& dr_box)
{
  std::vector<Segment<LayerCoord>> routing_segment_list;
  DRNode* path_head_node = dr_box.get_path_head_node();

  DRNode* curr_node = path_head_node;
  DRNode* pre_node = curr_node->get_parent_node();

  if (pre_node == nullptr) {
    // 起点和终点重合
    return routing_segment_list;
  }
  Orientation curr_orientation = RTUtil::getOrientation(*curr_node, *pre_node);
  while (pre_node->get_parent_node() != nullptr) {
    Orientation pre_orientation = RTUtil::getOrientation(*pre_node, *pre_node->get_parent_node());
    if (curr_orientation != pre_orientation) {
      routing_segment_list.emplace_back(*curr_node, *pre_node);
      curr_orientation = pre_orientation;
      curr_node = pre_node;
    }
    pre_node = pre_node->get_parent_node();
  }
  routing_segment_list.emplace_back(*curr_node, *pre_node);

  return routing_segment_list;
}

bool DetailedRouter::replaceParentNode(DRBox& dr_box, DRNode* parent_node, DRNode* child_node)
{
  return getKnowCost(dr_box, parent_node, child_node) < child_node->get_known_cost();
}

void DetailedRouter::resetPathHead(DRBox& dr_box)
{
  dr_box.set_path_head_node(popFromOpenList(dr_box));
}

bool DetailedRouter::isRoutingFailed(DRBox& dr_box)
{
  return dr_box.get_end_node_comb_idx() == -1;
}

void DetailedRouter::resetSinglePath(DRBox& dr_box)
{
  dr_box.set_dr_route_strategy(DRRouteStrategy::kNone);

  std::priority_queue<DRNode*, std::vector<DRNode*>, CmpDRNodeCost> empty_queue;
  dr_box.set_open_queue(empty_queue);

  std::vector<DRNode*>& single_path_visited_node_list = dr_box.get_single_path_visited_node_list();
  for (DRNode* visited_node : single_path_visited_node_list) {
    visited_node->set_state(DRNodeState::kNone);
    visited_node->set_parent_node(nullptr);
    visited_node->set_known_cost(0);
    visited_node->set_estimated_cost(0);
  }
  single_path_visited_node_list.clear();

  dr_box.set_path_head_node(nullptr);
  dr_box.set_end_node_comb_idx(-1);
}

void DetailedRouter::updatePathResult(DRBox& dr_box)
{
  for (Segment<LayerCoord>& routing_segment : getRoutingSegmentListByPathHead(dr_box)) {
    dr_box.get_routing_segment_list().push_back(routing_segment);
  }
}

void DetailedRouter::updateDirectionSet(DRBox& dr_box)
{
  DRNode* path_head_node = dr_box.get_path_head_node();

  DRNode* curr_node = path_head_node;
  DRNode* pre_node = curr_node->get_parent_node();
  while (pre_node != nullptr) {
    curr_node->get_direction_set().insert(RTUtil::getDirection(*curr_node, *pre_node));
    pre_node->get_direction_set().insert(RTUtil::getDirection(*pre_node, *curr_node));
    curr_node = pre_node;
    pre_node = curr_node->get_parent_node();
  }
}

void DetailedRouter::resetStartAndEnd(DRBox& dr_box)
{
  std::vector<std::vector<DRNode*>>& start_node_comb_list = dr_box.get_start_node_comb_list();
  std::vector<std::vector<DRNode*>>& end_node_comb_list = dr_box.get_end_node_comb_list();
  std::vector<DRNode*>& path_node_list = dr_box.get_path_node_list();
  DRNode* path_head_node = dr_box.get_path_head_node();
  irt_int end_node_comb_idx = dr_box.get_end_node_comb_idx();

  end_node_comb_list[end_node_comb_idx].clear();
  end_node_comb_list[end_node_comb_idx].push_back(path_head_node);

  DRNode* path_node = path_head_node->get_parent_node();
  if (path_node == nullptr) {
    // 起点和终点重合
    path_node = path_head_node;
  } else {
    // 起点和终点不重合
    while (path_node->get_parent_node() != nullptr) {
      path_node_list.push_back(path_node);
      path_node = path_node->get_parent_node();
    }
  }
  if (start_node_comb_list.size() == 1) {
    // 初始化时，要把start_node_comb_list的pin只留一个ap点
    // 后续只要将end_node_comb_list的pin保留一个ap点
    start_node_comb_list.front().clear();
    start_node_comb_list.front().push_back(path_node);
  }
  start_node_comb_list.push_back(end_node_comb_list[end_node_comb_idx]);
  end_node_comb_list.erase(end_node_comb_list.begin() + end_node_comb_idx);
}

void DetailedRouter::updateTaskResult(DRBox& dr_box, DRTask& dr_task)
{
  dr_task.set_routing_segment_list(dr_box.get_routing_segment_list());
}

void DetailedRouter::resetSingleTask(DRBox& dr_box)
{
  dr_box.set_dr_task_ref(nullptr);
  dr_box.set_routing_region(SpaceRegion());
  dr_box.get_start_node_comb_list().clear();
  dr_box.get_end_node_comb_list().clear();
  dr_box.get_path_node_list().clear();

  std::vector<DRNode*>& single_task_visited_node_list = dr_box.get_single_task_visited_node_list();
  for (DRNode* single_task_visited_node : single_task_visited_node_list) {
    single_task_visited_node->get_direction_set().clear();
  }
  single_task_visited_node_list.clear();

  dr_box.get_routing_segment_list().clear();
}

// manager open list

void DetailedRouter::pushToOpenList(DRBox& dr_box, DRNode* curr_node)
{
  std::priority_queue<DRNode*, std::vector<DRNode*>, CmpDRNodeCost>& open_queue = dr_box.get_open_queue();
  std::vector<DRNode*>& single_task_visited_node_list = dr_box.get_single_task_visited_node_list();
  std::vector<DRNode*>& single_path_visited_node_list = dr_box.get_single_path_visited_node_list();

  open_queue.push(curr_node);
  curr_node->set_state(DRNodeState::kOpen);
  single_task_visited_node_list.push_back(curr_node);
  single_path_visited_node_list.push_back(curr_node);
}

DRNode* DetailedRouter::popFromOpenList(DRBox& dr_box)
{
  std::priority_queue<DRNode*, std::vector<DRNode*>, CmpDRNodeCost>& open_queue = dr_box.get_open_queue();

  DRNode* node = nullptr;
  if (!open_queue.empty()) {
    node = open_queue.top();
    open_queue.pop();
    node->set_state(DRNodeState::kClose);
  }
  return node;
}

// calculate known cost

double DetailedRouter::getKnowCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  bool exist_neighbor = false;
  for (auto& [orientation, neighbor_ptr] : start_node->get_neighbor_ptr_map()) {
    if (neighbor_ptr == end_node) {
      exist_neighbor = true;
      break;
    }
  }
  if (!exist_neighbor) {
    LOG_INST.error(Loc::current(), "The neighbor not exist!");
  }

  double cost = 0;
  cost += start_node->get_known_cost();
  cost += getJointCost(dr_box, end_node, RTUtil::getOrientation(*end_node, *start_node));
  cost += getKnowWireCost(dr_box, start_node, end_node);
  cost += getKnowCornerCost(dr_box, start_node, end_node);
  cost += getKnowViaCost(dr_box, start_node, end_node);
  return cost;
}

double DetailedRouter::getJointCost(DRBox& dr_box, DRNode* curr_node, Orientation orientation)
{
  const std::map<LayerCoord, double, CmpLayerCoordByXASC>& curr_coord_cost_map = dr_box.get_curr_coord_cost_map();

  double task_cost = 0;
  auto iter = curr_coord_cost_map.find(*curr_node);
  if (iter != curr_coord_cost_map.end()) {
    task_cost = iter->second;
  }
  return task_cost;
}

double DetailedRouter::getKnowWireCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  double wire_cost = 0;
  if (start_node->get_layer_idx() == end_node->get_layer_idx()) {
    wire_cost += RTUtil::getManhattanDistance(start_node->get_planar_coord(), end_node->get_planar_coord());

    RoutingLayer& routing_layer = routing_layer_list[start_node->get_layer_idx()];
    if (routing_layer.get_direction() != RTUtil::getDirection(*start_node, *end_node)) {
      wire_cost *= 2;
    }
  }
  wire_cost *= dr_box.get_wire_unit();
  return wire_cost;
}

double DetailedRouter::getKnowCornerCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  double corner_cost = 0;
  if (start_node->get_layer_idx() == end_node->get_layer_idx()) {
    std::set<Direction> start_direction_set = start_node->get_direction_set();
    if (start_node->get_parent_node() != nullptr) {
      start_direction_set.insert(RTUtil::getDirection(*start_node->get_parent_node(), *start_node));
    }
    std::set<Direction> end_direction_set = end_node->get_direction_set();
    end_direction_set.insert(RTUtil::getDirection(*start_node, *end_node));

    if (start_direction_set.size() == 1 && end_direction_set.size() == 1) {
      if (*start_direction_set.begin() != *end_direction_set.begin()) {
        corner_cost += dr_box.get_corner_unit();
      }
    }
  }
  return corner_cost;
}

double DetailedRouter::getKnowViaCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  return dr_box.get_via_unit() * std::abs(start_node->get_layer_idx() - end_node->get_layer_idx());
}

// calculate estimate cost

double DetailedRouter::getEstimateCostToEnd(DRBox& dr_box, DRNode* curr_node)
{
  std::vector<std::vector<DRNode*>>& end_node_comb_list = dr_box.get_end_node_comb_list();

  double estimate_cost = DBL_MAX;
  for (std::vector<DRNode*>& end_node_comb : end_node_comb_list) {
    for (DRNode* end_node : end_node_comb) {
      if (end_node->isClose()) {
        continue;
      }
      estimate_cost = std::min(estimate_cost, getEstimateCost(dr_box, curr_node, end_node));
    }
  }
  return estimate_cost;
}

double DetailedRouter::getEstimateCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  double estimate_cost = 0;
  estimate_cost += getEstimateWireCost(dr_box, start_node, end_node);
  estimate_cost += getEstimateCornerCost(dr_box, start_node, end_node);
  estimate_cost += getEstimateViaCost(dr_box, start_node, end_node);
  return estimate_cost;
}

double DetailedRouter::getEstimateWireCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  double wire_cost = 0;
  wire_cost += RTUtil::getManhattanDistance(start_node->get_planar_coord(), end_node->get_planar_coord());
  wire_cost *= dr_box.get_wire_unit();
  return wire_cost;
}

double DetailedRouter::getEstimateCornerCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  double corner_cost = 0;
  if (start_node->get_layer_idx() == end_node->get_layer_idx()) {
    if (RTUtil::isOblique(*start_node, *end_node)) {
      corner_cost += dr_box.get_corner_unit();
    }
  }
  return corner_cost;
}

double DetailedRouter::getEstimateViaCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node)
{
  return dr_box.get_via_unit() * std::abs(start_node->get_layer_idx() - end_node->get_layer_idx());
}

void DetailedRouter::processDRBox(DRBox& dr_box)
{
  for (DRTask& dr_task : dr_box.get_dr_task_list()) {
    buildRoutingResult(dr_task);
  }
}

void DetailedRouter::buildRoutingResult(DRTask& dr_task)
{
  std::vector<LayerCoord> driving_grid_coord_list;
  std::map<LayerCoord, std::set<irt_int>, CmpLayerCoordByXASC> key_coord_pin_map;
  std::vector<DRGroup>& dr_group_list = dr_task.get_dr_group_list();
  for (size_t i = 0; i < dr_group_list.size(); i++) {
    for (auto& [coord, direction_set] : dr_group_list[i].get_coord_direction_map()) {
      driving_grid_coord_list.push_back(coord);
      key_coord_pin_map[coord].insert(static_cast<irt_int>(i));
    }
  }
  std::vector<Segment<LayerCoord>>& routing_segment_list = dr_task.get_routing_segment_list();
  RTNode& rt_node = dr_task.get_origin_node()->value();
  rt_node.set_routing_tree(RTUtil::getTreeByFullFlow(driving_grid_coord_list, routing_segment_list, key_coord_pin_map));
}

void DetailedRouter::reportDRBox(DRBox& dr_box)
{
  countDRBox(dr_box);
  if (omp_get_num_threads() == 1) {
    reportTable(dr_box);
  }
}

void DetailedRouter::countDRBox(DRBox& dr_box)
{
  irt_int micron_dbu = DM_INST.getDatabase().get_micron_dbu();
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = DM_INST.getDatabase().get_layer_via_master_list();

  DRBoxStat dr_box_stat;

  std::map<irt_int, double>& routing_wire_length_map = dr_box_stat.get_routing_wire_length_map();
  std::map<irt_int, irt_int>& cut_via_number_map = dr_box_stat.get_cut_via_number_map();
  std::map<DRSourceType, std::map<std::string, irt_int>>& source_drc_number_map = dr_box_stat.get_source_drc_number_map();

  for (DRTask& dr_task : dr_box.get_dr_task_list()) {
    std::vector<Segment<LayerCoord>>& routing_segment_list = dr_task.get_routing_segment_list();
    for (Segment<LayerCoord>& routing_segment : routing_segment_list) {
      irt_int first_layer_idx = routing_segment.get_first().get_layer_idx();
      irt_int second_layer_idx = routing_segment.get_second().get_layer_idx();
      if (first_layer_idx == second_layer_idx) {
        double wire_length = RTUtil::getManhattanDistance(routing_segment.get_first(), routing_segment.get_second()) / 1.0 / micron_dbu;
        routing_wire_length_map[first_layer_idx] += wire_length;
      } else {
        RTUtil::sortASC(first_layer_idx, second_layer_idx);
        for (irt_int layer_idx = first_layer_idx; layer_idx < second_layer_idx; layer_idx++) {
          cut_via_number_map[layer_via_master_list[layer_idx].front().get_cut_layer_idx()]++;
        }
      }
    }
    // std::vector<LayerRect> real_rect_list = DM_INST.getRealRectList(routing_segment_list);
    // for (auto& [source, region_query] : dr_box.get_source_region_query_map()) {
    //   std::map<std::string, irt_int> drc_num_map;
    //   if (source == DRSourceType::kSelfBoxResult) {
    //     drc_num_map = RTAPI_INST.getViolation(region_query);
    //   } else {
    //     drc_num_map = RTAPI_INST.getViolation(region_query, real_rect_list);
    //   }
    //   for (auto& [drc, num] : drc_num_map) {
    //     source_drc_number_map[source][drc] += num;
    //   }
    // }
  }

  double total_wire_length = 0;
  irt_int total_via_number = 0;
  irt_int total_drc_number = 0;
  for (auto& [routing_layer_idx, wire_length] : routing_wire_length_map) {
    total_wire_length += wire_length;
  }
  for (auto& [cut_layer_idx, via_number] : cut_via_number_map) {
    total_via_number += via_number;
  }
  for (auto& [source, drc_number_map] : source_drc_number_map) {
    for (auto& [drc, number] : drc_number_map) {
      total_drc_number += number;
    }
  }
  dr_box_stat.set_total_wire_length(total_wire_length);
  dr_box_stat.set_total_via_number(total_via_number);
  dr_box_stat.set_total_drc_number(total_drc_number);

  dr_box.set_dr_box_stat(dr_box_stat);
}

void DetailedRouter::reportTable(DRBox& dr_box)
{
}

void DetailedRouter::updateDRBox(DRModel& dr_model, DRBox& dr_box)
{
  for (DRTask& dr_task : dr_box.get_dr_task_list()) {
    addRectToEnv(dr_model, DRSourceType::kBoxResult, dr_box.get_dr_box_id(), dr_task.get_origin_net_idx(),
                 dr_task.get_routing_segment_list());
  }
}

void DetailedRouter::reportDRModel(DRModel& dr_model)
{
  countDRModel(dr_model);
  reportTable(dr_model);
}

void DetailedRouter::countDRModel(DRModel& dr_model)
{
  DRModelStat dr_model_stat;

  std::map<irt_int, double>& routing_wire_length_map = dr_model_stat.get_routing_wire_length_map();
  std::map<irt_int, irt_int>& cut_via_number_map = dr_model_stat.get_cut_via_number_map();
  std::map<DRSourceType, std::map<std::string, irt_int>>& source_drc_number_map = dr_model_stat.get_source_drc_number_map();

  GridMap<DRBox>& dr_box_map = dr_model.get_dr_box_map();
  for (irt_int x = 0; x < dr_box_map.get_x_size(); x++) {
    for (irt_int y = 0; y < dr_box_map.get_y_size(); y++) {
      DRBoxStat& dr_box_stat = dr_box_map[x][y].get_dr_box_stat();
      for (auto& [routing_layer_idx, wire_length] : dr_box_stat.get_routing_wire_length_map()) {
        routing_wire_length_map[routing_layer_idx] += wire_length;
      }
      for (auto& [cut_layer_idx, via_number] : dr_box_stat.get_cut_via_number_map()) {
        cut_via_number_map[cut_layer_idx] += via_number;
      }
      for (auto& [source, drc_number_map] : dr_box_stat.get_source_drc_number_map()) {
        for (auto& [drc, number] : drc_number_map) {
          source_drc_number_map[source][drc] += number;
        }
      }
    }
  }

  double total_wire_length = 0;
  irt_int total_via_number = 0;
  irt_int total_drc_number = 0;
  for (auto& [routing_layer_idx, wire_length] : routing_wire_length_map) {
    total_wire_length += wire_length;
  }
  for (auto& [cut_layer_idx, via_number] : cut_via_number_map) {
    total_via_number += via_number;
  }
  for (auto& [source, drc_number_map] : source_drc_number_map) {
    for (auto& [drc, number] : drc_number_map) {
      total_drc_number += number;
    }
  }
  dr_model_stat.set_total_wire_length(total_wire_length);
  dr_model_stat.set_total_via_number(total_via_number);
  dr_model_stat.set_total_drc_number(total_drc_number);

  dr_model.set_dr_model_stat(dr_model_stat);
}

void DetailedRouter::reportTable(DRModel& dr_model)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  std::vector<CutLayer>& cut_layer_list = DM_INST.getDatabase().get_cut_layer_list();

  DRModelStat& dr_model_stat = dr_model.get_dr_model_stat();
  std::map<irt_int, double>& routing_wire_length_map = dr_model_stat.get_routing_wire_length_map();
  std::map<irt_int, irt_int>& cut_via_number_map = dr_model_stat.get_cut_via_number_map();
  std::map<DRSourceType, std::map<std::string, irt_int>>& source_drc_number_map = dr_model_stat.get_source_drc_number_map();
  double total_wire_length = dr_model_stat.get_total_wire_length();
  irt_int total_via_number = dr_model_stat.get_total_via_number();
  irt_int total_drc_number = dr_model_stat.get_total_drc_number();

  // wire table
  fort::char_table wire_table;
  wire_table.set_border_style(FT_SOLID_STYLE);
  wire_table << fort::header << "Routing Layer"
             << "Wire Length / um" << fort::endr;
  for (RoutingLayer& routing_layer : routing_layer_list) {
    double wire_length = routing_wire_length_map[routing_layer.get_layer_idx()];
    wire_table << routing_layer.get_layer_name()
               << RTUtil::getString(wire_length, "(", RTUtil::getPercentage(wire_length, total_wire_length), "%)") << fort::endr;
  }
  wire_table << fort::header << "Total" << total_wire_length << fort::endr;
  // via table
  fort::char_table via_table;
  via_table.set_border_style(FT_SOLID_STYLE);
  via_table << fort::header << "Cut Layer"
            << "Via number" << fort::endr;
  for (CutLayer& cut_layer : cut_layer_list) {
    irt_int via_number = cut_via_number_map[cut_layer.get_layer_idx()];
    via_table << cut_layer.get_layer_name() << RTUtil::getString(via_number, "(", RTUtil::getPercentage(via_number, total_via_number), "%)")
              << fort::endr;
  }
  via_table << fort::header << "Total" << total_via_number << fort::endr;

  // init item column/row map
  irt_int column = 0;
  std::map<std::string, irt_int> item_column_map;
  item_column_map["DRC\\Source"] = column++;
  // report drc info
  for (auto& [source, drc_number_map] : source_drc_number_map) {
    item_column_map[GetDRSourceTypeName()(source)] = column++;
  }
  item_column_map["Total"] = column;

  irt_int row = 0;
  std::map<std::string, irt_int> item_row_map;
  item_row_map["DRC\\Source"] = row++;
  for (auto& [drc, number] : source_drc_number_map.begin()->second) {
    item_row_map[drc] = row++;
  }
  item_row_map["Total"] = row;

  // build table
  fort::char_table drc_table;
  drc_table.set_border_style(FT_SOLID_ROUND_STYLE);
  drc_table << fort::header;

  drc_table[0][0] = "DRC\\Source";

  // column item
  for (auto& [source, drc_number_map] : source_drc_number_map) {
    std::string source_name = GetDRSourceTypeName()(source);
    drc_table[0][item_column_map[source_name]] = source_name;
  }
  drc_table[0][item_column_map["Total"]] = "Total";
  // row item
  for (auto& [drc, number] : source_drc_number_map.begin()->second) {
    drc_table[item_row_map[drc]][0] = drc;
  }
  drc_table[item_row_map["Total"]][0] = "Total";
  drc_table << fort::header;
  // element
  for (auto& [source, drc_number_map] : source_drc_number_map) {
    irt_int column = item_column_map[GetDRSourceTypeName()(source)];
    for (auto& [drc, number] : drc_number_map) {
      drc_table[item_row_map[drc]][column] = RTUtil::getString(number);
    }
  }
  for (auto& [source, drc_number_map] : source_drc_number_map) {
    irt_int total_number = 0;
    for (auto& [drc, number] : drc_number_map) {
      total_number += number;
    }
    irt_int row = item_row_map["Total"];
    irt_int column = item_column_map[GetDRSourceTypeName()(source)];
    drc_table[row][column] = RTUtil::getString(total_number);
  }

  std::map<std::string, irt_int> drc_total_number_map;
  for (auto& [source, drc_number_map] : source_drc_number_map) {
    for (auto& [drc, number] : drc_number_map) {
      drc_total_number_map[drc] += number;
    }
  }
  for (auto& [drc, total_number] : drc_total_number_map) {
    irt_int row = item_row_map[drc];
    irt_int column = item_column_map["Total"];
    drc_table[row][column] = RTUtil::getString(total_number);
  }
  drc_table[item_row_map["Total"]][item_column_map["Total"]] = RTUtil::getString(total_drc_number);

  // print
  std::vector<std::vector<std::string>> table_list;
  table_list.push_back(RTUtil::splitString(wire_table.to_string(), '\n'));
  table_list.push_back(RTUtil::splitString(via_table.to_string(), '\n'));
  table_list.push_back(RTUtil::splitString(drc_table.to_string(), '\n'));
  std::sort(table_list.begin(), table_list.end(),
            [](std::vector<std::string>& a, std::vector<std::string>& b) { return a.size() > b.size(); });
  for (size_t i = 0; i < table_list.front().size(); i++) {
    std::string table_str;
    for (std::vector<std::string>& table : table_list) {
      if (i < table.size()) {
        table_str += table[i];
        table_str += " ";
      }
    }
    LOG_INST.info(Loc::current(), table_str);
  }
}

#endif

#if 1  // update

void DetailedRouter::update(DRModel& dr_model)
{
  for (DRNet& dr_net : dr_model.get_dr_net_list()) {
    Net* origin_net = dr_net.get_origin_net();
    origin_net->set_dr_result_tree(dr_net.get_dr_result_tree());
  }
}

#endif

#if 0  // plot dr_box

void DetailedRouter::plotDRBox(DRBox& dr_box, irt_int curr_task_idx)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  std::string dr_temp_directory_path = DM_INST.getConfig().dr_temp_directory_path;

  irt_int width = INT_MAX;
  for (ScaleGrid& x_grid : dr_box.get_box_scale_axis().get_x_grid_list()) {
    width = std::min(width, x_grid.get_step_length());
  }
  for (ScaleGrid& y_grid : dr_box.get_box_scale_axis().get_y_grid_list()) {
    width = std::min(width, y_grid.get_step_length());
  }
  width = std::max(1, width / 3);

  GPGDS gp_gds;

  // base_region
  GPStruct base_region_struct("base_region");
  GPBoundary gp_boundary;
  gp_boundary.set_layer_idx(0);
  gp_boundary.set_data_type(0);
  gp_boundary.set_rect(dr_box.get_base_region());
  base_region_struct.push(gp_boundary);
  gp_gds.addStruct(base_region_struct);

  std::vector<GridMap<DRNode>>& layer_node_map = dr_box.get_layer_node_map();
  // dr_node_map
  GPStruct dr_node_map_struct("dr_node_map");
  for (GridMap<DRNode>& dr_node_map : layer_node_map) {
    for (irt_int grid_x = 0; grid_x < dr_node_map.get_x_size(); grid_x++) {
      for (irt_int grid_y = 0; grid_y < dr_node_map.get_y_size(); grid_y++) {
        DRNode& dr_node = dr_node_map[grid_x][grid_y];
        PlanarRect real_rect = RTUtil::getEnlargedRect(dr_node.get_planar_coord(), width);
        irt_int y_reduced_span = std::max(1, real_rect.getYSpan() / 12);
        irt_int y = real_rect.get_rt_y();

        GPBoundary gp_boundary;
        switch (dr_node.get_state()) {
          case DRNodeState::kNone:
            gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kNone));
            break;
          case DRNodeState::kOpen:
            gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kOpen));
            break;
          case DRNodeState::kClose:
            gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kClose));
            break;
          default:
            LOG_INST.error(Loc::current(), "The type is error!");
            break;
        }
        gp_boundary.set_rect(real_rect);
        gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(dr_node.get_layer_idx()));
        dr_node_map_struct.push(gp_boundary);

        y -= y_reduced_span;
        GPText gp_text_node_real_coord;
        gp_text_node_real_coord.set_coord(real_rect.get_lb_x(), y);
        gp_text_node_real_coord.set_text_type(static_cast<irt_int>(GPGraphType::kInfo));
        gp_text_node_real_coord.set_message(
            RTUtil::getString("(", dr_node.get_x(), " , ", dr_node.get_y(), " , ", dr_node.get_layer_idx(), ")"));
        gp_text_node_real_coord.set_layer_idx(GP_INST.getGDSIdxByRouting(dr_node.get_layer_idx()));
        gp_text_node_real_coord.set_presentation(GPTextPresentation::kLeftMiddle);
        dr_node_map_struct.push(gp_text_node_real_coord);

        y -= y_reduced_span;
        GPText gp_text_node_grid_coord;
        gp_text_node_grid_coord.set_coord(real_rect.get_lb_x(), y);
        gp_text_node_grid_coord.set_text_type(static_cast<irt_int>(GPGraphType::kInfo));
        gp_text_node_grid_coord.set_message(RTUtil::getString("(", grid_x, " , ", grid_y, " , ", dr_node.get_layer_idx(), ")"));
        gp_text_node_grid_coord.set_layer_idx(GP_INST.getGDSIdxByRouting(dr_node.get_layer_idx()));
        gp_text_node_grid_coord.set_presentation(GPTextPresentation::kLeftMiddle);
        dr_node_map_struct.push(gp_text_node_grid_coord);

        y -= y_reduced_span;
        GPText gp_text_direction_set;
        gp_text_direction_set.set_coord(real_rect.get_lb_x(), y);
        gp_text_direction_set.set_text_type(static_cast<irt_int>(GPGraphType::kInfo));
        gp_text_direction_set.set_message("direction_set: ");
        gp_text_direction_set.set_layer_idx(GP_INST.getGDSIdxByRouting(dr_node.get_layer_idx()));
        gp_text_direction_set.set_presentation(GPTextPresentation::kLeftMiddle);
        dr_node_map_struct.push(gp_text_direction_set);

        if (!dr_node.get_direction_set().empty()) {
          y -= y_reduced_span;
          GPText gp_text_direction_set_info;
          gp_text_direction_set_info.set_coord(real_rect.get_lb_x(), y);
          gp_text_direction_set_info.set_text_type(static_cast<irt_int>(GPGraphType::kInfo));
          std::string direction_set_info_message = "--";
          for (Direction direction : dr_node.get_direction_set()) {
            direction_set_info_message += RTUtil::getString("(", GetDirectionName()(direction), ")");
          }
          gp_text_direction_set_info.set_message(direction_set_info_message);
          gp_text_direction_set_info.set_layer_idx(GP_INST.getGDSIdxByRouting(dr_node.get_layer_idx()));
          gp_text_direction_set_info.set_presentation(GPTextPresentation::kLeftMiddle);
          dr_node_map_struct.push(gp_text_direction_set_info);
        }
      }
    }
  }
  gp_gds.addStruct(dr_node_map_struct);

  // neighbor_map
  GPStruct neighbor_map_struct("neighbor_map");
  for (GridMap<DRNode>& dr_node_map : layer_node_map) {
    for (irt_int grid_x = 0; grid_x < dr_node_map.get_x_size(); grid_x++) {
      for (irt_int grid_y = 0; grid_y < dr_node_map.get_y_size(); grid_y++) {
        DRNode& dr_node = dr_node_map[grid_x][grid_y];
        PlanarRect real_rect = RTUtil::getEnlargedRect(dr_node.get_planar_coord(), width);

        irt_int lb_x = real_rect.get_lb_x();
        irt_int lb_y = real_rect.get_lb_y();
        irt_int rt_x = real_rect.get_rt_x();
        irt_int rt_y = real_rect.get_rt_y();
        irt_int mid_x = (lb_x + rt_x) / 2;
        irt_int mid_y = (lb_y + rt_y) / 2;
        irt_int x_reduced_span = (rt_x - lb_x) / 4;
        irt_int y_reduced_span = (rt_y - lb_y) / 4;
        irt_int width = std::min(x_reduced_span, y_reduced_span) / 2;

        for (auto& [orientation, neighbor_node] : dr_node.get_neighbor_ptr_map()) {
          GPPath gp_path;
          switch (orientation) {
            case Orientation::kEast:
              gp_path.set_segment(rt_x - x_reduced_span, mid_y, rt_x, mid_y);
              break;
            case Orientation::kSouth:
              gp_path.set_segment(mid_x, lb_y, mid_x, lb_y + y_reduced_span);
              break;
            case Orientation::kWest:
              gp_path.set_segment(lb_x, mid_y, lb_x + x_reduced_span, mid_y);
              break;
            case Orientation::kNorth:
              gp_path.set_segment(mid_x, rt_y - y_reduced_span, mid_x, rt_y);
              break;
            case Orientation::kUp:
              gp_path.set_segment(rt_x - x_reduced_span, rt_y - y_reduced_span, rt_x, rt_y);
              break;
            case Orientation::kDown:
              gp_path.set_segment(lb_x, lb_y, lb_x + x_reduced_span, lb_y + y_reduced_span);
              break;
            default:
              LOG_INST.error(Loc::current(), "The orientation is oblique!");
              break;
          }
          gp_path.set_layer_idx(GP_INST.getGDSIdxByRouting(dr_node.get_layer_idx()));
          gp_path.set_width(width);
          gp_path.set_data_type(static_cast<irt_int>(GPGraphType::kNeighbor));
          neighbor_map_struct.push(gp_path);
        }
      }
    }
  }
  gp_gds.addStruct(neighbor_map_struct);

  // net_blockage_map
  for (auto& [net_idx, blockage_list] : dr_box.get_source_net_rect_map()[DRSourceType::kBlockage]) {
    GPStruct blockage_struct(RTUtil::getString("blockage@", net_idx));
    for (const LayerRect& blockage : blockage_list) {
      GPBoundary gp_boundary;
      gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kBlockage));
      gp_boundary.set_rect(blockage);
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(blockage.get_layer_idx()));
      blockage_struct.push(gp_boundary);
    }
    gp_gds.addStruct(blockage_struct);
  }

  // net_panel_result_map
  for (auto& [net_idx, panel_result_list] : dr_box.get_source_net_rect_map()[DRSourceType::kPanelResult]) {
    GPStruct panel_result_struct(RTUtil::getString("panel_result@", net_idx));
    for (const LayerRect& panel_result : panel_result_list) {
      GPBoundary gp_boundary;
      gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kPanelResult));
      gp_boundary.set_rect(panel_result);
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(panel_result.get_layer_idx()));
      panel_result_struct.push(gp_boundary);
    }
    gp_gds.addStruct(panel_result_struct);
  }

  // net_other_box_result_map
  for (auto& [net_idx, other_box_result_list] : dr_box.get_source_net_rect_map()[DRSourceType::kOtherBoxResult]) {
    GPStruct other_box_result_struct(RTUtil::getString("other_box_result@", net_idx));
    for (const LayerRect& other_box_result : other_box_result_list) {
      GPBoundary gp_boundary;
      gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kOtherBoxResult));
      gp_boundary.set_rect(other_box_result);
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(other_box_result.get_layer_idx()));
      other_box_result_struct.push(gp_boundary);
    }
    gp_gds.addStruct(other_box_result_struct);
  }

  // net_self_box_result_map
  for (auto& [net_idx, self_box_result_list] : dr_box.get_source_net_rect_map()[DRSourceType::kSelfBoxResult]) {
    GPStruct self_box_result_struct(RTUtil::getString("self_box_result@", net_idx));
    for (const LayerRect& self_box_result : self_box_result_list) {
      GPBoundary gp_boundary;
      gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kSelfBoxResult));
      gp_boundary.set_rect(self_box_result);
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(self_box_result.get_layer_idx()));
      self_box_result_struct.push(gp_boundary);
    }
    gp_gds.addStruct(self_box_result_struct);
  }

  // box_scale_axis
  GPStruct box_scale_axis_struct("box_scale_axis");
  PlanarCoord& real_lb = dr_box.get_base_region().get_lb();
  PlanarCoord& real_rt = dr_box.get_base_region().get_rt();
  ScaleAxis& box_scale_axis = dr_box.get_box_scale_axis();
  std::vector<irt_int> x_list = RTUtil::getClosedScaleList(real_lb.get_x(), real_rt.get_x(), box_scale_axis.get_x_grid_list());
  std::vector<irt_int> y_list = RTUtil::getClosedScaleList(real_lb.get_y(), real_rt.get_y(), box_scale_axis.get_y_grid_list());
  for (irt_int layer_idx = 0; layer_idx < static_cast<irt_int>(layer_node_map.size()); layer_idx++) {
    for (irt_int x : x_list) {
      GPPath gp_path;
      gp_path.set_data_type(static_cast<irt_int>(GPGraphType::kScaleAxis));
      gp_path.set_segment(x, real_lb.get_y(), x, real_rt.get_y());
      gp_path.set_layer_idx(GP_INST.getGDSIdxByRouting(layer_idx));
      box_scale_axis_struct.push(gp_path);
    }
    for (irt_int y : y_list) {
      GPPath gp_path;
      gp_path.set_data_type(static_cast<irt_int>(GPGraphType::kScaleAxis));
      gp_path.set_segment(real_lb.get_x(), y, real_rt.get_x(), y);
      gp_path.set_layer_idx(GP_INST.getGDSIdxByRouting(layer_idx));
      box_scale_axis_struct.push(gp_path);
    }
  }
  gp_gds.addStruct(box_scale_axis_struct);

  // task
  for (DRTask& dr_task : dr_box.get_dr_task_list()) {
    GPStruct task_struct(RTUtil::getString("task_", dr_task.get_task_idx(), "(net_", dr_task.get_origin_net_idx(), ")"));

    if (curr_task_idx == -1 || dr_task.get_task_idx() == curr_task_idx) {
      for (DRGroup& dr_group : dr_task.get_dr_group_list()) {
        for (auto& [coord, direction_set] : dr_group.get_coord_direction_map()) {
          GPBoundary gp_boundary;
          gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kKey));
          gp_boundary.set_rect(RTUtil::getEnlargedRect(coord, width));
          gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(coord.get_layer_idx()));
          task_struct.push(gp_boundary);
        }
      }
    }
    {
      // bounding_box
      GPBoundary gp_boundary;
      gp_boundary.set_layer_idx(0);
      gp_boundary.set_data_type(1);
      gp_boundary.set_rect(dr_task.get_bounding_box().get_base_region());
      task_struct.push(gp_boundary);
    }
    for (Segment<LayerCoord>& segment : dr_task.get_routing_segment_list()) {
      LayerCoord first_coord = segment.get_first();
      irt_int first_layer_idx = first_coord.get_layer_idx();
      LayerCoord second_coord = segment.get_second();
      irt_int second_layer_idx = second_coord.get_layer_idx();
      irt_int half_width = routing_layer_list[first_layer_idx].get_min_width() / 2;

      if (first_layer_idx == second_layer_idx) {
        GPBoundary gp_boundary;
        gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kPath));
        gp_boundary.set_rect(RTUtil::getEnlargedRect(first_coord, second_coord, half_width));
        gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(first_layer_idx));
        task_struct.push(gp_boundary);
      } else {
        RTUtil::sortASC(first_layer_idx, second_layer_idx);
        for (irt_int layer_idx = first_layer_idx; layer_idx <= second_layer_idx; layer_idx++) {
          GPBoundary gp_boundary;
          gp_boundary.set_data_type(static_cast<irt_int>(GPGraphType::kPath));
          gp_boundary.set_rect(RTUtil::getEnlargedRect(first_coord, half_width));
          gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(layer_idx));
          task_struct.push(gp_boundary);
        }
      }
    }
    gp_gds.addStruct(task_struct);
  }
  std::string gds_file_path
      = RTUtil::getString(dr_temp_directory_path, "dr_box_", dr_box.get_grid_coord().get_x(), "_", dr_box.get_grid_coord().get_y(), ".gds");
  GP_INST.plot(gp_gds, gds_file_path, false, false);
}

#endif

}  // namespace irt
