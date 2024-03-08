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
#include "TrackAssigner.hpp"

#include "GDSPlotter.hpp"
#include "LayerCoord.hpp"
#include "RTAPI.hpp"
#include "TAPanel.hpp"

namespace irt {

// public

void TrackAssigner::initInst()
{
  if (_ta_instance == nullptr) {
    _ta_instance = new TrackAssigner();
  }
}

TrackAssigner& TrackAssigner::getInst()
{
  if (_ta_instance == nullptr) {
    LOG_INST.error(Loc::current(), "The instance not initialized!");
  }
  return *_ta_instance;
}

void TrackAssigner::destroyInst()
{
  if (_ta_instance != nullptr) {
    delete _ta_instance;
    _ta_instance = nullptr;
  }
}

// function

void TrackAssigner::assign()
{
  Monitor monitor;
  LOG_INST.info(Loc::current(), "Begin assigning...");
  TAModel ta_model = initTAModel();
  setTAParameter(ta_model);
  initTAPanelMap(ta_model);
  initTATaskList(ta_model);
  buildPanelSchedule(ta_model);
  assignTAPanelMap(ta_model);
  updateSummary(ta_model);
  printSummary(ta_model);
  writeNetCSV(ta_model);
  writeViolationCSV(ta_model);
  LOG_INST.info(Loc::current(), "End assign", monitor.getStatsInfo());
}

// private

TrackAssigner* TrackAssigner::_ta_instance = nullptr;

TAModel TrackAssigner::initTAModel()
{
  std::vector<Net>& net_list = DM_INST.getDatabase().get_net_list();

  TAModel ta_model;
  ta_model.set_ta_net_list(convertToTANetList(net_list));
  return ta_model;
}

std::vector<TANet> TrackAssigner::convertToTANetList(std::vector<Net>& net_list)
{
  std::vector<TANet> ta_net_list;
  ta_net_list.reserve(net_list.size());
  for (size_t i = 0; i < net_list.size(); i++) {
    ta_net_list.emplace_back(convertToTANet(net_list[i]));
  }
  return ta_net_list;
}

TANet TrackAssigner::convertToTANet(Net& net)
{
  TANet ta_net;
  ta_net.set_origin_net(&net);
  ta_net.set_net_idx(net.get_net_idx());
  ta_net.set_connect_type(net.get_connect_type());
  for (Pin& pin : net.get_pin_list()) {
    ta_net.get_ta_pin_list().push_back(TAPin(pin));
  }
  ta_net.set_gr_result_tree(net.get_gr_result_tree());
  return ta_net;
}

void TrackAssigner::setTAParameter(TAModel& ta_model)
{
  int32_t cost_unit = 128;

  TAParameter ta_parameter(cost_unit, cost_unit, cost_unit);
  LOG_INST.info(Loc::current(), "prefer_wire_unit : ", ta_parameter.get_prefer_wire_unit());
  LOG_INST.info(Loc::current(), "corner_unit : ", ta_parameter.get_corner_unit());
  LOG_INST.info(Loc::current(), "fixed_rect_unit : ", ta_parameter.get_fixed_rect_unit());
  LOG_INST.info(Loc::current(), "routed_rect_unit : ", ta_parameter.get_routed_rect_unit());
  LOG_INST.info(Loc::current(), "violation_unit : ", ta_parameter.get_violation_unit());
  ta_model.set_ta_parameter(ta_parameter);
}

void TrackAssigner::initTAPanelMap(TAModel& ta_model)
{
  ScaleAxis& gcell_axis = DM_INST.getDatabase().get_gcell_axis();
  Die& die = DM_INST.getDatabase().get_die();
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  TAParameter& ta_parameter = ta_model.get_ta_parameter();
  std::vector<std::vector<TAPanel>>& layer_panel_list = ta_model.get_layer_panel_list();
  for (RoutingLayer& routing_layer : routing_layer_list) {
    std::vector<TAPanel> ta_panel_list;
    if (routing_layer.isPreferH()) {
      for (ScaleGrid& gcell_grid : gcell_axis.get_y_grid_list()) {
        for (int32_t line = gcell_grid.get_start_line(); line < gcell_grid.get_end_line(); line += gcell_grid.get_step_length()) {
          TAPanel ta_panel;
          EXTLayerRect ta_panel_rect;
          ta_panel_rect.set_real_lb(die.get_real_lb_x(), line);
          ta_panel_rect.set_real_rt(die.get_real_rt_x(), line + gcell_grid.get_step_length());
          ta_panel_rect.set_grid_rect(RTUtil::getOpenGCellGridRect(ta_panel_rect.get_real_rect(), gcell_axis));
          ta_panel_rect.set_layer_idx(routing_layer.get_layer_idx());
          ta_panel.set_panel_rect(ta_panel_rect);
          TAPanelId ta_panel_id;
          ta_panel_id.set_layer_idx(routing_layer.get_layer_idx());
          ta_panel_id.set_panel_idx(static_cast<int32_t>(ta_panel_list.size()));
          ta_panel.set_ta_panel_id(ta_panel_id);
          ta_panel.set_ta_parameter(&ta_parameter);
          ta_panel_list.push_back(ta_panel);
        }
      }
    } else {
      for (ScaleGrid& gcell_grid : gcell_axis.get_x_grid_list()) {
        for (int32_t line = gcell_grid.get_start_line(); line < gcell_grid.get_end_line(); line += gcell_grid.get_step_length()) {
          TAPanel ta_panel;
          EXTLayerRect ta_panel_rect;
          ta_panel_rect.set_real_lb(line, die.get_real_lb_y());
          ta_panel_rect.set_real_rt(line + gcell_grid.get_step_length(), die.get_real_rt_y());
          ta_panel_rect.set_grid_rect(RTUtil::getOpenGCellGridRect(ta_panel_rect.get_real_rect(), gcell_axis));
          ta_panel_rect.set_layer_idx(routing_layer.get_layer_idx());
          ta_panel.set_panel_rect(ta_panel_rect);
          TAPanelId ta_panel_id;
          ta_panel_id.set_layer_idx(routing_layer.get_layer_idx());
          ta_panel_id.set_panel_idx(static_cast<int32_t>(ta_panel_list.size()));
          ta_panel.set_ta_panel_id(ta_panel_id);
          ta_panel.set_ta_parameter(&ta_parameter);
          ta_panel_list.push_back(ta_panel);
        }
      }
    }
    layer_panel_list.push_back(ta_panel_list);
  }
}

void TrackAssigner::initTATaskList(TAModel& ta_model)
{
  std::vector<TANet>& ta_net_list = ta_model.get_ta_net_list();
  std::vector<std::vector<TAPanel>>& layer_panel_list = ta_model.get_layer_panel_list();

  std::vector<std::map<TAPanelId, std::vector<TATask*>, CmpTAPanelId>> panel_task_map_list;
  panel_task_map_list.resize(ta_net_list.size());
#pragma omp parallel for
  for (size_t i = 0; i < ta_net_list.size(); i++) {
    panel_task_map_list[i] = getPanelTaskMap(ta_net_list[i]);
  }
  GridMap<std::vector<TATask*>> task_list_map;
  {
    int32_t x_size = static_cast<int32_t>(layer_panel_list.size());
    int32_t y_size = INT_MIN;
    for (std::vector<TAPanel>& ta_panel_list : layer_panel_list) {
      y_size = std::max(y_size, static_cast<int32_t>(ta_panel_list.size()));
    }
    task_list_map.init(x_size, y_size);
  }
  for (std::map<TAPanelId, std::vector<TATask*>, CmpTAPanelId>& panel_task_map : panel_task_map_list) {
    for (auto& [ta_panel_id, ta_task_list] : panel_task_map) {
      for (TATask* ta_task : ta_task_list) {
        task_list_map[ta_panel_id.get_layer_idx()][ta_panel_id.get_panel_idx()].push_back(ta_task);
      }
    }
  }

#pragma omp parallel for collapse(2)
  for (int x = 0; x < task_list_map.get_x_size(); x++) {
    for (int y = 0; y < task_list_map.get_y_size(); y++) {
      std::vector<TATask*>& ta_task_list = task_list_map[x][y];
      if (ta_task_list.empty()) {
        continue;
      }
      for (size_t i = 0; i < ta_task_list.size(); i++) {
        ta_task_list[i]->set_task_idx(static_cast<int32_t>(i));
      }
      std::sort(ta_task_list.begin(), ta_task_list.end(), CmpTATask());
      layer_panel_list[x][y].set_ta_task_list(ta_task_list);
    }
  }
}

std::map<TAPanelId, std::vector<TATask*>, CmpTAPanelId> TrackAssigner::getPanelTaskMap(TANet& ta_net)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  std::map<TAPanelId, std::vector<TATask*>, CmpTAPanelId> panel_task_map;
  for (Segment<TNode<Guide>*>& segment : RTUtil::getSegListByTree(ta_net.get_gr_result_tree())) {
    Guide lb_guide = segment.get_first()->value();
    Guide rt_guide = segment.get_second()->value();
    PlanarCoord& lb_grid_coord = lb_guide.get_grid_coord().get_planar_coord();
    PlanarCoord& rt_grid_coord = rt_guide.get_grid_coord().get_planar_coord();
    int32_t guide_layer_idx = lb_guide.get_layer_idx();

    if (lb_grid_coord == rt_grid_coord) {
      continue;
    }
    // ta_panel_id
    TAPanelId ta_panel_id;
    ta_panel_id.set_layer_idx(guide_layer_idx);
    if (RTUtil::isHorizontal(lb_grid_coord, rt_grid_coord)) {
      ta_panel_id.set_panel_idx(lb_grid_coord.get_y());
    } else {
      ta_panel_id.set_panel_idx(lb_grid_coord.get_x());
    }
    // ta_task
    RoutingLayer& routing_layer = routing_layer_list[guide_layer_idx];
    std::vector<ScaleGrid>& x_track_grid_list = routing_layer.getXTrackGridList();
    std::vector<ScaleGrid>& y_track_grid_list = routing_layer.getYTrackGridList();

    std::vector<TAGroup> ta_group_list(2);
    if (RTUtil::isHorizontal(lb_grid_coord, rt_grid_coord)) {
      RTUtil::swapByCMP(lb_guide, rt_guide, [](Guide& a, Guide& b) {
        return CmpPlanarCoordByXASC()(a.get_grid_coord().get_planar_coord(), b.get_grid_coord().get_planar_coord());
      });
      int32_t lb_x = RTUtil::getScaleList(lb_guide.get_lb_x(), lb_guide.get_rt_x(), x_track_grid_list).back();
      int32_t rt_x = RTUtil::getScaleList(rt_guide.get_lb_x(), rt_guide.get_rt_x(), x_track_grid_list).front();
      for (int32_t y : RTUtil::getScaleList(lb_guide.get_lb_y(), lb_guide.get_rt_y(), y_track_grid_list)) {
        ta_group_list.front().get_coord_list().emplace_back(lb_x, y, guide_layer_idx);
        ta_group_list.back().get_coord_list().emplace_back(rt_x, y, guide_layer_idx);
      }
    } else if (RTUtil::isVertical(lb_grid_coord, rt_grid_coord)) {
      RTUtil::swapByCMP(lb_guide, rt_guide, [](Guide& a, Guide& b) {
        return CmpPlanarCoordByYASC()(a.get_grid_coord().get_planar_coord(), b.get_grid_coord().get_planar_coord());
      });
      int32_t lb_y = RTUtil::getScaleList(lb_guide.get_lb_y(), lb_guide.get_rt_y(), y_track_grid_list).back();
      int32_t rt_y = RTUtil::getScaleList(rt_guide.get_lb_y(), rt_guide.get_rt_y(), y_track_grid_list).front();
      for (int32_t x : RTUtil::getScaleList(lb_guide.get_lb_x(), lb_guide.get_rt_x(), x_track_grid_list)) {
        ta_group_list.front().get_coord_list().emplace_back(x, lb_y, guide_layer_idx);
        ta_group_list.back().get_coord_list().emplace_back(x, rt_y, guide_layer_idx);
      }
    }
    TATask* ta_task = new TATask();
    ta_task->set_net_idx(ta_net.get_net_idx());
    ta_task->set_connect_type(ta_net.get_connect_type());
    ta_task->set_ta_group_list(ta_group_list);
    buildBoundingBox(ta_task);
    ta_task->set_routed_times(0);
    panel_task_map[ta_panel_id].push_back(ta_task);
  }
  return panel_task_map;
}

