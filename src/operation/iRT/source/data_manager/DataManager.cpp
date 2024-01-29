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
#include "DataManager.hpp"

#include "RTAPI.hpp"
#include "RTU.hpp"
#include "RTUtil.hpp"
#include "file_rt.hpp"

namespace irt {

// public

void DataManager::initInst()
{
  if (_dm_instance == nullptr) {
    _dm_instance = new DataManager();
  }
}

DataManager& DataManager::getInst()
{
  if (_dm_instance == nullptr) {
    LOG_INST.error(Loc::current(), "The instance not initialized!");
  }
  return *_dm_instance;
}

void DataManager::destroyInst()
{
  if (_dm_instance != nullptr) {
    delete _dm_instance;
    _dm_instance = nullptr;
  }
}

// function

void DataManager::input(std::map<std::string, std::any>& config_map, idb::IdbBuilder* idb_builder)
{
  Monitor monitor;
  LOG_INST.info(Loc::current(), "Begin inputting...");

  wrapConfig(config_map);
  wrapDatabase(idb_builder);
  buildConfig();
  buildDatabase();
  printConfig();
  printDatabase();

  LOG_INST.info(Loc::current(), "End input!", monitor.getStatsInfo());
}

void DataManager::output(idb::IdbBuilder* idb_builder)
{
  Monitor monitor;
  LOG_INST.info(Loc::current(), "Begin outputting...");

  outputGCellGrid(idb_builder);
  outputNetList(idb_builder);

  LOG_INST.info(Loc::current(), "End output!", monitor.getStatsInfo());
}

// void DataManager::save(Stage stage)
// {
//   Monitor monitor;
//   LOG_INST.info(Loc::current(), "Begin saving...");

//   saveStageResult(stage);

//   LOG_INST.info(Loc::current(), "End save!", monitor.getStatsInfo());
// }

// void DataManager::load(Stage stage)
// {
//   Monitor monitor;
//   LOG_INST.info(Loc::current(), "Begin loading...");

//   loadStageResult(stage);

//   LOG_INST.info(Loc::current(), "End load!", monitor.getStatsInfo());
// }

#if 1  // 更新GCellMap

void DataManager::updateFixedRectToGCellMap(ChangeType change_type, irt_int net_idx, EXTLayerRect* ext_layer_rect, bool is_routing)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  for (irt_int x = ext_layer_rect->get_grid_lb_x(); x <= ext_layer_rect->get_grid_rt_x(); x++) {
    for (irt_int y = ext_layer_rect->get_grid_lb_y(); y <= ext_layer_rect->get_grid_rt_y(); y++) {
      auto& net_fixed_rect_map = gcell_map[x][y].get_type_layer_net_fixed_rect_map()[is_routing][ext_layer_rect->get_layer_idx()];
      if (change_type == ChangeType::kAdd) {
        net_fixed_rect_map[net_idx].insert(ext_layer_rect);
      } else {
        net_fixed_rect_map[net_idx].erase(ext_layer_rect);
        if (net_fixed_rect_map[net_idx].empty()) {
          net_fixed_rect_map.erase(net_idx);
        }
      }
    }
  }
}

void DataManager::updateAccessPointToGCellMap(ChangeType change_type, irt_int net_idx, AccessPoint* access_point)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  auto& net_access_point_map = gcell_map[access_point->get_grid_x()][access_point->get_grid_y()].get_net_access_point_map();
  if (change_type == ChangeType::kAdd) {
    net_access_point_map[net_idx].insert(access_point);
  } else {
    net_access_point_map[net_idx].erase(access_point);
    if (net_access_point_map[net_idx].empty()) {
      net_access_point_map.erase(net_idx);
    }
  }
}

void DataManager::updateNetResultToGCellMap(ChangeType change_type, irt_int net_idx, Segment<LayerCoord>* segment)
{
  ScaleAxis& gcell_axis = _database.get_gcell_axis();
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  PlanarRect grid_rect = RTUtil::getClosedGCellGridRect(*segment, gcell_axis);

  for (irt_int x = grid_rect.get_lb_x(); x <= grid_rect.get_rt_x(); x++) {
    for (irt_int y = grid_rect.get_lb_y(); y <= grid_rect.get_rt_y(); y++) {
      auto& net_result_map = gcell_map[x][y].get_net_result_map();
      if (change_type == ChangeType::kAdd) {
        net_result_map[net_idx].insert(segment);
      } else {
        net_result_map[net_idx].erase(segment);
        if (net_result_map[net_idx].empty()) {
          net_result_map.erase(net_idx);
        }
      }
    }
  }
}

void DataManager::updateViolationToGCellMap(ChangeType change_type, Violation* violation)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  PlanarRect& grid_rect = violation->get_violation_shape().get_grid_rect();

  for (irt_int x = grid_rect.get_lb_x(); x <= grid_rect.get_rt_x(); x++) {
    for (irt_int y = grid_rect.get_lb_y(); y <= grid_rect.get_rt_y(); y++) {
      GCell& gcell = gcell_map[x][y];
      if (change_type == ChangeType::kAdd) {
        gcell.get_violation_set().insert(violation);
      } else {
        gcell.get_violation_set().erase(violation);
      }
    }
  }
}

void DataManager::updatePatchToGCellMap(ChangeType change_type, irt_int net_idx, EXTLayerRect* ext_layer_rect)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  for (irt_int x = ext_layer_rect->get_grid_lb_x(); x <= ext_layer_rect->get_grid_rt_x(); x++) {
    for (irt_int y = ext_layer_rect->get_grid_lb_y(); y <= ext_layer_rect->get_grid_rt_y(); y++) {
      auto& net_patch_map = gcell_map[x][y].get_net_patch_map();
      if (change_type == ChangeType::kAdd) {
        net_patch_map[net_idx].insert(ext_layer_rect);
      } else {
        net_patch_map[net_idx].erase(ext_layer_rect);
        if (net_patch_map[net_idx].empty()) {
          net_patch_map.erase(net_idx);
        }
      }
    }
  }
}

std::map<bool, std::map<irt_int, std::map<irt_int, std::set<EXTLayerRect*>>>> DataManager::getTypeLayerNetFixedRectMap(
    EXTPlanarRect& region)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  std::map<bool, std::map<irt_int, std::map<irt_int, std::set<EXTLayerRect*>>>> type_layer_net_fixed_rect_map;
  for (irt_int x = region.get_grid_lb_x(); x <= region.get_grid_rt_x(); x++) {
    for (irt_int y = region.get_grid_lb_y(); y <= region.get_grid_rt_y(); y++) {
      for (auto& [is_routing, layer_net_fixed_rect_map] : gcell_map[x][y].get_type_layer_net_fixed_rect_map()) {
        for (auto& [layer_idx, net_fixed_rect_map] : layer_net_fixed_rect_map) {
          for (auto& [net_idx, fixed_rect_set] : net_fixed_rect_map) {
            type_layer_net_fixed_rect_map[is_routing][layer_idx][net_idx].insert(fixed_rect_set.begin(), fixed_rect_set.end());
          }
        }
      }
    }
  }
  return type_layer_net_fixed_rect_map;
}

std::map<irt_int, std::set<AccessPoint*>> DataManager::getNetAccessPointMap(EXTPlanarRect& region)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  std::map<irt_int, std::set<AccessPoint*>> net_access_point_map;
  for (irt_int x = region.get_grid_lb_x(); x <= region.get_grid_rt_x(); x++) {
    for (irt_int y = region.get_grid_lb_y(); y <= region.get_grid_rt_y(); y++) {
      for (auto& [net_idx, access_point_set] : gcell_map[x][y].get_net_access_point_map()) {
        net_access_point_map[net_idx].insert(access_point_set.begin(), access_point_set.end());
      }
    }
  }
  return net_access_point_map;
}

std::map<irt_int, std::set<Segment<LayerCoord>*>> DataManager::getNetResultMap(EXTPlanarRect& region)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  std::map<irt_int, std::set<Segment<LayerCoord>*>> net_result_map;
  for (irt_int x = region.get_grid_lb_x(); x <= region.get_grid_rt_x(); x++) {
    for (irt_int y = region.get_grid_lb_y(); y <= region.get_grid_rt_y(); y++) {
      for (auto& [net_idx, result_set] : gcell_map[x][y].get_net_result_map()) {
        net_result_map[net_idx].insert(result_set.begin(), result_set.end());
      }
    }
  }
  return net_result_map;
}

std::set<Violation*> DataManager::getViolationSet(EXTPlanarRect& region)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  std::set<Violation*> violation_set;
  for (irt_int x = region.get_grid_lb_x(); x <= region.get_grid_rt_x(); x++) {
    for (irt_int y = region.get_grid_lb_y(); y <= region.get_grid_rt_y(); y++) {
      violation_set.insert(gcell_map[x][y].get_violation_set().begin(), gcell_map[x][y].get_violation_set().end());
    }
  }
  return violation_set;
}

std::map<irt_int, std::set<EXTLayerRect*>> DataManager::getNetPatchMap(EXTPlanarRect& region)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();

  std::map<irt_int, std::set<EXTLayerRect*>> net_patch_map;
  for (irt_int x = region.get_grid_lb_x(); x <= region.get_grid_rt_x(); x++) {
    for (irt_int y = region.get_grid_lb_y(); y <= region.get_grid_rt_y(); y++) {
      for (auto& [net_idx, patch_set] : gcell_map[x][y].get_net_patch_map()) {
        net_patch_map[net_idx].insert(patch_set.begin(), patch_set.end());
      }
    }
  }
  return net_patch_map;
}

#endif

#if 1  // 获得NetShapeList

std::vector<NetShape> DataManager::getNetShapeList(irt_int net_idx, std::vector<Segment<LayerCoord>>& segment_list)
{
  std::vector<NetShape> drc_shape_list;
  for (Segment<LayerCoord>& segment : segment_list) {
    for (NetShape& drc_shape : getNetShapeList(net_idx, segment)) {
      drc_shape_list.push_back(drc_shape);
    }
  }
  return drc_shape_list;
}

std::vector<NetShape> DataManager::getNetShapeList(irt_int net_idx, Segment<LayerCoord>& segment)
{
  std::vector<NetShape> drc_shape_list;
  for (NetShape& drc_shape : getNetShapeList(net_idx, segment.get_first(), segment.get_second())) {
    drc_shape_list.push_back(drc_shape);
  }
  return drc_shape_list;
}

std::vector<NetShape> DataManager::getNetShapeList(irt_int net_idx, MTree<LayerCoord>& coord_tree)
{
  std::vector<NetShape> drc_shape_list;
  for (Segment<TNode<LayerCoord>*>& coord_segment : RTUtil::getSegListByTree(coord_tree)) {
    for (NetShape& drc_shape : getNetShapeList(net_idx, coord_segment.get_first()->value(), coord_segment.get_second()->value())) {
      drc_shape_list.push_back(drc_shape);
    }
  }
  return drc_shape_list;
}

std::vector<NetShape> DataManager::getNetShapeList(irt_int net_idx, LayerCoord& first_coord, LayerCoord& second_coord)
{
  std::vector<RoutingLayer>& routing_layer_list = DM_INST.getDatabase().get_routing_layer_list();
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = DM_INST.getDatabase().get_layer_via_master_list();

  std::vector<NetShape> drc_shape_list;
  irt_int first_layer_idx = first_coord.get_layer_idx();
  irt_int second_layer_idx = second_coord.get_layer_idx();
  if (first_layer_idx != second_layer_idx) {
    RTUtil::swapByASC(first_layer_idx, second_layer_idx);
    for (irt_int layer_idx = first_layer_idx; layer_idx < second_layer_idx; layer_idx++) {
      ViaMaster& via_master = layer_via_master_list[layer_idx].front();

      LayerRect& above_enclosure = via_master.get_above_enclosure();
      LayerRect offset_above_enclosure(RTUtil::getOffsetRect(above_enclosure, first_coord), above_enclosure.get_layer_idx());
      drc_shape_list.emplace_back(net_idx, offset_above_enclosure, true);

      LayerRect& below_enclosure = via_master.get_below_enclosure();
      LayerRect offset_below_enclosure(RTUtil::getOffsetRect(below_enclosure, first_coord), below_enclosure.get_layer_idx());
      drc_shape_list.emplace_back(net_idx, offset_below_enclosure, true);

      for (PlanarRect& cut_shape : via_master.get_cut_shape_list()) {
        LayerRect offset_cut_shape(RTUtil::getOffsetRect(cut_shape, first_coord), via_master.get_cut_layer_idx());
        drc_shape_list.emplace_back(net_idx, offset_cut_shape, false);
      }
    }
  } else {
    irt_int half_width = routing_layer_list[first_layer_idx].get_min_width() / 2;
    LayerRect wire_rect(RTUtil::getEnlargedRect(first_coord, second_coord, half_width), first_layer_idx);
    drc_shape_list.emplace_back(net_idx, wire_rect, true);
  }
  return drc_shape_list;
}

