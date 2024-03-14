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

#include "idrc_api.h"

#include "idm.h"
#include "idrc.h"
#include "idrc_config.h"
#include "idrc_data.h"
#include "idrc_dm.h"
#include "tech_rules.h"

namespace idrc {

void DrcApi::init(std::string config)
{
  DrcConfigInst->init(config);

  /// tech rule is a singleton pattern, so it must be inited if drc starts
  DrcTechRuleInst->init();
}

void DrcApi::exit()
{
  DrcTechRuleInst->destroyInst();
}

std::map<ViolationEnumType, std::vector<DrcViolation*>> DrcApi::check(std::vector<idb::IdbLayerShape*>& env_shape_list,
                                                                      std::map<int, std::vector<idb::IdbLayerShape*>>& pin_data,
                                                                      std::map<int, std::vector<idb::IdbRegularWireSegment*>>& routing_data)
{
  DrcManager drc_manager;

  auto* data_manager = drc_manager.get_data_manager();
  // auto* rule_manager = drc_manager.get_rule_manager();
  auto* violation_manager = drc_manager.get_violation_manager();
  if (data_manager == nullptr /*|| rule_manager == nullptr */ || violation_manager == nullptr) {
    return {};
  }

  /// set drc rule stratagy by rt paramter
  /// tbd
  // rule_manager->get_stratagy()->set_stratagy_type(DrcStratagyType::kCheckFast);

  data_manager->set_env_shapes(&env_shape_list);
  data_manager->set_pin_data(&pin_data);
  data_manager->set_routing_data(&routing_data);

  auto check_type = (env_shape_list.size() + pin_data.size() + routing_data.size()) > 0 ? DrcCheckerType::kRT : DrcCheckerType::kDef;

#ifdef DEBUG_IDRC_API
  if (check_type == DrcCheckerType::kDef) {
    std::cout << "idrc : check def" << std::endl;
  }
#endif

#ifdef DEBUG_IDRC_API
  ieda::Stats stats_engine;
#endif
  drc_manager.dataInit(check_type);
#ifdef DEBUG_IDRC_API
  if (check_type == DrcCheckerType::kDef) {
    std::cout << "idrc : engine start"
              << " runtime = " << stats_engine.elapsedRunTime() << " memory = " << stats_engine.memoryDelta() << std::endl;
  }
#endif

#ifdef DEBUG_IDRC_API
  ieda::Stats stats_build_condition;
#endif
  drc_manager.dataOperate();
#ifdef DEBUG_IDRC_API
  if (check_type == DrcCheckerType::kDef) {
    std::cout << "idrc : build condition"
              << " runtime = " << stats_build_condition.elapsedRunTime() << " memory = " << stats_build_condition.memoryDelta()
              << std::endl;
  }
#endif

#ifdef DEBUG_IDRC_API
  ieda::Stats stats_check;
#endif
  drc_manager.dataCheck();

#ifdef DEBUG_IDRC_API
  if (check_type == DrcCheckerType::kDef) {
    std::cout << "idrc : check"
              << " runtime = " << stats_check.elapsedRunTime() << " memory = " << stats_check.memoryDelta() << std::endl;
  }
#endif

  return violation_manager->get_violation_map();
}
/**
 * check DRC violation for DEF file
 * initialize data from idb
 */
std::map<ViolationEnumType, std::vector<DrcViolation*>> DrcApi::checkDef()
{
  std::vector<idb::IdbLayerShape*> env_shape_list;
  std::map<int, std::vector<idb::IdbLayerShape*>> pin_data;
  std::map<int, std::vector<idb::IdbRegularWireSegment*>> routing_data;
  return check(env_shape_list, pin_data, routing_data);
}

void plotGDS(std::string gds_file,
             std::map<int32_t, std::map<ViolationEnumType, std::vector<ieda_solver::GeometryRect>>>& layer_type_rect_list_map)
{
  std::ofstream gds_stream(gds_file);
  if (gds_stream.is_open()) {
    gds_stream << "HEADER 600" << std::endl;
    gds_stream << "BGNLIB" << std::endl;
    gds_stream << "LIBNAME GDSLib" << std::endl;
    gds_stream << "UNITS 0.001 1e-9" << std::endl;
    gds_stream << "BGNSTR" << std::endl;
    gds_stream << "STRNAME top" << std::endl;

    for (auto& [layer_idx, type_rect_list_map] : layer_type_rect_list_map) {
      for (auto& [type, rect_list] : type_rect_list_map) {
        for (auto& rect : rect_list) {
          int32_t lb_x = ieda_solver::lowLeftX(rect);
          int32_t rt_x = ieda_solver::upRightX(rect);
          int32_t lb_y = ieda_solver::lowLeftY(rect);
          int32_t rt_y = ieda_solver::upRightY(rect);

          gds_stream << "BOUNDARY" << std::endl;
          gds_stream << "LAYER " << layer_idx << std::endl;
          gds_stream << "DATATYPE " << int32_t(type) << std::endl;
          gds_stream << "XY" << std::endl;
          gds_stream << lb_x << " : " << lb_y << std::endl;
          gds_stream << rt_x << " : " << lb_y << std::endl;
          gds_stream << rt_x << " : " << rt_y << std::endl;
          gds_stream << lb_x << " : " << rt_y << std::endl;
          gds_stream << lb_x << " : " << lb_y << std::endl;
          gds_stream << "ENDEL" << std::endl;
        }
      }
    }
    gds_stream << "ENDSTR" << std::endl;
    gds_stream << "ENDLIB" << std::endl;
    gds_stream.close();
    std::cout << "[Info] Result has been written to '" << gds_file << "'!" << std::endl;
  } else {
    std::cout << "[Error] Failed to open gds file '" << gds_file << "'!" << std::endl;
  }
}

void DrcApi::diagnosis(std::string third_json_file, std::string idrc_json_file, std::string output_dir)
{
  std::map<int32_t, std::map<ViolationEnumType, std::vector<ieda_solver::GeometryRect>>> third_layer_type_rect_list_map;
  {
    std::ifstream third_json_stream(third_json_file);
    json third_json;
    third_json_stream >> third_json;

    // third_layer_type_rect_list_map
  }
  std::map<int32_t, std::map<ViolationEnumType, std::vector<ieda_solver::GeometryRect>>> idrc_layer_type_rect_list_map;
  {
    std::ifstream idrc_json_stream(idrc_json_file);
    json idrc_json;
    idrc_json_stream >> idrc_json;

    // idrc_layer_type_rect_list_map
  }

  std::map<int32_t, std::map<ViolationEnumType, std::vector<ieda_solver::GeometryRect>>> third_diff_idrc_layer_type_rect_list_map;
  std::map<int32_t, std::map<ViolationEnumType, std::vector<ieda_solver::GeometryRect>>> idrc_diff_third_layer_type_rect_list_map;
  {
    auto create_set = [](std::map<int32_t, std::map<ViolationEnumType, std::vector<ieda_solver::GeometryRect>>>& input,
                         std::map<int32_t, std::map<ViolationEnumType, ieda_solver::GeometryPolygonSet>>& result) {
      for (auto& [layer, violation_map] : input) {
        for (auto& [type, violation_list] : violation_map) {
          for (auto& rect : violation_list) {
            result[layer][type] += rect;
          }
        }
      }
    };

    auto diff_two_map = [](std::map<int32_t, std::map<ViolationEnumType, ieda_solver::GeometryPolygonSet>>& map1,
                           std::map<int32_t, std::map<ViolationEnumType, ieda_solver::GeometryPolygonSet>>& map2,
                           std::map<int32_t, std::map<ViolationEnumType, std::vector<ieda_solver::GeometryRect>>>& result) {
      for (auto& [layer, violation_map] : map1) {
        for (auto& [type, polyset] : violation_map) {
          auto& other_polyset = map2[layer][type];
          auto diff_set = polyset - other_polyset;
          ieda_solver::getDefaultRectangles(result[layer][type], diff_set);
        }
      }
    };

    std::map<int32_t, std::map<ViolationEnumType, ieda_solver::GeometryPolygonSet>> third_layer_type_polyset_map;
    std::map<int32_t, std::map<ViolationEnumType, ieda_solver::GeometryPolygonSet>> idrc_layer_type_polyset_map;

    create_set(third_layer_type_rect_list_map, third_layer_type_polyset_map);
    create_set(idrc_layer_type_rect_list_map, idrc_layer_type_polyset_map);

    diff_two_map(third_layer_type_polyset_map, idrc_layer_type_polyset_map, third_diff_idrc_layer_type_rect_list_map);
    diff_two_map(idrc_layer_type_polyset_map, third_layer_type_polyset_map, idrc_diff_third_layer_type_rect_list_map);
  }

  std::string third_gds_file = output_dir + "/third.gds";
  std::string idrc_gds_file = output_dir + "/idrc.gds";
  std::string third_diff_idrc_gds_file = output_dir + "/third_diff_idrc.gds";
  std::string idrc_diff_third_gds_file = output_dir + "/idrc_diff_third.gds";

  plotGDS(third_gds_file, third_layer_type_rect_list_map);
  plotGDS(idrc_gds_file, idrc_layer_type_rect_list_map);
  plotGDS(third_diff_idrc_gds_file, third_diff_idrc_layer_type_rect_list_map);
  plotGDS(idrc_diff_third_gds_file, idrc_diff_third_layer_type_rect_list_map);
}

}  // namespace idrc