void TrackAssigner::buildBoundingBox(TATask* ta_task)
{
  std::vector<PlanarCoord> coord_list;
  for (TAGroup& ta_group : ta_task->get_ta_group_list()) {
    for (LayerCoord& coord : ta_group.get_coord_list()) {
      coord_list.push_back(coord);
    }
  }
  ta_task->set_bounding_box(RTUtil::getBoundingBox(coord_list));
}

void TrackAssigner::buildPanelSchedule(TAModel& ta_model)
{
  std::vector<std::vector<TAPanel>>& layer_panel_list = ta_model.get_layer_panel_list();

  int32_t range = 2;

  std::vector<std::vector<TAPanelId>> ta_panel_id_list_list;
  for (int32_t layer_idx = 0; layer_idx < static_cast<int32_t>(layer_panel_list.size()); layer_idx++) {
    for (int32_t start_i = 0; start_i < range; start_i++) {
      std::vector<TAPanelId> ta_panel_id_list;
      for (int32_t i = start_i; i < static_cast<int32_t>(layer_panel_list[layer_idx].size()); i += range) {
        ta_panel_id_list.emplace_back(layer_idx, i);
      }
      ta_panel_id_list_list.push_back(ta_panel_id_list);
    }
  }
  ta_model.set_ta_panel_id_list_list(ta_panel_id_list_list);
}

void TrackAssigner::assignTAPanelMap(TAModel& ta_model)
{
  std::vector<std::vector<TAPanel>>& layer_panel_list = ta_model.get_layer_panel_list();

  size_t total_panel_num = 0;
  for (std::vector<TAPanelId>& ta_panel_id_list : ta_model.get_ta_panel_id_list_list()) {
    total_panel_num += ta_panel_id_list.size();
  }

  size_t assigned_panel_num = 0;
  for (std::vector<TAPanelId>& ta_panel_id_list : ta_model.get_ta_panel_id_list_list()) {
    Monitor stage_monitor;
#pragma omp parallel for
    for (TAPanelId& ta_panel_id : ta_panel_id_list) {
      TAPanel& ta_panel = layer_panel_list[ta_panel_id.get_layer_idx()][ta_panel_id.get_panel_idx()];
      if (ta_panel.get_ta_task_list().empty()) {
        continue;
      }
      buildFixedRectList(ta_panel);
      buildPanelTrackAxis(ta_panel);
      initTANodeMap(ta_panel);
      buildTANodeNeighbor(ta_panel);
      buildOrienNetMap(ta_panel);
      // debugCheckTAPanel(ta_panel);
      routeTAPanel(ta_panel);
      updateTATaskToGcellMap(ta_panel);
      updateViolationToGcellMap(ta_panel);
      // debugPlotTAPanel(ta_panel, -1, "routed");
      freeTAPanel(ta_panel);
    }
    assigned_panel_num += ta_panel_id_list.size();
    LOG_INST.info(Loc::current(), "Assigned ", assigned_panel_num, "/", total_panel_num, "(",
                  RTUtil::getPercentage(assigned_panel_num, total_panel_num), ") panels", stage_monitor.getStatsInfo());
  }
}

void TrackAssigner::buildFixedRectList(TAPanel& ta_panel)
{
  for (auto& [is_routing, layer_net_fixed_rect_map] : DM_INST.getTypeLayerNetFixedRectMap(ta_panel.get_panel_rect())) {
    for (auto& [layer_idx, net_fixed_rect_map] : layer_net_fixed_rect_map) {
      if (is_routing == true && layer_idx == ta_panel.get_panel_rect().get_layer_idx()) {
        ta_panel.set_net_fixed_rect_map(net_fixed_rect_map);
        break;
      }
    }
  }
}

void TrackAssigner::buildPanelTrackAxis(TAPanel& ta_panel)
{
  std::vector<int32_t> x_scale_list;
  std::vector<int32_t> y_scale_list;

  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    for (TAGroup& ta_group : ta_task->get_ta_group_list()) {
      for (LayerCoord& coord : ta_group.get_coord_list()) {
        x_scale_list.push_back(coord.get_x());
        y_scale_list.push_back(coord.get_y());
      }
    }
  }

  ScaleAxis& panel_track_axis = ta_panel.get_panel_track_axis();
  std::sort(x_scale_list.begin(), x_scale_list.end());
  x_scale_list.erase(std::unique(x_scale_list.begin(), x_scale_list.end()), x_scale_list.end());
  panel_track_axis.set_x_grid_list(RTUtil::makeScaleGridList(x_scale_list));
  std::sort(y_scale_list.begin(), y_scale_list.end());
  y_scale_list.erase(std::unique(y_scale_list.begin(), y_scale_list.end()), y_scale_list.end());
  panel_track_axis.set_y_grid_list(RTUtil::makeScaleGridList(y_scale_list));
}

void TrackAssigner::initTANodeMap(TAPanel& ta_panel)
{
  PlanarCoord& real_lb = ta_panel.get_panel_rect().get_real_lb();
  PlanarCoord& real_rt = ta_panel.get_panel_rect().get_real_rt();
  ScaleAxis& panel_track_axis = ta_panel.get_panel_track_axis();
  std::vector<int32_t> x_list = RTUtil::getScaleList(real_lb.get_x(), real_rt.get_x(), panel_track_axis.get_x_grid_list());
  std::vector<int32_t> y_list = RTUtil::getScaleList(real_lb.get_y(), real_rt.get_y(), panel_track_axis.get_y_grid_list());

  GridMap<TANode>& ta_node_map = ta_panel.get_ta_node_map();
  ta_node_map.init(x_list.size(), y_list.size());
  for (size_t x = 0; x < x_list.size(); x++) {
    for (size_t y = 0; y < y_list.size(); y++) {
      TANode& ta_node = ta_node_map[x][y];
      ta_node.set_x(x_list[x]);
      ta_node.set_y(y_list[y]);
      ta_node.set_layer_idx(ta_panel.get_panel_rect().get_layer_idx());
    }
  }
}