std::vector<NetShape> DataManager::getNetShapeList(irt_int net_idx, MTree<PhysicalNode>& physical_node_tree)
{
  std::vector<NetShape> drc_shape_list;
  for (TNode<PhysicalNode>* physical_node_node : RTUtil::getNodeList(physical_node_tree)) {
    for (NetShape& drc_shape : getNetShapeList(net_idx, physical_node_node->value())) {
      drc_shape_list.push_back(drc_shape);
    }
  }
  return drc_shape_list;
}

std::vector<NetShape> DataManager::getNetShapeList(irt_int net_idx, PhysicalNode& physical_node)
{
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = DM_INST.getDatabase().get_layer_via_master_list();

  std::vector<NetShape> drc_shape_list;
  if (physical_node.isType<WireNode>()) {
    WireNode& wire_node = physical_node.getNode<WireNode>();
    PlanarRect wire_rect = RTUtil::getEnlargedRect(wire_node.get_first(), wire_node.get_second(), wire_node.get_wire_width() / 2);
    drc_shape_list.emplace_back(net_idx, LayerRect(wire_rect, wire_node.get_layer_idx()), true);
  } else if (physical_node.isType<ViaNode>()) {
    ViaNode& via_node = physical_node.getNode<ViaNode>();
    ViaMasterIdx& via_master_idx = via_node.get_via_master_idx();
    ViaMaster& via_master = layer_via_master_list[via_master_idx.get_below_layer_idx()][via_master_idx.get_via_idx()];

    LayerRect& above_enclosure = via_master.get_above_enclosure();
    LayerRect offset_above_enclosure(RTUtil::getOffsetRect(above_enclosure, via_node), above_enclosure.get_layer_idx());
    drc_shape_list.emplace_back(net_idx, offset_above_enclosure, true);

    LayerRect& below_enclosure = via_master.get_below_enclosure();
    LayerRect offset_below_enclosure(RTUtil::getOffsetRect(below_enclosure, via_node), below_enclosure.get_layer_idx());
    drc_shape_list.emplace_back(net_idx, offset_below_enclosure, true);

    for (PlanarRect& cut_shape : via_master.get_cut_shape_list()) {
      LayerRect offset_cut_shape(RTUtil::getOffsetRect(cut_shape, via_node), via_master.get_cut_layer_idx());
      drc_shape_list.emplace_back(net_idx, offset_cut_shape, false);
    }
  } else if (physical_node.isType<PatchNode>()) {
    PatchNode& patch_node = physical_node.getNode<PatchNode>();
    drc_shape_list.emplace_back(net_idx, patch_node, true);
  }
  return drc_shape_list;
}

#endif

#if 1  // 获得IdbSegment

idb::IdbLayerShape* DataManager::getIDBLayerShapeByFixedRect(EXTLayerRect* fixed_rect, bool is_routing)
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  std::vector<CutLayer>& cut_layer_list = _database.get_cut_layer_list();
  idb::IdbLayers* idb_layer_list = _helper.get_idb_builder()->get_def_service()->get_layout()->get_layers();

  std::string layer_name;
  if (is_routing) {
    layer_name = routing_layer_list[fixed_rect->get_layer_idx()].get_layer_name();
  } else {
    layer_name = cut_layer_list[fixed_rect->get_layer_idx()].get_layer_name();
  }
  idb::IdbLayer* idb_layer = idb_layer_list->find_layer(layer_name);
  if (idb_layer == nullptr) {
    LOG_INST.error(Loc::current(), "Can not find idb layer ", layer_name);
  }
  PlanarRect& real_rect = fixed_rect->get_real_rect();

  idb::IdbLayerShape* idb_shape = new idb::IdbLayerShape();
  idb_shape->set_type_rect();
  idb_shape->add_rect(real_rect.get_lb_x(), real_rect.get_lb_y(), real_rect.get_rt_x(), real_rect.get_rt_y());
  idb_shape->set_layer(idb_layer);
  return idb_shape;
}

idb::IdbRegularWireSegment* DataManager::getIDBSegmentByNetResult(irt_int net_idx, Segment<LayerCoord>& segment)
{
  if (segment.get_first().get_layer_idx() == segment.get_second().get_layer_idx()) {
    return getIDBWire(net_idx, segment);
  } else {
    return getIDBVia(net_idx, segment);
  }
}

idb::IdbRegularWireSegment* DataManager::getIDBSegmentByNetPatch(irt_int net_idx, EXTLayerRect& ext_layer_rect)
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  idb::IdbLayers* idb_layer_list = _helper.get_idb_builder()->get_def_service()->get_layout()->get_layers();

  std::string layer_name = routing_layer_list[ext_layer_rect.get_layer_idx()].get_layer_name();
  idb::IdbLayer* idb_layer = idb_layer_list->find_layer(layer_name);
  if (idb_layer == nullptr) {
    LOG_INST.error(Loc::current(), "Can not find idb layer ", layer_name);
  }
  PlanarRect& real_rect = ext_layer_rect.get_real_rect();

  idb::IdbRegularWireSegment* idb_segment = new idb::IdbRegularWireSegment();
  idb_segment->set_layer(idb_layer);
  idb_segment->set_is_rect(true);
  idb_segment->add_point(real_rect.get_lb_x(), real_rect.get_lb_y());
  idb_segment->set_delta_rect(0, 0, real_rect.get_rt_x() - real_rect.get_lb_x(), real_rect.get_rt_y() - real_rect.get_lb_y());
  return idb_segment;
}

#endif

// private

DataManager* DataManager::_dm_instance = nullptr;

#if 1  // wrap

void DataManager::wrapConfig(std::map<std::string, std::any>& config_map)
{
  /////////////////////////////////////////////
  _config.temp_directory_path = RTUtil::getConfigValue<std::string>(config_map, "-temp_directory_path", "./rt_temp_directory");
  _config.log_level = RTUtil::getConfigValue<irt_int>(config_map, "-log_level", 0);
  _config.thread_number = RTUtil::getConfigValue<irt_int>(config_map, "-thread_number", 8);
  _config.bottom_routing_layer = RTUtil::getConfigValue<std::string>(config_map, "-bottom_routing_layer", "");
  _config.top_routing_layer = RTUtil::getConfigValue<std::string>(config_map, "-top_routing_layer", "");
  _config.ta_prefer_wire_unit = RTUtil::getConfigValue<double>(config_map, "-ta_prefer_wire_unit", 1);
  _config.ta_nonprefer_wire_unit = RTUtil::getConfigValue<double>(config_map, "-ta_nonprefer_wire_unit", 2);
  _config.ta_corner_unit = RTUtil::getConfigValue<double>(config_map, "-ta_corner_unit", 1);
  _config.ta_pin_distance_unit = RTUtil::getConfigValue<double>(config_map, "-ta_pin_distance_unit", 1);
  _config.ta_group_distance_unit = RTUtil::getConfigValue<double>(config_map, "-ta_group_distance_unit", 0.5);
  _config.ta_blockage_unit = RTUtil::getConfigValue<double>(config_map, "-ta_blockage_unit", 2048);
  _config.ta_net_shape_unit = RTUtil::getConfigValue<double>(config_map, "-ta_net_shape_unit", 128);
  _config.ta_reserved_via_unit = RTUtil::getConfigValue<double>(config_map, "-ta_reserved_via_unit", 32);
  _config.ta_history_cost_unit = RTUtil::getConfigValue<double>(config_map, "-ta_history_cost_unit", 4);
  _config.ta_model_max_iter_num = RTUtil::getConfigValue<irt_int>(config_map, "-ta_model_max_iter_num", 1);
  _config.ta_panel_max_iter_num = RTUtil::getConfigValue<irt_int>(config_map, "-ta_panel_max_iter_num", 1);
  _config.dr_prefer_wire_unit = RTUtil::getConfigValue<double>(config_map, "-dr_prefer_wire_unit", 1);
  _config.dr_nonprefer_wire_unit = RTUtil::getConfigValue<double>(config_map, "-dr_nonprefer_wire_unit", 2);
  _config.dr_via_unit = RTUtil::getConfigValue<double>(config_map, "-dr_via_unit", 1);
  _config.dr_corner_unit = RTUtil::getConfigValue<double>(config_map, "-dr_corner_unit", 1);
  /////////////////////////////////////////////
}

void DataManager::wrapDatabase(idb::IdbBuilder* idb_builder)
{
  wrapMicronDBU(idb_builder);
  wrapDie(idb_builder);
  wrapRow(idb_builder);
  wrapLayerList(idb_builder);
  wrapLayerViaMasterList(idb_builder);
  wrapBlockageList(idb_builder);
  wrapNetList(idb_builder);
  updateHelper(idb_builder);
}

void DataManager::wrapMicronDBU(idb::IdbBuilder* idb_builder)
{
  _database.set_micron_dbu(idb_builder->get_def_service()->get_design()->get_units()->get_micron_dbu());
}

void DataManager::wrapDie(idb::IdbBuilder* idb_builder)
{
  idb::IdbDie* die = idb_builder->get_lef_service()->get_layout()->get_die();

  EXTPlanarRect& die_box = _database.get_die();
  die_box.set_real_lb(die->get_llx(), die->get_lly());
  die_box.set_real_rt(die->get_urx(), die->get_ury());
}

void DataManager::wrapRow(idb::IdbBuilder* idb_builder)
{
  irt_int start_x = INT_MAX;
  irt_int start_y = INT_MAX;
  for (idb::IdbRow* idb_row : idb_builder->get_def_service()->get_layout()->get_rows()->get_row_list()) {
    start_x = std::min(start_x, idb_row->get_original_coordinate()->get_x());
    start_y = std::min(start_y, idb_row->get_original_coordinate()->get_y());
  }
  Row& row = _database.get_row();
  row.set_start_x(start_x);
  row.set_start_y(start_y);
  row.set_height(idb_builder->get_def_service()->get_layout()->get_rows()->get_row_height());
}

void DataManager::wrapLayerList(idb::IdbBuilder* idb_builder)
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  std::vector<CutLayer>& cut_layer_list = _database.get_cut_layer_list();

  std::vector<idb::IdbLayer*>& idb_layers = idb_builder->get_lef_service()->get_layout()->get_layers()->get_layers();
  for (idb::IdbLayer* idb_layer : idb_layers) {
    if (idb_layer->is_routing()) {
      idb::IdbLayerRouting* idb_routing_layer = dynamic_cast<idb::IdbLayerRouting*>(idb_layer);
      RoutingLayer routing_layer;
      routing_layer.set_layer_idx(idb_routing_layer->get_id());
      routing_layer.set_layer_order(idb_routing_layer->get_order());
      routing_layer.set_layer_name(idb_routing_layer->get_name());
      routing_layer.set_min_width(idb_routing_layer->get_min_width());
      routing_layer.set_min_area(idb_routing_layer->get_area());
      routing_layer.set_prefer_direction(getRTDirectionByDB(idb_routing_layer->get_direction()));
      wrapTrackAxis(routing_layer, idb_routing_layer);
      wrapSpacingTable(routing_layer, idb_routing_layer);
      routing_layer_list.push_back(std::move(routing_layer));
    } else if (idb_layer->is_cut()) {
      idb::IdbLayerCut* idb_cut_layer = dynamic_cast<idb::IdbLayerCut*>(idb_layer);
      CutLayer cut_layer;
      cut_layer.set_layer_idx(idb_cut_layer->get_id());
      cut_layer.set_layer_order(idb_cut_layer->get_order());
      cut_layer.set_layer_name(idb_cut_layer->get_name());
      cut_layer.set_spacing(0);
      cut_layer_list.push_back(std::move(cut_layer));
    }
  }
}

void DataManager::wrapTrackAxis(RoutingLayer& routing_layer, idb::IdbLayerRouting* idb_layer)
{
  ScaleAxis& track_axis = routing_layer.get_track_axis();

  for (idb::IdbTrackGrid* idb_track_grid : idb_layer->get_track_grid_list()) {
    idb::IdbTrack* idb_track = idb_track_grid->get_track();

    ScaleGrid track_grid;
    track_grid.set_start_line(static_cast<irt_int>(idb_track->get_start()));
    track_grid.set_step_length(static_cast<irt_int>(idb_track->get_pitch()));
    track_grid.set_step_num(static_cast<irt_int>(idb_track_grid->get_track_num()));

    if (idb_track->get_direction() == idb::IdbTrackDirection::kDirectionX) {
      track_axis.get_x_grid_list().push_back(track_grid);
    } else if (idb_track->get_direction() == idb::IdbTrackDirection::kDirectionY) {
      track_axis.get_y_grid_list().push_back(track_grid);
    }
  }
}

void DataManager::wrapSpacingTable(RoutingLayer& routing_layer, idb::IdbLayerRouting* idb_layer)
{
  std::shared_ptr<idb::IdbParallelSpacingTable> idb_spacing_table;
  if (idb_layer->get_spacing_table().get()->get_parallel().get() != nullptr && idb_layer->get_spacing_table().get()->is_parallel()) {
    idb_spacing_table = idb_layer->get_spacing_table()->get_parallel();
  } else if (idb_layer->get_spacing_list() != nullptr && !idb_layer->get_spacing_table().get()->is_parallel()) {
    idb_spacing_table = idb_layer->get_spacing_table_from_spacing_list()->get_parallel();
  } else {
    LOG_INST.error(Loc::current(), "The idb spacing table is error!");
  }

  SpacingTable& spacing_table = routing_layer.get_spacing_table();
  std::vector<irt_int>& width_list = spacing_table.get_width_list();
  std::vector<irt_int>& parallel_length_list = spacing_table.get_parallel_length_list();
  GridMap<irt_int>& width_parallel_length_map = spacing_table.get_width_parallel_length_map();

  width_list = idb_spacing_table->get_width_list();
  parallel_length_list = idb_spacing_table->get_parallel_length_list();
  width_parallel_length_map.init(width_list.size(), parallel_length_list.size());
  for (irt_int x = 0; x < width_parallel_length_map.get_x_size(); x++) {
    for (irt_int y = 0; y < width_parallel_length_map.get_y_size(); y++) {
      width_parallel_length_map[x][y] = idb_spacing_table->get_spacing_table()[x][y];
    }
  }
}

void DataManager::wrapLayerViaMasterList(idb::IdbBuilder* idb_builder)
{
  idb::IdbVias* idb_via_list_lib = idb_builder->get_lef_service()->get_layout()->get_via_list();
  if (idb_via_list_lib == nullptr) {
    LOG_INST.error(Loc::current(), "Via list in tech lef is empty!");
  }

  std::vector<std::vector<ViaMaster>>& layer_via_master_list = _database.get_layer_via_master_list();
  std::vector<idb::IdbLayer*> idb_routing_layers = idb_builder->get_lef_service()->get_layout()->get_layers()->get_routing_layers();
  layer_via_master_list.resize(idb_routing_layers.size());

  std::vector<idb::IdbVia*>& idb_via_list = idb_via_list_lib->get_via_list();
  for (size_t i = 0; i < idb_via_list.size(); i++) {
    idb::IdbVia* idb_via = idb_via_list[i];
    if (idb_via == nullptr) {
      LOG_INST.error(Loc::current(), "The via is empty!");
    }
    ViaMaster via_master;
    via_master.set_via_name(idb_via->get_name());
    idb::IdbViaMaster* idb_via_master = idb_via->get_instance();
    // top enclosure
    idb::IdbLayerShape* idb_shape_top = idb_via_master->get_top_layer_shape();
    idb::IdbLayerRouting* idb_layer_top = dynamic_cast<idb::IdbLayerRouting*>(idb_shape_top->get_layer());
    idb::IdbRect idb_box_top = idb_shape_top->get_bounding_box();
    LayerRect above_enclosure(idb_box_top.get_low_x(), idb_box_top.get_low_y(), idb_box_top.get_high_x(), idb_box_top.get_high_y(),
                              idb_layer_top->get_id());
    via_master.set_above_enclosure(above_enclosure);
    via_master.set_above_direction(getRTDirectionByDB(idb_layer_top->get_direction()));
    // bottom enclosure
    idb::IdbLayerShape* idb_shape_bottom = idb_via_master->get_bottom_layer_shape();
    idb::IdbLayerRouting* idb_layer_bottom = dynamic_cast<idb::IdbLayerRouting*>(idb_shape_bottom->get_layer());
    idb::IdbRect idb_box_bottom = idb_shape_bottom->get_bounding_box();
    LayerRect below_enclosure(idb_box_bottom.get_low_x(), idb_box_bottom.get_low_y(), idb_box_bottom.get_high_x(),
                              idb_box_bottom.get_high_y(), idb_layer_bottom->get_id());
    via_master.set_below_enclosure(below_enclosure);
    via_master.set_below_direction(getRTDirectionByDB(idb_layer_bottom->get_direction()));
    // cut shape
    idb::IdbLayerShape idb_shape_cut = idb_via->get_cut_layer_shape();
    std::vector<PlanarRect>& cut_shape_list = via_master.get_cut_shape_list();
    for (idb::IdbRect* idb_rect : idb_shape_cut.get_rect_list()) {
      PlanarRect cut_shape;
      cut_shape.set_lb(idb_rect->get_low_x(), idb_rect->get_low_y());
      cut_shape.set_rt(idb_rect->get_high_x(), idb_rect->get_high_y());
      cut_shape_list.push_back(std::move(cut_shape));
    }
    via_master.set_cut_layer_idx(idb_shape_cut.get_layer()->get_id());
    layer_via_master_list.front().push_back(std::move(via_master));
  }
}

void DataManager::wrapBlockageList(idb::IdbBuilder* idb_builder)
{
  wrapArtificialBlockage(idb_builder);
  wrapInstanceBlockage(idb_builder);
  wrapSpecialNetBlockage(idb_builder);
}

void DataManager::wrapArtificialBlockage(idb::IdbBuilder* idb_builder)
{
  // Artificial
  idb::IdbBlockageList* idb_blockage_list = idb_builder->get_def_service()->get_design()->get_blockage_list();
  if (!idb_blockage_list->get_blockage_list().empty()) {
    LOG_INST.warn(Loc::current(), "The artificial blockage will be ignored!");
  }
}

void DataManager::wrapInstanceBlockage(idb::IdbBuilder* idb_builder)
{
  std::vector<Blockage>& routing_blockage_list = _database.get_routing_blockage_list();
  std::vector<Blockage>& cut_blockage_list = _database.get_cut_blockage_list();

  // instance
  std::vector<idb::IdbInstance*> instance_list = idb_builder->get_def_service()->get_design()->get_instance_list()->get_instance_list();
  std::vector<idb::IdbLayerShape*> layer_shape_list;
  for (idb::IdbInstance* instance : instance_list) {
    // instance obs
    std::vector<idb::IdbLayerShape*>& obs_box_list = instance->get_obs_box_list();
    layer_shape_list.insert(layer_shape_list.end(), obs_box_list.begin(), obs_box_list.end());
    // instance pin without net
    for (idb::IdbPin* idb_pin : instance->get_pin_list()->get_pin_list()) {
      if (idb_pin->get_net() != nullptr) {
        continue;
      }
      std::vector<idb::IdbLayerShape*>& port_box_list = idb_pin->get_port_box_list();
      layer_shape_list.insert(layer_shape_list.end(), port_box_list.begin(), port_box_list.end());
    }
  }
  for (idb::IdbLayerShape* layer_shape : layer_shape_list) {
    for (idb::IdbRect* rect : layer_shape->get_rect_list()) {
      Blockage blockage;
      blockage.set_real_lb(rect->get_low_x(), rect->get_low_y());
      blockage.set_real_rt(rect->get_high_x(), rect->get_high_y());
      blockage.set_layer_idx(layer_shape->get_layer()->get_id());
      if (blockage.get_real_rect().getArea() < DBL_ERROR) {
        continue;
      }
      if (layer_shape->get_layer()->is_routing()) {
        routing_blockage_list.push_back(std::move(blockage));
      } else if (layer_shape->get_layer()->is_cut()) {
        cut_blockage_list.push_back(std::move(blockage));
      }
    }
  }
}

void DataManager::wrapSpecialNetBlockage(idb::IdbBuilder* idb_builder)
{
  std::vector<Blockage>& routing_blockage_list = _database.get_routing_blockage_list();
  std::vector<Blockage>& cut_blockage_list = _database.get_cut_blockage_list();

  // special net
  idb::IdbSpecialNetList* idb_special_net_list = idb_builder->get_def_service()->get_design()->get_special_net_list();
  for (idb::IdbSpecialNet* idb_net : idb_special_net_list->get_net_list()) {
    for (idb::IdbSpecialWire* idb_wire : idb_net->get_wire_list()->get_wire_list()) {
      for (idb::IdbSpecialWireSegment* idb_segment : idb_wire->get_segment_list()) {
        if (idb_segment->is_via()) {
          std::vector<idb::IdbLayerShape> layer_shape_list;
          layer_shape_list.push_back(idb_segment->get_via()->get_top_layer_shape());
          layer_shape_list.push_back(idb_segment->get_via()->get_bottom_layer_shape());
          layer_shape_list.push_back(idb_segment->get_via()->get_cut_layer_shape());

          for (idb::IdbLayerShape& layer_shape : layer_shape_list) {
            for (idb::IdbRect* rect : layer_shape.get_rect_list()) {
              Blockage blockage;
              blockage.set_real_lb(rect->get_low_x(), rect->get_low_y());
              blockage.set_real_rt(rect->get_high_x(), rect->get_high_y());
              blockage.set_layer_idx(layer_shape.get_layer()->get_id());
              if (blockage.get_real_rect().getArea() < DBL_ERROR) {
                continue;
              }
              if (layer_shape.get_layer()->is_routing()) {
                routing_blockage_list.push_back(std::move(blockage));
              } else if (layer_shape.get_layer()->is_cut()) {
                cut_blockage_list.push_back(std::move(blockage));
              }
            }
          }
        } else {
          idb::IdbRect* idb_rect = idb_segment->get_bounding_box();
          // wire
          Blockage blockage;
          blockage.set_real_lb(idb_rect->get_low_x(), idb_rect->get_low_y());
          blockage.set_real_rt(idb_rect->get_high_x(), idb_rect->get_high_y());
          blockage.set_layer_idx(idb_segment->get_layer()->get_id());
          if (blockage.get_real_rect().getArea() < DBL_ERROR) {
            continue;
          }
          routing_blockage_list.push_back(std::move(blockage));
        }
      }
    }
  }
}

void DataManager::wrapNetList(idb::IdbBuilder* idb_builder)
{
  std::vector<Net>& net_list = _database.get_net_list();
  std::vector<idb::IdbNet*> idb_net_list = idb_builder->get_def_service()->get_design()->get_net_list()->get_net_list();

  // bound setting
  for (idb::IdbNet* idb_net : idb_net_list) {
    if (!checkSkipping(idb_net)) {
      Net net;
      net.set_net_name(idb_net->get_net_name());
      net.set_connect_type(getRTConnectTypeByDB(idb_net->get_connect_type()));
      wrapPinList(net, idb_net);
      processEmptyShapePin(net);
      wrapDrivingPin(net, idb_net);
      net_list.push_back(std::move(net));
    }
  }
}