void TrackAssigner::buildTANodeNeighbor(TAPanel& ta_panel)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();

  GridMap<TANode>& ta_node_map = ta_panel.get_ta_node_map();
  for (int32_t x = 0; x < ta_node_map.get_x_size(); x++) {
    for (int32_t y = 0; y < ta_node_map.get_y_size(); y++) {
      std::map<Orientation, TANode*>& neighbor_node_map = ta_node_map[x][y].get_neighbor_node_map();
      if (routing_layer_list[ta_panel.get_panel_rect().get_layer_idx()].isPreferH()) {
        if (x != 0) {
          neighbor_node_map[Orientation::kWest] = &ta_node_map[x - 1][y];
        }
        if (x != (ta_node_map.get_x_size() - 1)) {
          neighbor_node_map[Orientation::kEast] = &ta_node_map[x + 1][y];
        }
      } else {
        if (y != 0) {
          neighbor_node_map[Orientation::kSouth] = &ta_node_map[x][y - 1];
        }
        if (y != (ta_node_map.get_y_size() - 1)) {
          neighbor_node_map[Orientation::kNorth] = &ta_node_map[x][y + 1];
        }
      }
    }
  }
}

void TrackAssigner::buildOrienNetMap(TAPanel& ta_panel)
{
  for (auto& [net_idx, fixed_rect_set] : ta_panel.get_net_fixed_rect_map()) {
    for (auto& fixed_rect : fixed_rect_set) {
      updateFixedRectToGraph(ta_panel, ChangeType::kAdd, net_idx, fixed_rect, true);
    }
  }
}

void TrackAssigner::routeTAPanel(TAPanel& ta_panel)
{
  std::vector<TATask*> ta_task_list = initTaskSchedule(ta_panel);
  while (!ta_task_list.empty()) {
    for (TATask* ta_task : ta_task_list) {
      routeTATask(ta_panel, ta_task);
      ta_task->addRoutedTimes();
    }
    updateViolationList(ta_panel);
    ta_task_list = getTaskScheduleByViolation(ta_panel);
  }
}

std::vector<TATask*> TrackAssigner::initTaskSchedule(TAPanel& ta_panel)
{
  std::vector<TATask*> ta_task_list;
  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    ta_task_list.push_back(ta_task);
  }
  return ta_task_list;
}

void TrackAssigner::routeTATask(TAPanel& ta_panel, TATask* ta_task)
{
  initSingleTask(ta_panel, ta_task);
  while (!isConnectedAllEnd(ta_panel)) {
    routeSinglePath(ta_panel);
    updatePathResult(ta_panel);
    updateDirectionSet(ta_panel);
    resetStartAndEnd(ta_panel);
    resetSinglePath(ta_panel);
  }
  updateTaskResult(ta_panel);
  resetSingleTask(ta_panel);
}

void TrackAssigner::initSingleTask(TAPanel& ta_panel, TATask* ta_task)
{
  ScaleAxis& panel_track_axis = ta_panel.get_panel_track_axis();
  GridMap<TANode>& ta_node_map = ta_panel.get_ta_node_map();

  // single task
  ta_panel.set_curr_ta_task(ta_task);
  {
    std::vector<std::vector<TANode*>> node_list_list;
    std::vector<TAGroup>& ta_group_list = ta_task->get_ta_group_list();
    for (TAGroup& ta_group : ta_group_list) {
      std::vector<TANode*> node_comb;
      for (LayerCoord& coord : ta_group.get_coord_list()) {
        if (!RTUtil::existTrackGrid(coord, panel_track_axis)) {
          LOG_INST.error(Loc::current(), "The coord can not find grid!");
        }
        PlanarCoord grid_coord = RTUtil::getTrackGrid(coord, panel_track_axis);
        TANode& ta_node = ta_node_map[grid_coord.get_x()][grid_coord.get_y()];
        node_comb.push_back(&ta_node);
      }
      node_list_list.push_back(node_comb);
    }
    for (size_t i = 0; i < node_list_list.size(); i++) {
      if (i == 0) {
        ta_panel.get_start_node_list_list().push_back(node_list_list[i]);
      } else {
        ta_panel.get_end_node_list_list().push_back(node_list_list[i]);
      }
    }
  }
  ta_panel.get_path_node_list().clear();
  ta_panel.get_single_task_visited_node_list().clear();
  ta_panel.get_routing_segment_list().clear();
}

bool TrackAssigner::isConnectedAllEnd(TAPanel& ta_panel)
{
  return ta_panel.get_end_node_list_list().empty();
}

void TrackAssigner::routeSinglePath(TAPanel& ta_panel)
{
  initPathHead(ta_panel);
  while (!searchEnded(ta_panel)) {
    expandSearching(ta_panel);
    resetPathHead(ta_panel);
  }
}

void TrackAssigner::initPathHead(TAPanel& ta_panel)
{
  std::vector<std::vector<TANode*>>& start_node_list_list = ta_panel.get_start_node_list_list();
  std::vector<TANode*>& path_node_list = ta_panel.get_path_node_list();

  for (std::vector<TANode*>& start_node_comb : start_node_list_list) {
    for (TANode* start_node : start_node_comb) {
      start_node->set_estimated_cost(getEstimateCostToEnd(ta_panel, start_node));
      pushToOpenList(ta_panel, start_node);
    }
  }
  for (TANode* path_node : path_node_list) {
    path_node->set_estimated_cost(getEstimateCostToEnd(ta_panel, path_node));
    pushToOpenList(ta_panel, path_node);
  }
  resetPathHead(ta_panel);
}

bool TrackAssigner::searchEnded(TAPanel& ta_panel)
{
  std::vector<std::vector<TANode*>>& end_node_list_list = ta_panel.get_end_node_list_list();
  TANode* path_head_node = ta_panel.get_path_head_node();

  if (path_head_node == nullptr) {
    ta_panel.set_end_node_comb_idx(-1);
    return true;
  }
  for (size_t i = 0; i < end_node_list_list.size(); i++) {
    for (TANode* end_node : end_node_list_list[i]) {
      if (path_head_node == end_node) {
        ta_panel.set_end_node_comb_idx(static_cast<int32_t>(i));
        return true;
      }
    }
  }
  return false;
}

void TrackAssigner::expandSearching(TAPanel& ta_panel)
{
  PriorityQueue<TANode*, std::vector<TANode*>, CmpTANodeCost>& open_queue = ta_panel.get_open_queue();
  TANode* path_head_node = ta_panel.get_path_head_node();

  for (auto& [orientation, neighbor_node] : path_head_node->get_neighbor_node_map()) {
    if (neighbor_node == nullptr) {
      continue;
    }
    if (!RTUtil::isInside(ta_panel.get_curr_ta_task()->get_bounding_box(), *neighbor_node)) {
      continue;
    }
    if (neighbor_node->isClose()) {
      continue;
    }
    double know_cost = getKnowCost(ta_panel, path_head_node, neighbor_node);
    if (neighbor_node->isOpen() && know_cost < neighbor_node->get_known_cost()) {
      neighbor_node->set_known_cost(know_cost);
      neighbor_node->set_parent_node(path_head_node);
      // 对优先队列中的值修改了，需要重新建堆
      std::make_heap(open_queue.begin(), open_queue.end(), CmpTANodeCost());
    } else if (neighbor_node->isNone()) {
      neighbor_node->set_known_cost(know_cost);
      neighbor_node->set_parent_node(path_head_node);
      neighbor_node->set_estimated_cost(getEstimateCostToEnd(ta_panel, neighbor_node));
      pushToOpenList(ta_panel, neighbor_node);
    }
  }
}

void TrackAssigner::resetPathHead(TAPanel& ta_panel)
{
  ta_panel.set_path_head_node(popFromOpenList(ta_panel));
}

bool TrackAssigner::isRoutingFailed(TAPanel& ta_panel)
{
  return ta_panel.get_end_node_comb_idx() == -1;
}

void TrackAssigner::resetSinglePath(TAPanel& ta_panel)
{
  PriorityQueue<TANode*, std::vector<TANode*>, CmpTANodeCost> empty_queue;
  ta_panel.set_open_queue(empty_queue);

  std::vector<TANode*>& single_path_visited_node_list = ta_panel.get_single_path_visited_node_list();
  for (TANode* visited_node : single_path_visited_node_list) {
    visited_node->set_state(TANodeState::kNone);
    visited_node->set_parent_node(nullptr);
    visited_node->set_known_cost(0);
    visited_node->set_estimated_cost(0);
  }
  single_path_visited_node_list.clear();

  ta_panel.set_path_head_node(nullptr);
  ta_panel.set_end_node_comb_idx(-1);
}

void TrackAssigner::updatePathResult(TAPanel& ta_panel)
{
  for (Segment<LayerCoord>& routing_segment : getRoutingSegmentListByNode(ta_panel.get_path_head_node())) {
    ta_panel.get_routing_segment_list().push_back(routing_segment);
  }
}

std::vector<Segment<LayerCoord>> TrackAssigner::getRoutingSegmentListByNode(TANode* node)
{
  std::vector<Segment<LayerCoord>> routing_segment_list;

  TANode* curr_node = node;
  TANode* pre_node = curr_node->get_parent_node();

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

void TrackAssigner::updateDirectionSet(TAPanel& ta_panel)
{
  TANode* path_head_node = ta_panel.get_path_head_node();

  TANode* curr_node = path_head_node;
  TANode* pre_node = curr_node->get_parent_node();
  while (pre_node != nullptr) {
    curr_node->get_direction_set().insert(RTUtil::getDirection(*curr_node, *pre_node));
    pre_node->get_direction_set().insert(RTUtil::getDirection(*pre_node, *curr_node));
    curr_node = pre_node;
    pre_node = curr_node->get_parent_node();
  }
}

void TrackAssigner::resetStartAndEnd(TAPanel& ta_panel)
{
  std::vector<std::vector<TANode*>>& start_node_list_list = ta_panel.get_start_node_list_list();
  std::vector<std::vector<TANode*>>& end_node_list_list = ta_panel.get_end_node_list_list();
  std::vector<TANode*>& path_node_list = ta_panel.get_path_node_list();
  TANode* path_head_node = ta_panel.get_path_head_node();
  int32_t end_node_comb_idx = ta_panel.get_end_node_comb_idx();

  // 对于抵达的终点pin，只保留到达的node
  end_node_list_list[end_node_comb_idx].clear();
  end_node_list_list[end_node_comb_idx].push_back(path_head_node);

  TANode* path_node = path_head_node->get_parent_node();
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
  if (start_node_list_list.size() == 1) {
    // 初始化时，要把start_node_list_list的pin只留一个ap点
    // 后续只要将end_node_list_list的pin保留一个ap点
    start_node_list_list.front().clear();
    start_node_list_list.front().push_back(path_node);
  }
  start_node_list_list.push_back(end_node_list_list[end_node_comb_idx]);
  end_node_list_list.erase(end_node_list_list.begin() + end_node_comb_idx);
}

void TrackAssigner::updateTaskResult(TAPanel& ta_panel)
{
  std::vector<Segment<LayerCoord>> new_routing_segment_list = getRoutingSegmentList(ta_panel);

  TATask* curr_ta_task = ta_panel.get_curr_ta_task();
  // 原结果从graph删除
  for (Segment<LayerCoord>& routing_segment : curr_ta_task->get_routing_segment_list()) {
    updateNetResultToGraph(ta_panel, ChangeType::kDel, curr_ta_task->get_net_idx(), routing_segment);
  }
  curr_ta_task->set_routing_segment_list(new_routing_segment_list);
  // 新结果添加到graph
  for (Segment<LayerCoord>& routing_segment : curr_ta_task->get_routing_segment_list()) {
    updateNetResultToGraph(ta_panel, ChangeType::kAdd, curr_ta_task->get_net_idx(), routing_segment);
  }
}

std::vector<Segment<LayerCoord>> TrackAssigner::getRoutingSegmentList(TAPanel& ta_panel)
{
  TATask* curr_ta_task = ta_panel.get_curr_ta_task();

  std::vector<LayerCoord> driving_grid_coord_list;
  std::map<LayerCoord, std::set<int32_t>, CmpLayerCoordByXASC> key_coord_pin_map;
  std::vector<TAGroup>& ta_group_list = curr_ta_task->get_ta_group_list();
  for (size_t i = 0; i < ta_group_list.size(); i++) {
    for (LayerCoord& coord : ta_group_list[i].get_coord_list()) {
      driving_grid_coord_list.push_back(coord);
      key_coord_pin_map[coord].insert(static_cast<int32_t>(i));
    }
  }
  // 构建 优化 检查 routing_segment_list
  MTree<LayerCoord> coord_tree = RTUtil::getTreeByFullFlow(driving_grid_coord_list, ta_panel.get_routing_segment_list(), key_coord_pin_map);

  std::vector<Segment<LayerCoord>> routing_segment_list;
  for (Segment<TNode<LayerCoord>*>& segment : RTUtil::getSegListByTree(coord_tree)) {
    routing_segment_list.emplace_back(segment.get_first()->value(), segment.get_second()->value());
  }
  return routing_segment_list;
}

void TrackAssigner::resetSingleTask(TAPanel& ta_panel)
{
  ta_panel.set_curr_ta_task(nullptr);
  ta_panel.get_start_node_list_list().clear();
  ta_panel.get_end_node_list_list().clear();
  ta_panel.get_path_node_list().clear();

  std::vector<TANode*>& single_task_visited_node_list = ta_panel.get_single_task_visited_node_list();
  for (TANode* single_task_visited_node : single_task_visited_node_list) {
    single_task_visited_node->get_direction_set().clear();
  }
  single_task_visited_node_list.clear();

  ta_panel.get_routing_segment_list().clear();
}

// manager open list

void TrackAssigner::pushToOpenList(TAPanel& ta_panel, TANode* curr_node)
{
  PriorityQueue<TANode*, std::vector<TANode*>, CmpTANodeCost>& open_queue = ta_panel.get_open_queue();
  std::vector<TANode*>& single_task_visited_node_list = ta_panel.get_single_task_visited_node_list();
  std::vector<TANode*>& single_path_visited_node_list = ta_panel.get_single_path_visited_node_list();

  open_queue.push(curr_node);
  curr_node->set_state(TANodeState::kOpen);
  single_task_visited_node_list.push_back(curr_node);
  single_path_visited_node_list.push_back(curr_node);
}

TANode* TrackAssigner::popFromOpenList(TAPanel& ta_panel)
{
  PriorityQueue<TANode*, std::vector<TANode*>, CmpTANodeCost>& open_queue = ta_panel.get_open_queue();

  TANode* node = nullptr;
  if (!open_queue.empty()) {
    node = open_queue.top();
    open_queue.pop();
    node->set_state(TANodeState::kClose);
  }
  return node;
}

// calculate known cost

double TrackAssigner::getKnowCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  bool exist_neighbor = false;
  for (auto& [orientation, neighbor_ptr] : start_node->get_neighbor_node_map()) {
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
  cost += getNodeCost(ta_panel, start_node, RTUtil::getOrientation(*start_node, *end_node));
  cost += getNodeCost(ta_panel, end_node, RTUtil::getOrientation(*end_node, *start_node));
  cost += getKnowWireCost(ta_panel, start_node, end_node);
  cost += getKnowCornerCost(ta_panel, start_node, end_node);
  cost += getKnowViaCost(ta_panel, start_node, end_node);
  return cost;
}

double TrackAssigner::getNodeCost(TAPanel& ta_panel, TANode* curr_node, Orientation orientation)
{
  double fixed_rect_unit = ta_panel.get_ta_parameter()->get_fixed_rect_unit();
  double routed_rect_unit = ta_panel.get_ta_parameter()->get_routed_rect_unit();
  double violation_unit = ta_panel.get_ta_parameter()->get_violation_unit();

  int32_t net_idx = ta_panel.get_curr_ta_task()->get_net_idx();

  double cost = 0;
  cost += curr_node->getFixedRectCost(net_idx, orientation, fixed_rect_unit);
  cost += curr_node->getRoutedRectCost(net_idx, orientation, routed_rect_unit);
  cost += curr_node->getViolationCost(orientation, violation_unit);
  return cost;
}

double TrackAssigner::getKnowWireCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  double prefer_wire_unit = ta_panel.get_ta_parameter()->get_prefer_wire_unit();

  double wire_cost = 0;
  if (start_node->get_layer_idx() == end_node->get_layer_idx()) {
    wire_cost += RTUtil::getManhattanDistance(start_node->get_planar_coord(), end_node->get_planar_coord());

    RoutingLayer& routing_layer = routing_layer_list[start_node->get_layer_idx()];
    if (routing_layer.get_prefer_direction() == RTUtil::getDirection(*start_node, *end_node)) {
      wire_cost *= prefer_wire_unit;
    }
  }
  return wire_cost;
}

double TrackAssigner::getKnowCornerCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  double corner_unit = ta_panel.get_ta_parameter()->get_corner_unit();

  double corner_cost = 0;
  if (start_node->get_layer_idx() == end_node->get_layer_idx()) {
    std::set<Direction> direction_set;
    // 添加start direction
    std::set<Direction>& start_direction_set = start_node->get_direction_set();
    direction_set.insert(start_direction_set.begin(), start_direction_set.end());
    // 添加start到parent的direction
    if (start_node->get_parent_node() != nullptr) {
      direction_set.insert(RTUtil::getDirection(*start_node->get_parent_node(), *start_node));
    }
    // 添加end direction
    std::set<Direction>& end_direction_set = end_node->get_direction_set();
    direction_set.insert(end_direction_set.begin(), end_direction_set.end());
    // 添加start到end的direction
    direction_set.insert(RTUtil::getDirection(*start_node, *end_node));

    if (direction_set.size() == 2) {
      corner_cost += corner_unit;
    } else if (direction_set.size() == 2) {
      LOG_INST.error(Loc::current(), "Direction set is error!");
    }
  }
  return corner_cost;
}