bool DataManager::checkSkipping(idb::IdbNet* idb_net)
{
  std::string net_name = idb_net->get_net_name();

  // check pin number
  size_t pin_num = idb_net->get_instance_pin_list()->get_pin_num();
  if (pin_num <= 1) {
    LOG_INST.info(Loc::current(), "The net '", net_name, "' has ", pin_num, " pin! skipping...");
    return true;
  } else if (pin_num >= 500) {
    LOG_INST.warn(Loc::current(), "The ultra large net: ", net_name, " has ", pin_num, " pins!");
    sleep(2);
  }

  // check the connection form io_cell PAD to io_pin
  bool has_io_pin = false;
  if (idb_net->get_io_pin() != nullptr) {
    has_io_pin = true;
  }
  bool has_io_cell = false;
  for (idb::IdbInstance* instance : idb_net->get_instance_list()->get_instance_list()) {
    if (instance->get_cell_master()->is_pad()) {
      has_io_cell = true;
      break;
    }
  }
  if (has_io_pin && has_io_cell) {
    LOG_INST.info(Loc::current(), "The net '", net_name, "' connects PAD and io_pin! skipping...");
    return true;
  }

  // check connect_type
  if (idb_net->get_connect_type() == idb::IdbConnectType::kNone) {
    idb_net->set_connect_type(idb::IdbConnectType::kSignal);
  }
  if (idb_net->get_connect_type() == idb::IdbConnectType::kNone) {
    LOG_INST.warn(Loc::current(), "The connect type of net '", net_name, "' is none!");
  }

  return false;
}

void DataManager::wrapPinList(Net& net, idb::IdbNet* idb_net)
{
  std::vector<Pin>& pin_list = net.get_pin_list();

  // pin list in instance
  for (idb::IdbPin* idb_pin : idb_net->get_instance_pin_list()->get_pin_list()) {
    /// without term description in some cases
    if (idb_pin->get_term()->get_port_number() <= 0) {
      continue;
    }
    Pin pin;
    pin.set_pin_name(RTUtil::getString(idb_pin->get_instance()->get_name(), ":", idb_pin->get_pin_name()));
    wrapPinShapeList(pin, idb_pin);
    pin_list.push_back(std::move(pin));
  }
  // io pin list
  if (idb_net->get_io_pin() != nullptr) {
    idb::IdbPin* io_pin = idb_net->get_io_pin();
    Pin pin;
    pin.set_pin_name(io_pin->get_pin_name());
    wrapPinShapeList(pin, io_pin);
    pin_list.push_back(std::move(pin));
  }
}

void DataManager::wrapPinShapeList(Pin& pin, idb::IdbPin* idb_pin)
{
  std::vector<EXTLayerRect>& routing_shape_list = pin.get_routing_shape_list();
  std::vector<EXTLayerRect>& cut_shape_list = pin.get_cut_shape_list();

  for (idb::IdbLayerShape* layer_shape : idb_pin->get_port_box_list()) {
    for (idb::IdbRect* rect : layer_shape->get_rect_list()) {
      EXTLayerRect pin_shape;
      pin_shape.set_real_lb(rect->get_low_x(), rect->get_low_y());
      pin_shape.set_real_rt(rect->get_high_x(), rect->get_high_y());
      pin_shape.set_layer_idx(layer_shape->get_layer()->get_id());
      if (layer_shape->get_layer()->is_routing()) {
        routing_shape_list.push_back(std::move(pin_shape));
      } else if (layer_shape->get_layer()->is_cut()) {
        cut_shape_list.push_back(std::move(pin_shape));
      }
    }
  }
}

void DataManager::processEmptyShapePin(Net& net)
{
  std::vector<Pin>& pin_list = net.get_pin_list();

  std::vector<irt_int> empty_pin_idx_list;
  for (size_t i = 0; i < pin_list.size(); i++) {
    Pin& pin = pin_list[i];
    if (pin.get_routing_shape_list().empty()) {
      empty_pin_idx_list.push_back(i);
    }
  }

  irt_int legal_pin_idx = -1;
  for (size_t i = 0; i < pin_list.size(); i++) {
    Pin& pin = pin_list[i];
    if (!pin.get_routing_shape_list().empty()) {
      legal_pin_idx = i;
      break;
    }
  }

  if (legal_pin_idx == -1) {
    LOG_INST.error(Loc::current(), "There is no legal pin for net ", net.get_net_name());
  }

  for (size_t i = 0; i < empty_pin_idx_list.size(); i++) {
    pin_list[empty_pin_idx_list[i]].set_routing_shape_list(pin_list[legal_pin_idx].get_routing_shape_list());
  }
}

void DataManager::wrapDrivingPin(Net& net, idb::IdbNet* idb_net)
{
  idb::IdbPin* idb_driving_pin = idb_net->get_driving_pin();
  if (idb_driving_pin == nullptr) {
    LOG_INST.warn(Loc::current(), "The net '", net.get_net_name(), "' without driving pin!");
    idb_driving_pin = idb_net->get_instance_pin_list()->get_pin_list().front();
  }
  std::string driving_pin_name = idb_driving_pin->get_pin_name();
  if (!idb_driving_pin->is_io_pin()) {
    driving_pin_name = RTUtil::getString(idb_driving_pin->get_instance()->get_name(), ":", driving_pin_name);
  }
  for (Pin& pin : net.get_pin_list()) {
    if (pin.get_pin_name() == driving_pin_name) {
      pin.set_is_driving(true);
    }
  }
}

void DataManager::updateHelper(idb::IdbBuilder* idb_builder)
{
  _helper.set_idb_builder(idb_builder);
  _helper.set_design_name(idb_builder->get_def_service()->get_design()->get_design_name());
  _helper.set_lef_file_path_list(idb_builder->get_lef_service()->get_lef_files());
  _helper.set_def_file_path(idb_builder->get_def_service()->get_def_file());

  std::map<irt_int, irt_int>& routing_idb_layer_id_to_idx_map = _helper.get_routing_idb_layer_id_to_idx_map();
  std::map<irt_int, irt_int>& cut_idb_layer_id_to_idx_map = _helper.get_cut_idb_layer_id_to_idx_map();
  std::map<std::string, irt_int>& routing_layer_name_to_idx_map = _helper.get_routing_layer_name_to_idx_map();
  std::map<std::string, irt_int>& cut_layer_name_to_idx_map = _helper.get_cut_layer_name_to_idx_map();

  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  for (size_t i = 0; i < routing_layer_list.size(); i++) {
    routing_idb_layer_id_to_idx_map[routing_layer_list[i].get_layer_idx()] = static_cast<irt_int>(i);
    routing_layer_name_to_idx_map[routing_layer_list[i].get_layer_name()] = static_cast<irt_int>(i);
  }
  std::vector<CutLayer>& cut_layer_list = _database.get_cut_layer_list();
  for (size_t i = 0; i < cut_layer_list.size(); i++) {
    cut_idb_layer_id_to_idx_map[cut_layer_list[i].get_layer_idx()] = static_cast<irt_int>(i);
    cut_layer_name_to_idx_map[cut_layer_list[i].get_layer_name()] = static_cast<irt_int>(i);
  }
}

Direction DataManager::getRTDirectionByDB(idb::IdbLayerDirection idb_direction)
{
  if (idb_direction == idb::IdbLayerDirection::kHorizontal) {
    return Direction::kHorizontal;
  } else if (idb_direction == idb::IdbLayerDirection::kVertical) {
    return Direction::kVertical;
  } else {
    return Direction::kOblique;
  }
}

ConnectType DataManager::getRTConnectTypeByDB(idb::IdbConnectType idb_connect_type)
{
  ConnectType connect_type;
  switch (idb_connect_type) {
    case idb::IdbConnectType::kSignal:
      connect_type = ConnectType::kSignal;
      break;
    case idb::IdbConnectType::kPower:
      connect_type = ConnectType::kPower;
      break;
    case idb::IdbConnectType::kGround:
      connect_type = ConnectType::kGround;
      break;
    case idb::IdbConnectType::kClock:
      connect_type = ConnectType::kClock;
      break;
    case idb::IdbConnectType::kAnalog:
      connect_type = ConnectType::kAnalog;
      break;
    case idb::IdbConnectType::kReset:
      connect_type = ConnectType::kReset;
      break;
    case idb::IdbConnectType::kScan:
      connect_type = ConnectType::kScan;
      break;
    case idb::IdbConnectType::kTieOff:
      connect_type = ConnectType::kTieoff;
      break;
    default:
      connect_type = ConnectType::kNone;
      break;
  }
  return connect_type;
}

#endif

#if 1  // build

void DataManager::buildConfig()
{
  /////////////////////////////////////////////
  // **********        RT         ********** //
  _config.temp_directory_path = std::filesystem::absolute(_config.temp_directory_path);
  _config.temp_directory_path += "/";
  _config.log_file_path = _config.temp_directory_path + "rt.log";
  if (_config.bottom_routing_layer.empty()) {
    _config.bottom_routing_layer = _database.get_routing_layer_list().front().get_layer_name();
  }
  if (_config.top_routing_layer.empty()) {
    _config.top_routing_layer = _database.get_routing_layer_list().back().get_layer_name();
  }
  _config.bottom_routing_layer_idx = _helper.getRoutingLayerIdxByName(_config.bottom_routing_layer);
  _config.top_routing_layer_idx = _helper.getRoutingLayerIdxByName(_config.top_routing_layer);
  if (_config.bottom_routing_layer_idx >= _config.top_routing_layer_idx) {
    LOG_INST.error(Loc::current(), "The routing layer should be at least two layers!");
  }
  // **********    DataManager    ********** //
  _config.dm_temp_directory_path = _config.temp_directory_path + "data_manager/";
  // **********  DetailedRouter   ********** //
  _config.dr_temp_directory_path = _config.temp_directory_path + "detailed_router/";
  // **********    GDSPlotter     ********** //
  _config.gp_temp_directory_path = _config.temp_directory_path + "gds_plotter/";
  // **********   GlobalRouter    ********** //
  _config.gr_temp_directory_path = _config.temp_directory_path + "global_router/";
    // **********   InitialRouter    ********** //
  _config.ir_temp_directory_path = _config.temp_directory_path + "initial_router/";
  // **********   PinAccessor     ********** //
  _config.pa_temp_directory_path = _config.temp_directory_path + "pin_accessor/";
  // ********     SupplyAnalyzer    ******** //
  _config.sa_temp_directory_path = _config.temp_directory_path + "supply_analyzer/";
  // **********   TrackAssigner   ********** //
  _config.ta_temp_directory_path = _config.temp_directory_path + "track_assigner/";
  /////////////////////////////////////////////
  // **********        RT         ********** //
  RTUtil::createDir(_config.temp_directory_path);
  RTUtil::createDirByFile(_config.log_file_path);
  // **********    DataManager    ********** //
  RTUtil::createDir(_config.dm_temp_directory_path);
  // **********  DetailedRouter   ********** //
  RTUtil::createDir(_config.dr_temp_directory_path);
  // **********    GDSPlotter     ********** //
  RTUtil::createDir(_config.gp_temp_directory_path);
  // **********   GlobalRouter    ********** //
  RTUtil::createDir(_config.gr_temp_directory_path);
    // **********   InitialRouter    ********** //
  RTUtil::createDir(_config.ir_temp_directory_path);
  // **********   PinAccessor     ********** //
  RTUtil::createDir(_config.pa_temp_directory_path);
  // **********   SupplyAnalyzer     ********** //
  RTUtil::createDir(_config.sa_temp_directory_path);
  // **********   TrackAssigner   ********** //
  RTUtil::createDir(_config.ta_temp_directory_path);
  /////////////////////////////////////////////
}