double TrackAssigner::getKnowViaCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  return 0;
}

// calculate estimate cost

double TrackAssigner::getEstimateCostToEnd(TAPanel& ta_panel, TANode* curr_node)
{
  std::vector<std::vector<TANode*>>& end_node_list_list = ta_panel.get_end_node_list_list();

  double estimate_cost = DBL_MAX;
  for (std::vector<TANode*>& end_node_comb : end_node_list_list) {
    for (TANode* end_node : end_node_comb) {
      if (end_node->isClose()) {
        continue;
      }
      estimate_cost = std::min(estimate_cost, getEstimateCost(ta_panel, curr_node, end_node));
    }
  }
  return estimate_cost;
}

double TrackAssigner::getEstimateCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  double estimate_cost = 0;
  estimate_cost += getEstimateWireCost(ta_panel, start_node, end_node);
  estimate_cost += getEstimateCornerCost(ta_panel, start_node, end_node);
  estimate_cost += getEstimateViaCost(ta_panel, start_node, end_node);
  return estimate_cost;
}

double TrackAssigner::getEstimateWireCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  double prefer_wire_unit = ta_panel.get_ta_parameter()->get_prefer_wire_unit();

  double wire_cost = 0;
  wire_cost += RTUtil::getManhattanDistance(start_node->get_planar_coord(), end_node->get_planar_coord());
  wire_cost *= prefer_wire_unit;
  return wire_cost;
}

double TrackAssigner::getEstimateCornerCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  double corner_unit = ta_panel.get_ta_parameter()->get_corner_unit();

  double corner_cost = 0;
  if (start_node->get_layer_idx() == end_node->get_layer_idx()) {
    if (RTUtil::isOblique(*start_node, *end_node)) {
      corner_cost += corner_unit;
    }
  }
  return corner_cost;
}

double TrackAssigner::getEstimateViaCost(TAPanel& ta_panel, TANode* start_node, TANode* end_node)
{
  return 0;
}

void TrackAssigner::updateViolationList(TAPanel& ta_panel)
{
  std::vector<Violation> new_violation_list = getViolationList(ta_panel);

  // 原结果从graph删除
  for (Violation& violation : ta_panel.get_violation_list()) {
    updateViolationToGraph(ta_panel, ChangeType::kDel, violation);
  }
  ta_panel.set_violation_list(new_violation_list);
  // 新结果添加到graph
  for (Violation& violation : ta_panel.get_violation_list()) {
    updateViolationToGraph(ta_panel, ChangeType::kAdd, violation);
  }
}

std::vector<Violation> TrackAssigner::getViolationList(TAPanel& ta_panel)
{
  std::vector<idb::IdbLayerShape*> env_shape_list;
  std::map<int32_t, std::vector<idb::IdbLayerShape*>> net_pin_shape_map;
  for (auto& [net_idx, fixed_rect_set] : ta_panel.get_net_fixed_rect_map()) {
    if (net_idx == -1) {
      for (auto& fixed_rect : fixed_rect_set) {
        env_shape_list.push_back(DM_INST.getIDBLayerShapeByFixedRect(fixed_rect, true));
      }
    } else {
      for (auto& fixed_rect : fixed_rect_set) {
        net_pin_shape_map[net_idx].push_back(DM_INST.getIDBLayerShapeByFixedRect(fixed_rect, true));
      }
    }
  }
  std::map<int32_t, std::vector<idb::IdbRegularWireSegment*>> net_wire_via_map;
  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    for (Segment<LayerCoord>& routing_segment : ta_task->get_routing_segment_list()) {
      net_wire_via_map[ta_task->get_net_idx()].push_back(DM_INST.getIDBSegmentByNetResult(ta_task->get_net_idx(), routing_segment));
    }
  }
  std::vector<Violation> violation_list = RTAPI_INST.getViolationList(env_shape_list, net_pin_shape_map, net_wire_via_map);
  // free memory
  {
    for (idb::IdbLayerShape* env_shape : env_shape_list) {
      delete env_shape;
      env_shape = nullptr;
    }
    for (auto& [net_idx, pin_shape_list] : net_pin_shape_map) {
      for (idb::IdbLayerShape* pin_shape : pin_shape_list) {
        delete pin_shape;
        pin_shape = nullptr;
      }
    }
    for (auto& [net_idx, wire_via_list] : net_wire_via_map) {
      for (idb::IdbRegularWireSegment* wire_via : wire_via_list) {
        delete wire_via;
        wire_via = nullptr;
      }
    }
  }
  return violation_list;
}

std::vector<TATask*> TrackAssigner::getTaskScheduleByViolation(TAPanel& ta_panel)
{
  std::set<int32_t> violation_net_set;
  for (Violation& violation : ta_panel.get_violation_list()) {
    for (int32_t violation_net : violation.get_violation_net_set()) {
      violation_net_set.insert(violation_net);
    }
  }
  std::vector<TATask*> ta_task_list;
  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    if (!RTUtil::exist(violation_net_set, ta_task->get_net_idx())) {
      continue;
    }
    if (ta_task->get_routed_times() > 1) {
      continue;
    }
    ta_task_list.push_back(ta_task);
  }
  return ta_task_list;
}

void TrackAssigner::updateTATaskToGcellMap(TAPanel& ta_panel)
{
  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    for (Segment<LayerCoord>& routing_segment : ta_task->get_routing_segment_list()) {
      DM_INST.updateNetResultToGCellMap(ChangeType::kAdd, ta_task->get_net_idx(), new Segment<LayerCoord>(routing_segment));
    }
  }
}

void TrackAssigner::updateViolationToGcellMap(TAPanel& ta_panel)
{
  for (Violation& violation : ta_panel.get_violation_list()) {
    DM_INST.updateViolationToGCellMap(ChangeType::kAdd, new Violation(violation));
  }
}

void TrackAssigner::freeTAPanel(TAPanel& ta_panel)
{
  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    delete ta_task;
    ta_task = nullptr;
  }
  ta_panel.get_ta_task_list().clear();
  ta_panel.get_ta_node_map().free();
}

#if 1  // update env

void TrackAssigner::updateFixedRectToGraph(TAPanel& ta_panel, ChangeType change_type, int32_t net_idx, EXTLayerRect* fixed_rect,
                                           bool is_routing)
{
  NetShape net_shape(net_idx, fixed_rect->getRealLayerRect(), is_routing);
  for (auto& [ta_node, orientation_set] : getRoutingNodeOrientationMap(ta_panel, net_shape)) {
    for (Orientation orientation : orientation_set) {
      if (change_type == ChangeType::kAdd) {
        ta_node->get_orien_fixed_rect_map()[orientation].insert(net_shape.get_net_idx());
      } else if (change_type == ChangeType::kDel) {
        ta_node->get_orien_fixed_rect_map()[orientation].erase(net_shape.get_net_idx());
      }
    }
  }
}

void TrackAssigner::updateNetResultToGraph(TAPanel& ta_panel, ChangeType change_type, int32_t net_idx, Segment<LayerCoord>& segment)
{
  for (NetShape& net_shape : DM_INST.getNetShapeList(net_idx, segment)) {
    for (auto& [ta_node, orientation_set] : getRoutingNodeOrientationMap(ta_panel, net_shape)) {
      for (Orientation orientation : orientation_set) {
        if (change_type == ChangeType::kAdd) {
          ta_node->get_orien_routed_rect_map()[orientation].insert(net_shape.get_net_idx());
        } else if (change_type == ChangeType::kDel) {
          ta_node->get_orien_routed_rect_map()[orientation].erase(net_shape.get_net_idx());
        }
      }
    }
  }
}

void TrackAssigner::updateViolationToGraph(TAPanel& ta_panel, ChangeType change_type, Violation& violation)
{
  NetShape net_shape(-1, violation.get_violation_shape().getRealLayerRect(), violation.get_is_routing());
  for (auto& [ta_node, orientation_set] : getRoutingNodeOrientationMap(ta_panel, net_shape)) {
    for (Orientation orientation : orientation_set) {
      if (change_type == ChangeType::kAdd) {
        ta_node->get_orien_violation_number_map()[orientation]++;
      } else if (change_type == ChangeType::kDel) {
        ta_node->get_orien_violation_number_map()[orientation]--;
      }
    }
  }
}