void DataManager::buildDatabase()
{
  buildGCellAxis();
  buildDie();
  buildLayerList();
  buildLayerViaMasterList();
  buildBlockageList();
  buildNetList();
  buildGCellMap();
  updateHelper();
}

void DataManager::buildGCellAxis()
{
  makeGCellAxis();
  checkGCellAxis();
}

void DataManager::makeGCellAxis()
{
  ScaleAxis& gcell_axis = _database.get_gcell_axis();

  irt_int recommended_pitch = getRecommendedPitch();
  gcell_axis.set_x_grid_list(makeGCellGridList(Direction::kVertical, recommended_pitch));
  gcell_axis.set_y_grid_list(makeGCellGridList(Direction::kHorizontal, recommended_pitch));
}

irt_int DataManager::getRecommendedPitch()
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();

  std::map<irt_int, irt_int> pitch_count_map;
  for (RoutingLayer& routing_layer : routing_layer_list) {
    for (ScaleGrid& track_grid : routing_layer.getPreferTrackGridList()) {
      pitch_count_map[track_grid.get_step_length()]++;
    }
  }
  irt_int recommended_pitch = -1;
  irt_int max_count = INT32_MIN;
  for (auto [pitch, count] : pitch_count_map) {
    if (count > max_count) {
      max_count = count;
      recommended_pitch = pitch;
    }
  }
  if (max_count == 1) {
    irt_int min_pitch = INT_MAX;
    for (auto [pitch, count] : pitch_count_map) {
      min_pitch = std::min(min_pitch, pitch);
    }
    recommended_pitch = min_pitch;
  }
  if (recommended_pitch == -1) {
    LOG_INST.error(Loc::current(), "The recommended_pitch is -1!");
  }
  return recommended_pitch;
}

std::vector<ScaleGrid> DataManager::makeGCellGridList(Direction direction, irt_int recommended_pitch)
{
  Die& die = _database.get_die();
  Row& row = _database.get_row();

  irt_int die_start_scale = (direction == Direction::kVertical ? die.get_real_lb_x() : die.get_real_lb_y());
  irt_int die_end_scale = (direction == Direction::kVertical ? die.get_real_rt_x() : die.get_real_rt_y());
  irt_int row_mid_scale = (direction == Direction::kVertical ? row.get_start_x() : row.get_start_y());
  // 为了防止与track重合，减去一个recommended_pitch的一半
  row_mid_scale -= (recommended_pitch / 2);
  irt_int step_length = row.get_height();

  std::vector<irt_int> gcell_scale_list;
  gcell_scale_list.push_back(die_start_scale);
  for (irt_int gcell_scale = row_mid_scale; gcell_scale >= die_start_scale; gcell_scale -= step_length) {
    gcell_scale_list.push_back(gcell_scale);
  }
  for (irt_int gcell_scale = row_mid_scale; gcell_scale <= die_end_scale; gcell_scale += step_length) {
    gcell_scale_list.push_back(gcell_scale);
  }
  gcell_scale_list.push_back(die_end_scale);

  std::sort(gcell_scale_list.begin(), gcell_scale_list.end());
  // 删除小于step_length的
  for (irt_int i = 2; i < static_cast<irt_int>(gcell_scale_list.size()); i++) {
    if (std::abs(gcell_scale_list[i - 2] - gcell_scale_list[i - 1]) < step_length
        || std::abs(gcell_scale_list[i - 1] - gcell_scale_list[i]) < step_length) {
      gcell_scale_list[i - 1] = gcell_scale_list[i - 2];
    }
  }
  gcell_scale_list.erase(std::unique(gcell_scale_list.begin(), gcell_scale_list.end()), gcell_scale_list.end());
  return makeGCellGridList(gcell_scale_list);
}

std::vector<ScaleGrid> DataManager::makeGCellGridList(std::vector<irt_int>& gcell_scale_list)
{
  std::vector<ScaleGrid> gcell_grid_list;

  for (size_t i = 1; i < gcell_scale_list.size(); i++) {
    irt_int pre_scale = gcell_scale_list[i - 1];
    irt_int curr_scale = gcell_scale_list[i];

    ScaleGrid gcell_grid;
    gcell_grid.set_start_line(pre_scale);
    gcell_grid.set_step_length(curr_scale - pre_scale);
    gcell_grid.set_step_num(1);
    gcell_grid.set_end_line(curr_scale);
    gcell_grid_list.push_back(gcell_grid);
  }
  // merge
  RTUtil::merge(gcell_grid_list, [](ScaleGrid& sentry, ScaleGrid& soldier) {
    if (sentry.get_step_length() != soldier.get_step_length()) {
      return false;
    }
    sentry.set_start_line(std::min(sentry.get_start_line(), soldier.get_start_line()));
    sentry.set_step_num(sentry.get_step_num() + 1);
    sentry.set_end_line(std::max(sentry.get_end_line(), soldier.get_end_line()));
    return true;
  });

  return gcell_grid_list;
}

void DataManager::checkGCellAxis()
{
  ScaleAxis& gcell_axis = _database.get_gcell_axis();
  std::vector<ScaleGrid>& x_grid_list = gcell_axis.get_x_grid_list();
  std::vector<ScaleGrid>& y_grid_list = gcell_axis.get_y_grid_list();

  if (x_grid_list.empty() || y_grid_list.empty()) {
    LOG_INST.error(Loc::current(), "The gcell grid list is empty!");
  }
  for (size_t i = 0; i < x_grid_list.size(); i++) {
    if (x_grid_list[i].get_step_length() <= 0) {
      LOG_INST.error(Loc::current(), "The step length of x grid '", x_grid_list[i].get_step_length(), "' is wrong!");
    }
  }
  for (size_t i = 0; i < y_grid_list.size(); i++) {
    if (y_grid_list[i].get_step_length() <= 0) {
      LOG_INST.error(Loc::current(), "The step length of y grid '", y_grid_list[i].get_step_length(), "' is wrong!");
    }
  }
  for (size_t i = 1; i < x_grid_list.size(); i++) {
    if (x_grid_list[i - 1].get_end_line() < x_grid_list[i].get_start_line()) {
      LOG_INST.error(Loc::current(), "The x grid with gap '", x_grid_list[i - 1].get_end_line(), " < ", x_grid_list[i].get_start_line(),
                     "'!");
    } else if (x_grid_list[i - 1].get_end_line() > x_grid_list[i].get_start_line()) {
      LOG_INST.error(Loc::current(), "The x grid with overlapping '", x_grid_list[i - 1].get_end_line(), " < ",
                     x_grid_list[i].get_start_line(), "'!");
    }
  }
  for (size_t i = 1; i < y_grid_list.size(); i++) {
    if (y_grid_list[i - 1].get_end_line() < y_grid_list[i].get_start_line()) {
      LOG_INST.error(Loc::current(), "The y grid with gap '", y_grid_list[i - 1].get_end_line(), " < ", y_grid_list[i].get_start_line(),
                     "'!");
    } else if (y_grid_list[i - 1].get_end_line() > y_grid_list[i].get_start_line()) {
      LOG_INST.error(Loc::current(), "The y grid with overlapping '", y_grid_list[i - 1].get_end_line(), " > ",
                     y_grid_list[i].get_start_line(), "'!");
    }
  }
}

void DataManager::buildDie()
{
  makeDie();
  checkDie();
}

void DataManager::makeDie()
{
  Die& die = _database.get_die();
  ScaleAxis& gcell_axis = _database.get_gcell_axis();
  die.set_grid_rect(RTUtil::getOpenGCellGridRect(die.get_real_rect(), gcell_axis));
}

void DataManager::checkDie()
{
  Die& die = _database.get_die();

  if (die.get_real_lb_x() < 0 || die.get_real_lb_y() < 0 || die.get_real_rt_x() < 0 || die.get_real_rt_y() < 0) {
    LOG_INST.error(Loc::current(), "The die '(", die.get_real_lb_x(), " , ", die.get_real_lb_y(), ") - (", die.get_real_rt_x(), " , ",
                   die.get_real_rt_y(), ")' is wrong!");
  }
  if ((die.get_real_rt_x() <= die.get_real_lb_x()) || (die.get_real_rt_y() <= die.get_real_lb_y())) {
    LOG_INST.error(Loc::current(), "The die '(", die.get_real_lb_x(), " , ", die.get_real_lb_y(), ") - (", die.get_real_rt_x(), " , ",
                   die.get_real_rt_y(), ")' is wrong!");
  }
}

void DataManager::buildLayerList()
{
  transLayerList();
  makeLayerList();
  checkLayerList();
}

void DataManager::transLayerList()
{
  for (RoutingLayer& routing_layer : _database.get_routing_layer_list()) {
    routing_layer.set_layer_idx(_helper.getRoutingLayerIdxByIDBLayerId(routing_layer.get_layer_idx()));
  }
  for (CutLayer& cut_layer_list : _database.get_cut_layer_list()) {
    cut_layer_list.set_layer_idx(_helper.getCutLayerIdxByIDBLayerId(cut_layer_list.get_layer_idx()));
  }
}

void DataManager::makeLayerList()
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();

  for (RoutingLayer& routing_layer : routing_layer_list) {
    for (ScaleGrid& x_track_grid : routing_layer.getXTrackGridList()) {
      x_track_grid.set_end_line(x_track_grid.get_start_line() + x_track_grid.get_step_length() * x_track_grid.get_step_num());
    }
    for (ScaleGrid& y_track_grid : routing_layer.getYTrackGridList()) {
      y_track_grid.set_end_line(y_track_grid.get_start_line() + y_track_grid.get_step_length() * y_track_grid.get_step_num());
    }
  }
}

void DataManager::checkLayerList()
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  std::vector<CutLayer>& cut_layer_list = _database.get_cut_layer_list();

  if (routing_layer_list.empty()) {
    LOG_INST.error(Loc::current(), "The routing_layer_list is empty!");
  }
  if (cut_layer_list.empty()) {
    LOG_INST.error(Loc::current(), "The cut_layer_list is empty!");
  }

  for (RoutingLayer& routing_layer : routing_layer_list) {
    std::string& layer_name = routing_layer.get_layer_name();
    if (routing_layer.get_prefer_direction() == Direction::kNone) {
      LOG_INST.error(Loc::current(), "The layer '", layer_name, "' prefer_direction is none!");
    }
    for (ScaleGrid& x_track_grid : routing_layer.getXTrackGridList()) {
      if (x_track_grid.get_step_length() <= 0) {
        LOG_INST.error(Loc::current(), "The layer '", layer_name, "' x_track_grid step length '", x_track_grid.get_step_length(),
                       "' is wrong!");
      }
    }
    for (ScaleGrid& y_track_grid : routing_layer.getYTrackGridList()) {
      if (y_track_grid.get_step_length() <= 0) {
        LOG_INST.error(Loc::current(), "The layer '", layer_name, "' y_track_grid step length '", y_track_grid.get_step_length(),
                       "' is wrong!");
      }
    }
    SpacingTable& spacing_table = routing_layer.get_spacing_table();
    if (spacing_table.get_width_list().empty()) {
      LOG_INST.error(Loc::current(), "The layer '", layer_name, "' spacing width list is empty!");
    }
    for (irt_int width : spacing_table.get_width_list()) {
      if (width < 0) {
        LOG_INST.error(Loc::current(), "The layer '", layer_name, "' width < 0!");
      }
    }
    for (irt_int parallel_length : spacing_table.get_parallel_length_list()) {
      if (parallel_length < 0) {
        LOG_INST.error(Loc::current(), "The layer '", layer_name, "' parallel_length < 0!");
      }
    }
    GridMap<irt_int>& width_parallel_length_map = spacing_table.get_width_parallel_length_map();
    for (irt_int width_idx = 0; width_idx < width_parallel_length_map.get_x_size(); width_idx++) {
      for (irt_int parallel_length_idx = 0; parallel_length_idx < width_parallel_length_map.get_y_size(); parallel_length_idx++) {
        if (width_parallel_length_map[width_idx][parallel_length_idx] < 0) {
          LOG_INST.error(Loc::current(), "The layer '", layer_name, "' spacing < 0!");
        }
      }
    }
  }
}

void DataManager::buildLayerViaMasterList()
{
  transLayerViaMasterList();
  makeLayerViaMasterList();
}

void DataManager::transLayerViaMasterList()
{
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = _database.get_layer_via_master_list();

  for (std::vector<ViaMaster>& via_master_list : layer_via_master_list) {
    for (ViaMaster& via_master : via_master_list) {
      // above
      LayerRect& above_enclosure = via_master.get_above_enclosure();
      above_enclosure.set_layer_idx(_helper.getRoutingLayerIdxByIDBLayerId(above_enclosure.get_layer_idx()));
      // below
      LayerRect& below_enclosure = via_master.get_below_enclosure();
      below_enclosure.set_layer_idx(_helper.getRoutingLayerIdxByIDBLayerId(below_enclosure.get_layer_idx()));
      // cut
      via_master.set_cut_layer_idx(_helper.getCutLayerIdxByIDBLayerId(via_master.get_cut_layer_idx()));
    }
  }
}

void DataManager::makeLayerViaMasterList()
{
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = _database.get_layer_via_master_list();
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();

  std::vector<ViaMaster> first_via_master_list;
  for (ViaMaster& via_master : layer_via_master_list.front()) {
    irt_int below_layer_idx = via_master.get_below_enclosure().get_layer_idx();
    if (below_layer_idx == 0) {
      first_via_master_list.push_back(via_master);
    } else {
      layer_via_master_list[below_layer_idx].push_back(via_master);
    }
  }
  layer_via_master_list[0] = first_via_master_list;

  for (size_t layer_idx = 0; layer_idx < layer_via_master_list.size(); layer_idx++) {
    std::vector<ViaMaster>& via_master_list = layer_via_master_list[layer_idx];
    for (ViaMaster& via_master : via_master_list) {
      // above
      LayerRect& above_enclosure = via_master.get_above_enclosure();
      Direction above_layer_direction = routing_layer_list[above_enclosure.get_layer_idx()].get_prefer_direction();
      via_master.set_above_direction(above_enclosure.getRectDirection(above_layer_direction));
      // below
      LayerRect& below_enclosure = via_master.get_below_enclosure();
      Direction below_layer_direction = routing_layer_list[below_enclosure.get_layer_idx()].get_prefer_direction();
      via_master.set_below_direction(below_enclosure.getRectDirection(below_layer_direction));
    }
    std::sort(via_master_list.begin(), via_master_list.end(),
              [&](ViaMaster& via_master1, ViaMaster& via_master2) { return sortByMultiLevel(via_master1, via_master2); });
    for (size_t i = 0; i < via_master_list.size(); i++) {
      via_master_list[i].set_via_master_idx(layer_idx, i);
    }
  }
}

bool DataManager::sortByMultiLevel(ViaMaster& via_master1, ViaMaster& via_master2)
{
  SortStatus sort_status = SortStatus::kNone;

  sort_status = sortByLayerDirectionPriority(via_master1, via_master2);
  if (sort_status == SortStatus::kTrue) {
    return true;
  } else if (sort_status == SortStatus::kFalse) {
    return false;
  }
  sort_status = sortByWidthASC(via_master1, via_master2);
  if (sort_status == SortStatus::kTrue) {
    return true;
  } else if (sort_status == SortStatus::kFalse) {
    return false;
  }
  sort_status = sortByLengthASC(via_master1, via_master2);
  if (sort_status == SortStatus::kTrue) {
    return true;
  } else if (sort_status == SortStatus::kFalse) {
    return false;
  }
  sort_status = sortBySymmetryPriority(via_master1, via_master2);
  if (sort_status == SortStatus::kTrue) {
    return true;
  } else if (sort_status == SortStatus::kFalse) {
    return false;
  }
  return false;
}

// 层方向优先
SortStatus DataManager::sortByLayerDirectionPriority(ViaMaster& via_master1, ViaMaster& via_master2)
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();

  Direction above_layer_direction = routing_layer_list[via_master1.get_above_enclosure().get_layer_idx()].get_prefer_direction();
  Direction below_layer_direction = routing_layer_list[via_master1.get_below_enclosure().get_layer_idx()].get_prefer_direction();

  if (via_master1.get_above_direction() == above_layer_direction && via_master2.get_above_direction() != above_layer_direction) {
    return SortStatus::kTrue;
  } else if (via_master1.get_above_direction() != above_layer_direction && via_master2.get_above_direction() == above_layer_direction) {
    return SortStatus::kFalse;
  } else {
    if (via_master1.get_below_direction() == below_layer_direction && via_master2.get_below_direction() != below_layer_direction) {
      return SortStatus::kTrue;
    } else if (via_master1.get_below_direction() != below_layer_direction && via_master2.get_below_direction() == below_layer_direction) {
      return SortStatus::kFalse;
    } else {
      return SortStatus::kEqual;
    }
  }
}

// 宽度升序
SortStatus DataManager::sortByWidthASC(ViaMaster& via_master1, ViaMaster& via_master2)
{
  LayerRect& via_master1_above = via_master1.get_above_enclosure();
  LayerRect& via_master1_below = via_master1.get_below_enclosure();
  LayerRect& via_master2_above = via_master2.get_above_enclosure();
  LayerRect& via_master2_below = via_master2.get_below_enclosure();

  if (via_master1_above.getWidth() < via_master2_above.getWidth()) {
    return SortStatus::kTrue;
  } else if (via_master1_above.getWidth() > via_master2_above.getWidth()) {
    return SortStatus::kFalse;
  } else {
    if (via_master1_below.getWidth() < via_master2_below.getWidth()) {
      return SortStatus::kTrue;
    } else if (via_master1_below.getWidth() > via_master2_below.getWidth()) {
      return SortStatus::kFalse;
    } else {
      return SortStatus::kEqual;
    }
  }
}

// 长度升序
SortStatus DataManager::sortByLengthASC(ViaMaster& via_master1, ViaMaster& via_master2)
{
  LayerRect& via_master1_above = via_master1.get_above_enclosure();
  LayerRect& via_master1_below = via_master1.get_below_enclosure();
  LayerRect& via_master2_above = via_master2.get_above_enclosure();
  LayerRect& via_master2_below = via_master2.get_below_enclosure();

  if (via_master1_above.getLength() < via_master2_above.getLength()) {
    return SortStatus::kTrue;
  } else if (via_master1_above.getLength() > via_master2_above.getLength()) {
    return SortStatus::kFalse;
  } else {
    if (via_master1_below.getLength() < via_master2_below.getLength()) {
      return SortStatus::kTrue;
    } else if (via_master1_below.getLength() > via_master2_below.getLength()) {
      return SortStatus::kFalse;
    } else {
      return SortStatus::kEqual;
    }
  }
}

// 对称优先
SortStatus DataManager::sortBySymmetryPriority(ViaMaster& via_master1, ViaMaster& via_master2)
{
  LayerRect& via_master1_above = via_master1.get_above_enclosure();
  LayerRect& via_master1_below = via_master1.get_below_enclosure();
  LayerRect& via_master2_above = via_master2.get_above_enclosure();
  LayerRect& via_master2_below = via_master2.get_below_enclosure();

  // via_master的lb为负数，rt为正数
  irt_int via_master1_above_center_diff = std::abs(via_master1_above.get_lb_x() + via_master1_above.get_rt_x());
  irt_int via_master2_above_center_diff = std::abs(via_master2_above.get_lb_x() + via_master2_above.get_rt_x());
  irt_int via_master1_below_center_diff = std::abs(via_master1_below.get_lb_x() + via_master1_below.get_rt_x());
  irt_int via_master2_below_center_diff = std::abs(via_master2_below.get_lb_x() + via_master2_below.get_rt_x());
  if (via_master1_above_center_diff < via_master2_above_center_diff) {
    return SortStatus::kTrue;
  } else if (via_master1_above_center_diff > via_master2_above_center_diff) {
    return SortStatus::kFalse;
  } else {
    if (via_master1_below_center_diff < via_master2_below_center_diff) {
      return SortStatus::kTrue;
    } else if (via_master1_below_center_diff > via_master2_below_center_diff) {
      return SortStatus::kFalse;
    } else {
      return SortStatus::kEqual;
    }
  }
}

void DataManager::buildBlockageList()
{
  transBlockageList();
  makeBlockageList();
  checkBlockageList();
}

void DataManager::transBlockageList()
{
  std::vector<Blockage>& routing_blockage_list = _database.get_routing_blockage_list();
  std::vector<Blockage>& cut_blockage_list = _database.get_cut_blockage_list();

  for (Blockage& blockage : routing_blockage_list) {
    blockage.set_layer_idx(_helper.getRoutingLayerIdxByIDBLayerId(blockage.get_layer_idx()));
  }
  for (Blockage& blockage : cut_blockage_list) {
    blockage.set_layer_idx(_helper.getCutLayerIdxByIDBLayerId(blockage.get_layer_idx()));
  }
}

void DataManager::makeBlockageList()
{
  ScaleAxis& gcell_axis = _database.get_gcell_axis();
  Die& die = _database.get_die();
  std::vector<Blockage>& routing_blockage_list = _database.get_routing_blockage_list();
  std::vector<Blockage>& cut_blockage_list = _database.get_cut_blockage_list();

  for (Blockage& routing_blockage : routing_blockage_list) {
    routing_blockage.set_real_rect(RTUtil::getRegularRect(routing_blockage.get_real_rect(), die.get_real_rect()));
    routing_blockage.set_grid_rect(RTUtil::getClosedGCellGridRect(routing_blockage.get_real_rect(), gcell_axis));
  }
  for (Blockage& cut_blockage : cut_blockage_list) {
    cut_blockage.set_real_rect(RTUtil::getRegularRect(cut_blockage.get_real_rect(), die.get_real_rect()));
    cut_blockage.set_grid_rect(RTUtil::getClosedGCellGridRect(cut_blockage.get_real_rect(), gcell_axis));
  }
}

void DataManager::checkBlockageList()
{
  Die& die = _database.get_die();
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  std::vector<Blockage>& routing_blockage_list = _database.get_routing_blockage_list();
  std::vector<Blockage>& cut_blockage_list = _database.get_cut_blockage_list();

  for (Blockage& blockage : routing_blockage_list) {
    if (blockage.get_real_lb_x() < die.get_real_lb_x() || blockage.get_real_lb_y() < die.get_real_lb_y()
        || die.get_real_rt_x() < blockage.get_real_rt_x() || die.get_real_rt_y() < blockage.get_real_rt_y()) {
      // log
      LOG_INST.error(Loc::current(), "The blockage '(", blockage.get_real_lb_x(), " , ", blockage.get_real_lb_y(), ") - (",
                     blockage.get_real_rt_x(), " , ", blockage.get_real_rt_y(), ") ",
                     routing_layer_list[blockage.get_layer_idx()].get_layer_name(), "' is wrong! Die '(", die.get_real_lb_x(), " , ",
                     die.get_real_lb_y(), ") - (", die.get_real_rt_x(), " , ", die.get_real_rt_y(), ")'");
    }
  }
  for (Blockage& blockage : cut_blockage_list) {
    if (blockage.get_real_lb_x() < die.get_real_lb_x() || blockage.get_real_lb_y() < die.get_real_lb_y()
        || die.get_real_rt_x() < blockage.get_real_rt_x() || die.get_real_rt_y() < blockage.get_real_rt_y()) {
      // log
      LOG_INST.error(Loc::current(), "The blockage '(", blockage.get_real_lb_x(), " , ", blockage.get_real_lb_y(), ") - (",
                     blockage.get_real_rt_x(), " , ", blockage.get_real_rt_y(), ") ",
                     routing_layer_list[blockage.get_layer_idx()].get_layer_name(), "' is wrong! Die '(", die.get_real_lb_x(), " , ",
                     die.get_real_lb_y(), ") - (", die.get_real_rt_x(), " , ", die.get_real_rt_y(), ")'");
    }
  }
}