std::map<TANode*, std::set<Orientation>> TrackAssigner::getRoutingNodeOrientationMap(TAPanel& ta_panel, NetShape& net_shape)
{
  if (!net_shape.get_is_routing()) {
    LOG_INST.error(Loc::current(), "The type of net_shape is cut!");
  }
  if (ta_panel.get_panel_rect().get_layer_idx() != net_shape.get_layer_idx()) {
    LOG_INST.error(Loc::current(), "The layer_idx is error!");
  }
  RoutingLayer& routing_layer = DM_INST.getDatabase().get_routing_layer_list()[net_shape.get_layer_idx()];

  // 膨胀size为 min_spacing + 1/2 * width
  int32_t enlarged_size = routing_layer.getMinSpacing(net_shape.get_rect()) + (routing_layer.get_min_width() / 2);
  PlanarRect enlarged_rect = RTUtil::getEnlargedRect(net_shape.get_rect(), enlarged_size);

  std::map<TANode*, std::set<Orientation>> node_orientation_map;
  if (RTUtil::existTrackGrid(enlarged_rect, ta_panel.get_panel_track_axis())) {
    PlanarRect grid_rect = RTUtil::getTrackGridRect(enlarged_rect, ta_panel.get_panel_track_axis());
    for (int32_t grid_x = grid_rect.get_lb_x(); grid_x <= grid_rect.get_rt_x(); grid_x++) {
      for (int32_t grid_y = grid_rect.get_lb_y(); grid_y <= grid_rect.get_rt_y(); grid_y++) {
        TANode& node = ta_panel.get_ta_node_map()[grid_x][grid_y];
        for (auto& [orientation, neigbor_ptr] : node.get_neighbor_node_map()) {
          node_orientation_map[&node].insert(orientation);
          node_orientation_map[neigbor_ptr].insert(RTUtil::getOppositeOrientation(orientation));
        }
      }
    }
  }
  return node_orientation_map;
}

#endif

#if 1  // debug

void TrackAssigner::debugCheckTAPanel(TAPanel& ta_panel)
{
  TAPanelId& ta_panel_id = ta_panel.get_ta_panel_id();
  if (ta_panel.get_ta_panel_id().get_layer_idx() < 0 || ta_panel.get_ta_panel_id().get_panel_idx() < 0) {
    LOG_INST.error(Loc::current(), "The ta_panel_id is error!");
  }

  GridMap<TANode>& ta_node_map = ta_panel.get_ta_node_map();
  for (int32_t x = 0; x < ta_node_map.get_x_size(); x++) {
    for (int32_t y = 0; y < ta_node_map.get_y_size(); y++) {
      TANode& ta_node = ta_node_map[x][y];
      if (!RTUtil::isInside(ta_panel.get_panel_rect().get_real_rect(), ta_node.get_planar_coord())) {
        LOG_INST.error(Loc::current(), "The ta_node is out of panel!");
      }
      for (auto& [orien, neighbor] : ta_node.get_neighbor_node_map()) {
        Orientation opposite_orien = RTUtil::getOppositeOrientation(orien);
        if (!RTUtil::exist(neighbor->get_neighbor_node_map(), opposite_orien)) {
          LOG_INST.error(Loc::current(), "The ta_node neighbor is not bidirection!");
        }
        if (neighbor->get_neighbor_node_map()[opposite_orien] != &ta_node) {
          LOG_INST.error(Loc::current(), "The ta_node neighbor is not bidirection!");
        }
        LayerCoord node_coord(ta_node.get_planar_coord(), ta_node.get_layer_idx());
        LayerCoord neighbor_coord(neighbor->get_planar_coord(), neighbor->get_layer_idx());
        if (RTUtil::getOrientation(node_coord, neighbor_coord) == orien) {
          continue;
        }
        LOG_INST.error(Loc::current(), "The neighbor orien is different with real region!");
      }
    }
  }

  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    if (ta_task->get_net_idx() < 0) {
      LOG_INST.error(Loc::current(), "The idx of origin net is illegal!");
    }
    for (TAGroup& ta_group : ta_task->get_ta_group_list()) {
      if (ta_group.get_coord_list().empty()) {
        LOG_INST.error(Loc::current(), "The coord_direction_map is empty!");
      }
      for (LayerCoord& coord : ta_group.get_coord_list()) {
        int32_t layer_idx = coord.get_layer_idx();
        if (layer_idx != ta_panel.get_panel_rect().get_layer_idx()) {
          LOG_INST.error(Loc::current(), "The layer idx of group coord is illegal!");
        }
        if (!RTUtil::existTrackGrid(coord, ta_panel.get_panel_track_axis())) {
          LOG_INST.error(Loc::current(), "There is no grid coord for real coord(", coord.get_x(), ",", coord.get_y(), ")!");
        }
        PlanarCoord grid_coord = RTUtil::getTrackGrid(coord, ta_panel.get_panel_track_axis());
        TANode& ta_node = ta_node_map[grid_coord.get_x()][grid_coord.get_y()];
        if (ta_node.get_neighbor_node_map().empty()) {
          LOG_INST.error(Loc::current(), "The neighbor of group coord (", coord.get_x(), ",", coord.get_y(), ",", layer_idx,
                         ") is empty in panel(", ta_panel_id.get_layer_idx(), ",", ta_panel_id.get_panel_idx(), ")");
        }
        if (RTUtil::isInside(ta_panel.get_panel_rect().get_real_rect(), coord)) {
          continue;
        }
        LOG_INST.error(Loc::current(), "The coord (", coord.get_x(), ",", coord.get_y(), ") is out of panel!");
      }
    }
  }
}

void TrackAssigner::debugPlotTAPanel(TAPanel& ta_panel, int32_t curr_task_idx, std::string flag)
{
  ScaleAxis& gcell_axis = DM_INST.getDatabase().get_gcell_axis();
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = DM_INST.getDatabase().get_layer_via_master_list();
  std::string& ta_temp_directory_path = DM_INST.getConfig().ta_temp_directory_path;

  PlanarRect panel_rect = ta_panel.get_panel_rect().get_real_rect();

  int32_t width = INT_MAX;
  for (ScaleGrid& x_grid : ta_panel.get_panel_track_axis().get_x_grid_list()) {
    width = std::min(width, x_grid.get_step_length());
  }
  for (ScaleGrid& y_grid : ta_panel.get_panel_track_axis().get_y_grid_list()) {
    width = std::min(width, y_grid.get_step_length());
  }
  width = std::max(1, width / 3);

  GPGDS gp_gds;

  // base_region
  GPStruct base_region_struct("base_region");
  GPBoundary gp_boundary;
  gp_boundary.set_layer_idx(0);
  gp_boundary.set_data_type(0);
  gp_boundary.set_rect(panel_rect);
  base_region_struct.push(gp_boundary);
  gp_gds.addStruct(base_region_struct);

  // gcell_axis
  GPStruct gcell_axis_struct("gcell_axis");
  std::vector<int32_t> gcell_x_list = RTUtil::getScaleList(panel_rect.get_lb_x(), panel_rect.get_rt_x(), gcell_axis.get_x_grid_list());
  std::vector<int32_t> gcell_y_list = RTUtil::getScaleList(panel_rect.get_lb_y(), panel_rect.get_rt_y(), gcell_axis.get_y_grid_list());
  for (int32_t x : gcell_x_list) {
    GPPath gp_path;
    gp_path.set_layer_idx(0);
    gp_path.set_data_type(1);
    gp_path.set_segment(x, panel_rect.get_lb_y(), x, panel_rect.get_rt_y());
    gcell_axis_struct.push(gp_path);
  }
  for (int32_t y : gcell_y_list) {
    GPPath gp_path;
    gp_path.set_layer_idx(0);
    gp_path.set_data_type(1);
    gp_path.set_segment(panel_rect.get_lb_x(), y, panel_rect.get_rt_x(), y);
    gcell_axis_struct.push(gp_path);
  }
  gp_gds.addStruct(gcell_axis_struct);

  GridMap<TANode>& ta_node_map = ta_panel.get_ta_node_map();
  // ta_node_map
  GPStruct ta_node_map_struct("ta_node_map");
  for (int32_t grid_x = 0; grid_x < ta_node_map.get_x_size(); grid_x++) {
    for (int32_t grid_y = 0; grid_y < ta_node_map.get_y_size(); grid_y++) {
      TANode& ta_node = ta_node_map[grid_x][grid_y];
      PlanarRect real_rect = RTUtil::getEnlargedRect(ta_node.get_planar_coord(), width);
      int32_t y_reduced_span = std::max(1, real_rect.getYSpan() / 12);
      int32_t y = real_rect.get_rt_y();

      GPBoundary gp_boundary;
      switch (ta_node.get_state()) {
        case TANodeState::kNone:
          gp_boundary.set_data_type(static_cast<int32_t>(GPDataType::kNone));
          break;
        case TANodeState::kOpen:
          gp_boundary.set_data_type(static_cast<int32_t>(GPDataType::kOpen));
          break;
        case TANodeState::kClose:
          gp_boundary.set_data_type(static_cast<int32_t>(GPDataType::kClose));
          break;
        default:
          LOG_INST.error(Loc::current(), "The type is error!");
          break;
      }
      gp_boundary.set_rect(real_rect);
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_node.get_layer_idx()));
      ta_node_map_struct.push(gp_boundary);

      y -= y_reduced_span;
      GPText gp_text_node_real_coord;
      gp_text_node_real_coord.set_coord(real_rect.get_lb_x(), y);
      gp_text_node_real_coord.set_text_type(static_cast<int32_t>(GPDataType::kInfo));
      gp_text_node_real_coord.set_message(
          RTUtil::getString("(", ta_node.get_x(), " , ", ta_node.get_y(), " , ", ta_node.get_layer_idx(), ")"));
      gp_text_node_real_coord.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_node.get_layer_idx()));
      gp_text_node_real_coord.set_presentation(GPTextPresentation::kLeftMiddle);
      ta_node_map_struct.push(gp_text_node_real_coord);

      y -= y_reduced_span;
      GPText gp_text_node_grid_coord;
      gp_text_node_grid_coord.set_coord(real_rect.get_lb_x(), y);
      gp_text_node_grid_coord.set_text_type(static_cast<int32_t>(GPDataType::kInfo));
      gp_text_node_grid_coord.set_message(RTUtil::getString("(", grid_x, " , ", grid_y, " , ", ta_node.get_layer_idx(), ")"));
      gp_text_node_grid_coord.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_node.get_layer_idx()));
      gp_text_node_grid_coord.set_presentation(GPTextPresentation::kLeftMiddle);
      ta_node_map_struct.push(gp_text_node_grid_coord);

      y -= y_reduced_span;
      GPText gp_text_direction_set;
      gp_text_direction_set.set_coord(real_rect.get_lb_x(), y);
      gp_text_direction_set.set_text_type(static_cast<int32_t>(GPDataType::kInfo));
      gp_text_direction_set.set_message("direction_set: ");
      gp_text_direction_set.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_node.get_layer_idx()));
      gp_text_direction_set.set_presentation(GPTextPresentation::kLeftMiddle);
      ta_node_map_struct.push(gp_text_direction_set);

      if (!ta_node.get_direction_set().empty()) {
        y -= y_reduced_span;
        GPText gp_text_direction_set_info;
        gp_text_direction_set_info.set_coord(real_rect.get_lb_x(), y);
        gp_text_direction_set_info.set_text_type(static_cast<int32_t>(GPDataType::kInfo));
        std::string direction_set_info_message = "--";
        for (Direction direction : ta_node.get_direction_set()) {
          direction_set_info_message += RTUtil::getString("(", GetDirectionName()(direction), ")");
        }
        gp_text_direction_set_info.set_message(direction_set_info_message);
        gp_text_direction_set_info.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_node.get_layer_idx()));
        gp_text_direction_set_info.set_presentation(GPTextPresentation::kLeftMiddle);
        ta_node_map_struct.push(gp_text_direction_set_info);
      }
    }
  }
  gp_gds.addStruct(ta_node_map_struct);

  // neighbor_map
  GPStruct neighbor_map_struct("neighbor_map");
  for (int32_t grid_x = 0; grid_x < ta_node_map.get_x_size(); grid_x++) {
    for (int32_t grid_y = 0; grid_y < ta_node_map.get_y_size(); grid_y++) {
      TANode& ta_node = ta_node_map[grid_x][grid_y];
      PlanarRect real_rect = RTUtil::getEnlargedRect(ta_node.get_planar_coord(), width);

      int32_t lb_x = real_rect.get_lb_x();
      int32_t lb_y = real_rect.get_lb_y();
      int32_t rt_x = real_rect.get_rt_x();
      int32_t rt_y = real_rect.get_rt_y();
      int32_t mid_x = (lb_x + rt_x) / 2;
      int32_t mid_y = (lb_y + rt_y) / 2;
      int32_t x_reduced_span = (rt_x - lb_x) / 4;
      int32_t y_reduced_span = (rt_y - lb_y) / 4;
      int32_t width = std::min(x_reduced_span, y_reduced_span) / 2;

      for (auto& [orientation, neighbor_node] : ta_node.get_neighbor_node_map()) {
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
        gp_path.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_node.get_layer_idx()));
        gp_path.set_width(width);
        gp_path.set_data_type(static_cast<int32_t>(GPDataType::kNeighbor));
        neighbor_map_struct.push(gp_path);
      }
    }
  }
  gp_gds.addStruct(neighbor_map_struct);

  // panel_track_axis
  GPStruct panel_track_axis_struct("panel_track_axis");
  PlanarCoord& real_lb = panel_rect.get_lb();
  PlanarCoord& real_rt = panel_rect.get_rt();
  ScaleAxis& panel_track_axis = ta_panel.get_panel_track_axis();
  std::vector<int32_t> x_list = RTUtil::getScaleList(real_lb.get_x(), real_rt.get_x(), panel_track_axis.get_x_grid_list());
  std::vector<int32_t> y_list = RTUtil::getScaleList(real_lb.get_y(), real_rt.get_y(), panel_track_axis.get_y_grid_list());

  for (int32_t x : x_list) {
    GPPath gp_path;
    gp_path.set_data_type(static_cast<int32_t>(GPDataType::kAxis));
    gp_path.set_segment(x, real_lb.get_y(), x, real_rt.get_y());
    gp_path.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_panel.get_panel_rect().get_layer_idx()));
    panel_track_axis_struct.push(gp_path);
  }
  for (int32_t y : y_list) {
    GPPath gp_path;
    gp_path.set_data_type(static_cast<int32_t>(GPDataType::kAxis));
    gp_path.set_segment(real_lb.get_x(), y, real_rt.get_x(), y);
    gp_path.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_panel.get_panel_rect().get_layer_idx()));
    panel_track_axis_struct.push(gp_path);
  }
  gp_gds.addStruct(panel_track_axis_struct);

  for (auto& [net_idx, rect_set] : ta_panel.get_net_fixed_rect_map()) {
    GPStruct fixed_rect_struct(RTUtil::getString("fixed_rect(net_", net_idx, ")"));
    for (EXTLayerRect* rect : rect_set) {
      GPBoundary gp_boundary;
      gp_boundary.set_data_type(static_cast<int32_t>(GPDataType::kShape));
      gp_boundary.set_rect(rect->get_real_rect());
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(ta_panel.get_panel_rect().get_layer_idx()));
      fixed_rect_struct.push(gp_boundary);
    }
    gp_gds.addStruct(fixed_rect_struct);
  }

  GPStruct violation_struct("violation");
  for (Violation& violation : ta_panel.get_violation_list()) {
    EXTLayerRect& violation_shape = violation.get_violation_shape();

    GPBoundary gp_boundary;
    gp_boundary.set_data_type(static_cast<int32_t>(GPDataType::kViolation));
    gp_boundary.set_rect(violation_shape.get_real_rect());
    if (violation.get_is_routing()) {
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(violation_shape.get_layer_idx()));
    } else {
      gp_boundary.set_layer_idx(GP_INST.getGDSIdxByCut(violation_shape.get_layer_idx()));
    }
    violation_struct.push(gp_boundary);
  }
  gp_gds.addStruct(violation_struct);

  // task
  for (TATask* ta_task : ta_panel.get_ta_task_list()) {
    GPStruct task_struct(RTUtil::getString("task(net_", ta_task->get_net_idx(), ")"));

    if (curr_task_idx == -1 || ta_task->get_net_idx() == curr_task_idx) {
      for (TAGroup& ta_group : ta_task->get_ta_group_list()) {
        for (LayerCoord& coord : ta_group.get_coord_list()) {
          GPBoundary gp_boundary;
          gp_boundary.set_data_type(static_cast<int32_t>(GPDataType::kKey));
          gp_boundary.set_rect(RTUtil::getEnlargedRect(coord, width));
          gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(coord.get_layer_idx()));
          task_struct.push(gp_boundary);
        }
      }
    }
    // {
    //   // bounding_box
    //   GPBoundary gp_boundary;
    //   gp_boundary.set_layer_idx(0);
    //   gp_boundary.set_data_type(2);
    //   gp_boundary.set_rect(ta_task.get_bounding_box().get_base_region());
    //   task_struct.push(gp_boundary);
    // }
    for (Segment<LayerCoord>& segment : ta_task->get_routing_segment_list()) {
      LayerCoord first_coord = segment.get_first();
      LayerCoord second_coord = segment.get_second();
      int32_t first_layer_idx = first_coord.get_layer_idx();
      int32_t second_layer_idx = second_coord.get_layer_idx();
      int32_t half_width = routing_layer_list[first_layer_idx].get_min_width() / 2;

      if (first_layer_idx == second_layer_idx) {
        GPBoundary gp_boundary;
        gp_boundary.set_data_type(static_cast<int32_t>(GPDataType::kPath));
        gp_boundary.set_rect(RTUtil::getEnlargedRect(first_coord, second_coord, half_width));
        gp_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(first_layer_idx));
        task_struct.push(gp_boundary);
      } else {
        RTUtil::swapByASC(first_layer_idx, second_layer_idx);
        for (int32_t layer_idx = first_layer_idx; layer_idx < second_layer_idx; layer_idx++) {
          ViaMaster& via_master = layer_via_master_list[layer_idx].front();

          LayerRect& above_enclosure = via_master.get_above_enclosure();
          LayerRect offset_above_enclosure(RTUtil::getOffsetRect(above_enclosure, first_coord), above_enclosure.get_layer_idx());
          GPBoundary above_boundary;
          above_boundary.set_rect(offset_above_enclosure);
          above_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(above_enclosure.get_layer_idx()));
          above_boundary.set_data_type(static_cast<int32_t>(GPDataType::kPath));
          task_struct.push(above_boundary);

          LayerRect& below_enclosure = via_master.get_below_enclosure();
          LayerRect offset_below_enclosure(RTUtil::getOffsetRect(below_enclosure, first_coord), below_enclosure.get_layer_idx());
          GPBoundary below_boundary;
          below_boundary.set_rect(offset_below_enclosure);
          below_boundary.set_layer_idx(GP_INST.getGDSIdxByRouting(below_enclosure.get_layer_idx()));
          below_boundary.set_data_type(static_cast<int32_t>(GPDataType::kPath));
          task_struct.push(below_boundary);

          for (PlanarRect& cut_shape : via_master.get_cut_shape_list()) {
            LayerRect offset_cut_shape(RTUtil::getOffsetRect(cut_shape, first_coord), via_master.get_cut_layer_idx());
            GPBoundary cut_boundary;
            cut_boundary.set_rect(offset_cut_shape);
            cut_boundary.set_layer_idx(GP_INST.getGDSIdxByCut(via_master.get_cut_layer_idx()));
            cut_boundary.set_data_type(static_cast<int32_t>(GPDataType::kPath));
            task_struct.push(cut_boundary);
          }
        }
      }
    }
    gp_gds.addStruct(task_struct);
  }
  std::string gds_file_path = RTUtil::getString(ta_temp_directory_path, flag, "_ta_panel_", ta_panel.get_ta_panel_id().get_layer_idx(), "_",
                                                ta_panel.get_ta_panel_id().get_panel_idx(), ".gds");
  GP_INST.plot(gp_gds, gds_file_path);
}