void DataManager::buildNetList()
{
  std::vector<Net>& net_list = _database.get_net_list();

  for (size_t net_idx = 0; net_idx < net_list.size(); net_idx++) {
    Net& net = net_list[net_idx];
    net.set_net_idx(static_cast<irt_int>(net_idx));
    buildPinList(net);
  }
}

void DataManager::buildPinList(Net& net)
{
  transPinList(net);
  makePinList(net);
  checkPinList(net);
}

void DataManager::transPinList(Net& net)
{
  for (Pin& pin : net.get_pin_list()) {
    for (EXTLayerRect& routing_shape : pin.get_routing_shape_list()) {
      routing_shape.set_layer_idx(_helper.getRoutingLayerIdxByIDBLayerId(routing_shape.get_layer_idx()));
    }
    for (EXTLayerRect& cut_shape : pin.get_cut_shape_list()) {
      cut_shape.set_layer_idx(_helper.getCutLayerIdxByIDBLayerId(cut_shape.get_layer_idx()));
    }
  }
}

void DataManager::makePinList(Net& net)
{
  Die& die = _database.get_die();
  ScaleAxis& gcell_axis = _database.get_gcell_axis();
  std::vector<Pin>& pin_list = net.get_pin_list();

  for (size_t pin_idx = 0; pin_idx < pin_list.size(); pin_idx++) {
    Pin& pin = pin_list[pin_idx];
    pin.set_pin_idx(static_cast<irt_int>(pin_idx));
    for (EXTLayerRect& routing_shape : pin.get_routing_shape_list()) {
      routing_shape.set_real_rect(RTUtil::getRegularRect(routing_shape.get_real_rect(), die.get_real_rect()));
      routing_shape.set_grid_rect(RTUtil::getClosedGCellGridRect(routing_shape.get_real_rect(), gcell_axis));
    }
    for (EXTLayerRect& cut_shape : pin.get_cut_shape_list()) {
      cut_shape.set_real_rect(RTUtil::getRegularRect(cut_shape.get_real_rect(), die.get_real_rect()));
      cut_shape.set_grid_rect(RTUtil::getClosedGCellGridRect(cut_shape.get_real_rect(), gcell_axis));
    }
  }
}

void DataManager::checkPinList(Net& net)
{
  Die& die = _database.get_die();
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  std::vector<CutLayer>& cut_layer_list = _database.get_cut_layer_list();

  for (Pin& pin : net.get_pin_list()) {
    for (EXTLayerRect& routing_shape : pin.get_routing_shape_list()) {
      if (routing_shape.get_real_lb_x() < die.get_real_lb_x() || routing_shape.get_real_lb_y() < die.get_real_lb_y()
          || die.get_real_rt_x() < routing_shape.get_real_rt_x() || die.get_real_rt_y() < routing_shape.get_real_rt_y()) {
        LOG_INST.error(Loc::current(), "The pin_shape '(", routing_shape.get_real_lb_x(), " , ", routing_shape.get_real_lb_y(), ") - (",
                       routing_shape.get_real_rt_x(), " , ", routing_shape.get_real_rt_y(), ") ",
                       routing_layer_list[routing_shape.get_layer_idx()].get_layer_name(), "' is wrong! Die '(", die.get_real_lb_x(), " , ",
                       die.get_real_lb_y(), ") - (", die.get_real_rt_x(), " , ", die.get_real_rt_y(), ")'");
      }
    }
    for (EXTLayerRect& cut_shape : pin.get_cut_shape_list()) {
      if (cut_shape.get_real_lb_x() < die.get_real_lb_x() || cut_shape.get_real_lb_y() < die.get_real_lb_y()
          || die.get_real_rt_x() < cut_shape.get_real_rt_x() || die.get_real_rt_y() < cut_shape.get_real_rt_y()) {
        LOG_INST.error(Loc::current(), "The pin_shape '(", cut_shape.get_real_lb_x(), " , ", cut_shape.get_real_lb_y(), ") - (",
                       cut_shape.get_real_rt_x(), " , ", cut_shape.get_real_rt_y(), ") ",
                       cut_layer_list[cut_shape.get_layer_idx()].get_layer_name(), "' is wrong! Die '(", die.get_real_lb_x(), " , ",
                       die.get_real_lb_y(), ") - (", die.get_real_rt_x(), " , ", die.get_real_rt_y(), ")'");
      }
    }
  }
}

void DataManager::buildGCellMap()
{
  Die& die = _database.get_die();
  std::vector<Blockage>& routing_blockage_list = _database.get_routing_blockage_list();
  std::vector<Blockage>& cut_blockage_list = _database.get_cut_blockage_list();
  std::vector<Net>& net_list = _database.get_net_list();

  GridMap<GCell>& gcell_map = _database.get_gcell_map();
  gcell_map.init(die.getXSize(), die.getYSize());

  for (Blockage& routing_blockage : routing_blockage_list) {
    updateFixedRectToGCellMap(ChangeType::kAdd, -1, &routing_blockage, true);
  }
  for (Blockage& cut_blockage : cut_blockage_list) {
    updateFixedRectToGCellMap(ChangeType::kAdd, -1, &cut_blockage, false);
  }
  for (Net& net : net_list) {
    for (Pin& pin : net.get_pin_list()) {
      for (EXTLayerRect& routing_shape : pin.get_routing_shape_list()) {
        updateFixedRectToGCellMap(ChangeType::kAdd, net.get_net_idx(), &routing_shape, true);
      }
      for (EXTLayerRect& cut_shape : pin.get_cut_shape_list()) {
        updateFixedRectToGCellMap(ChangeType::kAdd, net.get_net_idx(), &cut_shape, false);
      }
    }
  }
}

void DataManager::updateHelper()
{
  std::map<std::string, ViaMasterIdx>& via_name_to_idx_map = _helper.get_via_name_to_idx_map();

  for (std::vector<ViaMaster>& via_master_list : _database.get_layer_via_master_list()) {
    for (ViaMaster& via_master : via_master_list) {
      via_name_to_idx_map[via_master.get_via_name()] = via_master.get_via_master_idx();
    }
  }

  std::map<irt_int, std::vector<irt_int>>& cut_to_adjacent_routing_map = _helper.get_cut_to_adjacent_routing_map();

  std::map<irt_int, std::pair<bool, irt_int>> layer_order_to_idx_map;
  for (RoutingLayer& routing_layer : _database.get_routing_layer_list()) {
    layer_order_to_idx_map[routing_layer.get_layer_order()] = std::make_pair(false, routing_layer.get_layer_idx());
  }
  for (CutLayer& cut_layer : _database.get_cut_layer_list()) {
    layer_order_to_idx_map[cut_layer.get_layer_order()] = std::make_pair(true, cut_layer.get_layer_idx());
  }
  std::vector<std::pair<bool, irt_int>> layer_idx_pair_list;
  for (auto& [layer_order, type_layer_idx_pair] : layer_order_to_idx_map) {
    layer_idx_pair_list.push_back(type_layer_idx_pair);
  }

  for (irt_int i = 0; i < static_cast<irt_int>(layer_idx_pair_list.size()); i++) {
    bool is_cut = layer_idx_pair_list[i].first;
    irt_int cut_layer_idx = layer_idx_pair_list[i].second;
    if (!is_cut) {
      continue;
    }
    std::vector<irt_int> adj_routing_layer_idx_list;
    if (i - 1 >= 0 && layer_idx_pair_list[i - 1].first == false) {
      adj_routing_layer_idx_list.push_back(layer_idx_pair_list[i - 1].second);
    }
    if (i + 1 < static_cast<irt_int>(layer_idx_pair_list.size()) && layer_idx_pair_list[i + 1].first == false) {
      adj_routing_layer_idx_list.push_back(layer_idx_pair_list[i + 1].second);
    }
    if (!adj_routing_layer_idx_list.empty()) {
      cut_to_adjacent_routing_map[cut_layer_idx] = adj_routing_layer_idx_list;
    }
  }
}

#endif

#if 1  // print

void DataManager::printConfig()
{
  omp_set_num_threads(std::max(_config.thread_number, 1));
  LOG_INST.setLogLevel(_config.log_level);
  LOG_INST.openLogFileStream(_config.log_file_path);
  /////////////////////////////////////////////
  // **********        RT         ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(0), "RT_CONFIG_INPUT");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.temp_directory_path);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "log_level");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.log_level);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "thread_number");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.thread_number);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "bottom_routing_layer");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.bottom_routing_layer);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "top_routing_layer");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.top_routing_layer);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_prefer_wire_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_prefer_wire_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_nonprefer_wire_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_nonprefer_wire_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_corner_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_corner_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_pin_distance_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_pin_distance_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_group_distance_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_group_distance_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_blockage_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_blockage_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_net_shape_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_net_shape_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_reserved_via_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_reserved_via_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_history_cost_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_history_cost_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_model_max_iter_num");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_model_max_iter_num);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "ta_panel_max_iter_num");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.ta_panel_max_iter_num);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "dr_prefer_wire_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.dr_prefer_wire_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "dr_nonprefer_wire_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.dr_nonprefer_wire_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "dr_via_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.dr_via_unit);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "dr_corner_unit");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.dr_corner_unit);
  // **********        RT         ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(0), "RT_CONFIG_BUILD");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "log_file_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.log_file_path);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "bottom_routing_layer_idx");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.bottom_routing_layer_idx);
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "top_routing_layer_idx");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _config.top_routing_layer_idx);
  // **********    DataManager    ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "DataManager");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "dm_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.dm_temp_directory_path);
  // **********  DetailedRouter   ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "DetailedRouter");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "dr_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.dr_temp_directory_path);
  // **********    GDSPlotter     ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "GDSPlotter");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "gp_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.gp_temp_directory_path);
  // **********   GlobalRouter    ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "GlobalRouter");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "gr_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.gr_temp_directory_path);
  // **********   InitialRouter    ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "InitialRouter");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "ir_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.ir_temp_directory_path);
  // **********   PinAccessor     ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "PinAccessor");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "pa_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.pa_temp_directory_path);
  // **********   SupplyAnalyzer   ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "SupplyAnalyzer");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "sa_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.sa_temp_directory_path);
  // **********   TrackAssigner   ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "TrackAssigner");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "ta_temp_directory_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), _config.ta_temp_directory_path);
  /////////////////////////////////////////////
  sleep(2);
}

void DataManager::printDatabase()
{
  ////////////////////////////////////////////////
  // ********** RT ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(0), "RT_DATABASE");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "design_name");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _helper.get_design_name());
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "lef_file_path_list");
  for (std::string& lef_file_path : _helper.get_lef_file_path_list()) {
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), lef_file_path);
  }
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "def_file_path");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _helper.get_def_file_path());
  // ********** MicronDBU ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "micron_dbu");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _database.get_micron_dbu());
  // ********** GCellAxis ********** //
  ScaleAxis& gcell_axis = _database.get_gcell_axis();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "gcell_axis");
  std::vector<ScaleGrid>& x_grid_list = gcell_axis.get_x_grid_list();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "x_grid_list");
  for (ScaleGrid& x_grid : x_grid_list) {
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), "start:", x_grid.get_start_line(), " step_length:", x_grid.get_step_length(),
                  " step_num:", x_grid.get_step_num(), " end:", x_grid.get_end_line());
  }
  std::vector<ScaleGrid>& y_grid_list = gcell_axis.get_y_grid_list();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "y_grid_list");
  for (ScaleGrid& y_grid : y_grid_list) {
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), "start:", y_grid.get_start_line(), " step_length:", y_grid.get_step_length(),
                  " step_num:", y_grid.get_step_num(), " end:", y_grid.get_end_line());
  }
  // ********** Die ********** //
  Die& die = _database.get_die();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "die");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "(", die.get_real_lb_x(), ",", die.get_real_lb_y(), ")-(", die.get_real_rt_x(),
                ",", die.get_real_rt_y(), ")");
  // ********** RoutingLayer ********** //
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "routing_layer_num");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), routing_layer_list.size());
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "routing_layer");
  for (RoutingLayer& routing_layer : routing_layer_list) {
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "idx:", routing_layer.get_layer_idx(),
                  " order:", routing_layer.get_layer_order(), " name:", routing_layer.get_layer_name(),
                  " min_width:", routing_layer.get_min_width(), " min_area:", routing_layer.get_min_area(),
                  " prefer_direction:", GetDirectionName()(routing_layer.get_prefer_direction()));

    ScaleAxis& track_axis = routing_layer.get_track_axis();
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "track_axis");
    std::vector<ScaleGrid>& x_grid_list = track_axis.get_x_grid_list();
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), "x_grid_list");
    for (ScaleGrid& x_grid : x_grid_list) {
      LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(4), "start:", x_grid.get_start_line(),
                    " step_length:", x_grid.get_step_length(), " step_num:", x_grid.get_step_num(), " end:", x_grid.get_end_line());
    }
    std::vector<ScaleGrid>& y_grid_list = track_axis.get_y_grid_list();
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(3), "y_grid_list");
    for (ScaleGrid& y_grid : y_grid_list) {
      LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(4), "start:", y_grid.get_start_line(),
                    " step_length:", y_grid.get_step_length(), " step_num:", y_grid.get_step_num(), " end:", y_grid.get_end_line());
    }
  }
  // ********** CutLayer ********** //
  std::vector<CutLayer>& cut_layer_list = _database.get_cut_layer_list();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "cut_layer_num");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), cut_layer_list.size());
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "cut_layer");
  for (CutLayer& cut_layer : cut_layer_list) {
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), "idx:", cut_layer.get_layer_idx(), " order:", cut_layer.get_layer_order(),
                  " name:", cut_layer.get_layer_name());
  }
  // ********** ViaMaster ********** //
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = _database.get_layer_via_master_list();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "layer_via_master_list");
  for (size_t below_layer_idx = 0; below_layer_idx < layer_via_master_list.size(); below_layer_idx++) {
    std::string via_master_name_string = (routing_layer_list[below_layer_idx].get_layer_name() + ": ");
    for (ViaMaster& via_master : layer_via_master_list[below_layer_idx]) {
      via_master_name_string += (via_master.get_via_name() + " ");
    }
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), via_master_name_string);
  }
  // ********** Blockage ********** //
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "routing_blockage_num");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _database.get_routing_blockage_list().size());
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "cut_blockage_num");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), _database.get_cut_blockage_list().size());
  // ********** Net ********** //
  std::vector<Net>& net_list = _database.get_net_list();
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "net_num");
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), net_list.size());
  LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(1), "pin_num_ratio");

  size_t pin_num_upper_limit = 100;
  std::map<size_t, size_t> pin_net_map;
  for (Net& net : net_list) {
    pin_net_map[std::min(net.get_pin_list().size(), pin_num_upper_limit)]++;
  }
  for (auto [pin_num, net_num] : pin_net_map) {
    std::string head_info = "net with ";
    if (pin_num == pin_num_upper_limit) {
      head_info += ">=";
    }
    LOG_INST.info(Loc::current(), RTUtil::getSpaceByTabNum(2), head_info, pin_num, " pins: ", net_num, "(",
                  RTUtil::getPercentage(net_num, net_list.size()), "%)");
  }
  // ******************** //
  sleep(2);
  ////////////////////////////////////////////////
}

#endif

#if 1  // output

void DataManager::outputGCellGrid(idb::IdbBuilder* idb_builder)
{
  ScaleAxis& gcell_axis = _database.get_gcell_axis();

  idb::IdbGCellGridList* idb_gcell_grid_list = idb_builder->get_lef_service()->get_layout()->get_gcell_grid_list();
  idb_gcell_grid_list->clear();

  for (idb::IdbTrackDirection idb_track_direction : {idb::IdbTrackDirection::kDirectionX, idb::IdbTrackDirection::kDirectionY}) {
    std::vector<ScaleGrid> gcell_grid_list;
    if (idb_track_direction == idb::IdbTrackDirection::kDirectionX) {
      gcell_grid_list = gcell_axis.get_x_grid_list();
    } else {
      gcell_grid_list = gcell_axis.get_y_grid_list();
    }
    for (ScaleGrid& gcell_grid : gcell_grid_list) {
      idb::IdbGCellGrid* idb_gcell_grid = new idb::IdbGCellGrid();
      idb_gcell_grid->set_start(gcell_grid.get_start_line());
      idb_gcell_grid->set_space(gcell_grid.get_step_length());
      idb_gcell_grid->set_num(gcell_grid.get_step_num() + 1);
      idb_gcell_grid->set_direction(idb_track_direction);
      idb_gcell_grid_list->add_gcell_grid(idb_gcell_grid);
    }
  }
}

void DataManager::outputNetList(idb::IdbBuilder* idb_builder)
{
  GridMap<GCell>& gcell_map = _database.get_gcell_map();
  std::vector<Net>& net_list = _database.get_net_list();

  std::map<irt_int, std::vector<idb::IdbRegularWireSegment*>> net_idb_segment_map;
  for (irt_int x = 0; x < gcell_map.get_x_size(); x++) {
    for (irt_int y = 0; y < gcell_map.get_y_size(); y++) {
      for (auto& [net_idx, segment_set] : gcell_map[x][y].get_net_result_map()) {
        for (Segment<LayerCoord>* segment : segment_set) {
          net_idb_segment_map[net_idx].push_back(getIDBSegmentByNetResult(net_idx, *segment));
        }
      }
      for (auto& [net_idx, patch_set] : gcell_map[x][y].get_net_patch_map()) {
        for (EXTLayerRect* patch : patch_set) {
          net_idb_segment_map[net_idx].push_back(getIDBSegmentByNetPatch(net_idx, *patch));
        }
      }
    }
  }

  idb::IdbNetList* idb_net_list = idb_builder->get_def_service()->get_design()->get_net_list();
  if (idb_net_list == nullptr) {
    LOG_INST.error(Loc::current(), "The idb net list is empty!");
  }
  for (auto& [net_idx, idb_segment_list] : net_idb_segment_map) {
    std::string net_name = net_list[net_idx].get_net_name();
    idb::IdbNet* idb_net = idb_net_list->find_net(net_name);
    if (idb_net == nullptr) {
      LOG_INST.info(Loc::current(), "The idb net named ", net_name, " cannot be found!");
      continue;
    }
    idb_net->clear_wire_list();
    idb::IdbRegularWireList* idb_wire_list = idb_net->get_wire_list();
    if (idb_wire_list == nullptr) {
      LOG_INST.error(Loc::current(), "The idb wire list is empty!");
    }
    idb::IdbRegularWire* idb_wire = idb_wire_list->add_wire();
    idb_wire->set_wire_state(idb::IdbWiringStatement::kRouted);

    irt_int print_new = false;
    for (idb::IdbRegularWireSegment* idb_segment : idb_segment_list) {
      idb_wire->add_segment(idb_segment);
      if (print_new == false) {
        idb_segment->set_layer_as_new();
        print_new = true;
      }
    }
  }
}

#endif

#if 0  // save & load

void DataManager::saveStageResult(Stage stage)
{
  Monitor monitor;
  std::string current_stage = GetStageName()(stage);
  std::string data_path = _config.dm_temp_directory_path + GetStageName()(stage) + ".dat";
  iplf::RtPersister ps(data_path);
  ps.saveWithHeader(getHeadInfo(current_stage), _database.get_net_list());
  LOG_INST.info(Loc::current(), "The ", current_stage, " result has been saved in '", data_path, "'!", monitor.getStatsInfo());
}

std::tuple<std::string, std::string, std::set<std::string>, std::string> DataManager::getHeadInfo(const std::string& stage)
{
  std::string design_name = _helper.get_design_name();
  std::vector<std::string>& lef_file_path_list = _helper.get_lef_file_path_list();
  std::set<std::string> lef_list{lef_file_path_list.begin(), lef_file_path_list.end()};
  std::string def_name = RTUtil::getFileName(_helper.get_def_file_path());

  return make_tuple(stage, design_name, lef_list, def_name);
}

void DataManager::loadStageResult(Stage stage)
{
  Monitor monitor;

  std::string current_stage = GetStageName()(stage);
  std::string data_path = _config.dm_temp_directory_path + GetStageName()(stage) + ".dat";
  iplf::RtPersister ps(data_path);
  auto header = ps.loadHeader<decltype(getHeadInfo(current_stage))>();
  // check header

  ps.loadWithHeader(getHeadInfo(current_stage), _database.get_net_list());
  LOG_INST.info(Loc::current(), "The ", current_stage, " result has been loaded from '", data_path, "'!", monitor.getStatsInfo());
}

#endif

idb::IdbRegularWireSegment* DataManager::getIDBWire(irt_int net_idx, Segment<LayerCoord>& segment)
{
  std::vector<RoutingLayer>& routing_layer_list = _database.get_routing_layer_list();
  idb::IdbLayers* idb_layer_list = _helper.get_idb_builder()->get_def_service()->get_layout()->get_layers();

  LayerCoord& first_coord = segment.get_first();
  LayerCoord& second_coord = segment.get_second();
  irt_int layer_idx = first_coord.get_layer_idx();

  if (RTUtil::isOblique(first_coord, second_coord)) {
    LOG_INST.error(Loc::current(), "The wire is oblique!");
  }
  std::string layer_name = routing_layer_list[layer_idx].get_layer_name();
  idb::IdbLayer* idb_layer = idb_layer_list->find_layer(layer_name);
  if (idb_layer == nullptr) {
    LOG_INST.error(Loc::current(), "Can not find idb layer ", layer_name);
  }
  idb::IdbRegularWireSegment* idb_segment = new idb::IdbRegularWireSegment();
  idb_segment->set_layer(idb_layer);
  idb_segment->add_point(first_coord.get_x(), first_coord.get_y());
  idb_segment->add_point(second_coord.get_x(), second_coord.get_y());
  return idb_segment;
}

idb::IdbRegularWireSegment* DataManager::getIDBVia(irt_int net_idx, Segment<LayerCoord>& segment)
{
  std::vector<std::vector<ViaMaster>>& layer_via_master_list = _database.get_layer_via_master_list();
  idb::IdbVias* lef_via_list = _helper.get_idb_builder()->get_lef_service()->get_layout()->get_via_list();
  idb::IdbVias* def_via_list = _helper.get_idb_builder()->get_def_service()->get_design()->get_via_list();

  LayerCoord& first_coord = segment.get_first();
  LayerCoord& second_coord = segment.get_second();
  irt_int below_layer_idx = std::min(first_coord.get_layer_idx(), second_coord.get_layer_idx());

  if (below_layer_idx < 0 || below_layer_idx >= static_cast<irt_int>(layer_via_master_list.size())) {
    LOG_INST.error(Loc::current(), "The via below_layer_idx is illegal!");
  }
  std::string via_name = layer_via_master_list[below_layer_idx].front().get_via_name();
  idb::IdbVia* idb_via = lef_via_list->find_via(via_name);
  if (idb_via == nullptr) {
    idb_via = def_via_list->find_via(via_name);
  }
  if (idb_via == nullptr) {
    LOG_INST.error(Loc::current(), "Can not find idb via ", via_name, "!");
  }
  idb::IdbLayer* idb_layer_top = idb_via->get_instance()->get_top_layer_shape()->get_layer();
  if (idb_layer_top == nullptr) {
    LOG_INST.error(Loc::current(), "Can not find layer from idb via ", via_name, "!");
  }
  idb::IdbRegularWireSegment* idb_segment = new idb::IdbRegularWireSegment();
  idb_segment->set_layer(idb_layer_top);
  idb_segment->set_is_via(true);
  idb_segment->add_point(first_coord.get_x(), first_coord.get_y());
  idb::IdbVia* idb_via_new = idb_segment->copy_via(idb_via);
  idb_via_new->set_coordinate(first_coord.get_x(), first_coord.get_y());
  return idb_segment;
}

}  // namespace irt