#endif

#if 1  // exhibit

void TrackAssigner::updateSummary(TAModel& ta_model)
{
  int32_t micron_dbu = DM_INST.getDatabase().get_micron_dbu();
  Die& die = DM_INST.getDatabase().get_die();
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  std::map<int32_t, double>& routing_wire_length_map = DM_INST.getSummary().ta_summary.routing_wire_length_map;
  double& total_wire_length = DM_INST.getSummary().ta_summary.total_wire_length;
  std::map<int32_t, int32_t>& routing_violation_num_map = DM_INST.getSummary().ta_summary.routing_violation_num_map;
  int32_t& total_violation_num = DM_INST.getSummary().ta_summary.total_violation_num;

  for (RoutingLayer& routing_layer : routing_layer_list) {
    routing_wire_length_map[routing_layer.get_layer_idx()] = 0;
    routing_violation_num_map[routing_layer.get_layer_idx()] = 0;
  }
  total_wire_length = 0;
  total_violation_num = 0;

  for (auto& [net_idx, segment_set] : DM_INST.getNetResultMap(die)) {
    for (Segment<LayerCoord>* segment : segment_set) {
      LayerCoord& first_coord = segment->get_first();
      LayerCoord& second_coord = segment->get_second();
      if (first_coord.get_layer_idx() == second_coord.get_layer_idx()) {
        double wire_length = RTUtil::getManhattanDistance(first_coord, second_coord) / 1.0 / micron_dbu;
        routing_wire_length_map[first_coord.get_layer_idx()] += wire_length;
        total_wire_length += wire_length;
      }
    }
  }
  for (Violation* violation : DM_INST.getViolationSet(die)) {
    routing_violation_num_map[violation->get_violation_shape().get_layer_idx()]++;
    total_violation_num++;
  }
}

void TrackAssigner::printSummary(TAModel& ta_model)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  std::map<int32_t, double>& routing_wire_length_map = DM_INST.getSummary().ta_summary.routing_wire_length_map;
  double& total_wire_length = DM_INST.getSummary().ta_summary.total_wire_length;
  std::map<int32_t, int32_t>& routing_violation_num_map = DM_INST.getSummary().ta_summary.routing_violation_num_map;
  int32_t& total_violation_num = DM_INST.getSummary().ta_summary.total_violation_num;

  fort::char_table routing_wire_length_map_table;
  {
    routing_wire_length_map_table << fort::header << "routing_layer"
                                  << "wire_length"
                                  << "proportion" << fort::endr;
    for (RoutingLayer& routing_layer : routing_layer_list) {
      routing_wire_length_map_table << routing_layer.get_layer_name() << routing_wire_length_map[routing_layer.get_layer_idx()]
                                    << RTUtil::getPercentage(routing_wire_length_map[routing_layer.get_layer_idx()], total_wire_length)
                                    << fort::endr;
    }
    routing_wire_length_map_table << fort::header << "Total" << total_wire_length
                                  << RTUtil::getPercentage(total_wire_length, total_wire_length) << fort::endr;
  }
  fort::char_table routing_violation_num_map_table;
  {
    routing_violation_num_map_table << fort::header << "routing_layer"
                                    << "violation_num"
                                    << "proportion" << fort::endr;
    for (RoutingLayer& routing_layer : routing_layer_list) {
      routing_violation_num_map_table << routing_layer.get_layer_name() << routing_violation_num_map[routing_layer.get_layer_idx()]
                                      << RTUtil::getPercentage(routing_violation_num_map[routing_layer.get_layer_idx()],
                                                               total_violation_num)
                                      << fort::endr;
    }
    routing_violation_num_map_table << fort::header << "Total" << total_violation_num
                                    << RTUtil::getPercentage(total_violation_num, total_violation_num) << fort::endr;
  }
  RTUtil::printTableList({routing_wire_length_map_table, routing_violation_num_map_table});
}

void TrackAssigner::writeNetCSV(TAModel& ta_model)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  GridMap<GCell>& gcell_map = DM_INST.getDatabase().get_gcell_map();
  std::string& ta_temp_directory_path = DM_INST.getConfig().ta_temp_directory_path;
  int32_t output_csv = DM_INST.getConfig().output_csv;
  if (!output_csv) {
    return;
  }
  std::vector<GridMap<int32_t>> layer_net_map;
  layer_net_map.resize(routing_layer_list.size());
  for (GridMap<int32_t>& net_map : layer_net_map) {
    net_map.init(gcell_map.get_x_size(), gcell_map.get_y_size());
  }
  for (int32_t x = 0; x < gcell_map.get_x_size(); x++) {
    for (int32_t y = 0; y < gcell_map.get_y_size(); y++) {
      std::map<int32_t, std::set<int32_t>> net_layer_map;
      for (auto& [net_idx, segment_set] : gcell_map[x][y].get_net_result_map()) {
        for (Segment<LayerCoord>* segment : segment_set) {
          int32_t first_layer_idx = segment->get_first().get_layer_idx();
          int32_t second_layer_idx = segment->get_second().get_layer_idx();
          RTUtil::swapByASC(first_layer_idx, second_layer_idx);
          for (int32_t layer_idx = first_layer_idx; layer_idx <= second_layer_idx; layer_idx++) {
            net_layer_map[net_idx].insert(layer_idx);
          }
        }
      }
      for (auto& [net_idx, layer_set] : net_layer_map) {
        for (int32_t layer_idx : layer_set) {
          layer_net_map[layer_idx][x][y]++;
        }
      }
    }
  }
  for (RoutingLayer& routing_layer : routing_layer_list) {
    std::ofstream* net_csv_file
        = RTUtil::getOutputFileStream(RTUtil::getString(ta_temp_directory_path, "net_map_", routing_layer.get_layer_name(), ".csv"));
    GridMap<int32_t>& net_map = layer_net_map[routing_layer.get_layer_idx()];
    for (int32_t y = net_map.get_y_size() - 1; y >= 0; y--) {
      for (int32_t x = 0; x < net_map.get_x_size(); x++) {
        RTUtil::pushStream(net_csv_file, net_map[x][y], ",");
      }
      RTUtil::pushStream(net_csv_file, "\n");
    }
    RTUtil::closeFileStream(net_csv_file);
  }
}

void TrackAssigner::writeViolationCSV(TAModel& ta_model)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  GridMap<GCell>& gcell_map = DM_INST.getDatabase().get_gcell_map();
  std::string& ta_temp_directory_path = DM_INST.getConfig().ta_temp_directory_path;
  int32_t output_csv = DM_INST.getConfig().output_csv;
  if (!output_csv) {
    return;
  }
  std::vector<GridMap<int32_t>> layer_violation_map;
  layer_violation_map.resize(routing_layer_list.size());
  for (GridMap<int32_t>& violation_map : layer_violation_map) {
    violation_map.init(gcell_map.get_x_size(), gcell_map.get_y_size());
  }
  for (int32_t x = 0; x < gcell_map.get_x_size(); x++) {
    for (int32_t y = 0; y < gcell_map.get_y_size(); y++) {
      for (Violation* violation : gcell_map[x][y].get_violation_set()) {
        layer_violation_map[violation->get_violation_shape().get_layer_idx()][x][y]++;
      }
    }
  }
  for (RoutingLayer& routing_layer : routing_layer_list) {
    std::ofstream* violation_csv_file
        = RTUtil::getOutputFileStream(RTUtil::getString(ta_temp_directory_path, "violation_map_", routing_layer.get_layer_name(), ".csv"));
    GridMap<int32_t>& violation_map = layer_violation_map[routing_layer.get_layer_idx()];
    for (int32_t y = violation_map.get_y_size() - 1; y >= 0; y--) {
      for (int32_t x = 0; x < violation_map.get_x_size(); x++) {
        RTUtil::pushStream(violation_csv_file, violation_map[x][y], ",");
      }
      RTUtil::pushStream(violation_csv_file, "\n");
    }
    RTUtil::closeFileStream(violation_csv_file);
  }
}

#endif

}  // namespace irt
