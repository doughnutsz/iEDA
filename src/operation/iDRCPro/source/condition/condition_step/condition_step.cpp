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

#include "condition_step.h"

#include "DRCViolationType.h"
#include "condition.h"
#include "idrc_util.h"
#include "idrc_violation.h"
#include "idrc_violation_manager.h"
#include "rule_condition_edge.h"
#include "rule_enum.h"
#include "tech_rules.h"

namespace idrc {

bool DrcRuleConditionStep::checkFastMode()
{
  bool b_result = true;

  b_result &= checkMinStep();

  return b_result;
}

bool DrcRuleConditionStep::checkCompleteMode()
{
  bool b_result = true;

  b_result &= checkMinStep();

  return b_result;
}

bool DrcRuleConditionStep::checkMinStep()
{
  bool b_result = true;

  auto& check_map = _condition_manager->get_check_map(RuleType::kEdgeMinStep);
  for (auto& [layer, check_list] : check_map) {
    // get rule step map
    auto* rule_routing_layer = DrcTechRuleInst->get_rule_routing_layer(layer);
    if (rule_routing_layer == nullptr) {
      continue;
    }
    auto* rule_map = rule_routing_layer->get_condition_map(RuleType::kEdge);
    auto& rule_step_map = rule_map->get_rule_map(RuleType::kEdgeMinStep);
    auto& rule_step_lef58_map = rule_map->get_rule_map(RuleType::kEdgeMinStepLef58);

    // handle all small step edges
    for (auto& point_pair : check_list->get_points()) {
      if (point_pair.first->is_min_step_checked() && point_pair.second->is_min_step_checked()) {
        continue;
      }

      // swap edge points order
      auto* point_1_next = point_pair.first->nextEndpoint();
      auto* point_2_next = point_pair.second->nextEndpoint();
      if (point_1_next != point_pair.second && point_2_next != point_pair.first) {
        continue;
      } else if (point_2_next == point_pair.first) {
        std::swap(point_pair.first, point_pair.second);
      }

      // match rule min step
      if (!checkMinStepSegment(point_pair.first, point_pair.second, layer, rule_step_map)) {
        b_result = false;
      }

      // TODO: match rule min step lef58
      if (!checkMinStepLef58Segment(point_pair.first, point_pair.second, layer, rule_step_lef58_map)) {
        b_result = false;
      }
    }
  }

  return b_result;
}

bool DrcRuleConditionStep::checkMinStepSegment(DrcBasicPoint* point_prev, DrcBasicPoint* point_next, idb::IdbLayer* layer,
                                               std::map<int, std::vector<ConditionRule*>> rule_step_map)
{
  bool b_result = true;

  int step_edge_length = point_prev->distance(point_next);
  // find rule and check
  for (auto& [value, rule_step_list] : rule_step_map) {
    if (value <= step_edge_length) {
      continue;
    }

    for (auto& rule_step : rule_step_list) {
      // violation rect data
      int llx = std::min(point_prev->get_x(), point_next->get_x());
      int lly = std::min(point_prev->get_y(), point_next->get_y());
      int urx = std::max(point_prev->get_x(), point_next->get_x());
      int ury = std::max(point_prev->get_y(), point_next->get_y());
      std::set<int> net_ids;
      net_ids.insert(point_prev->get_id());
      net_ids.insert(point_next->get_id());

      // get rule data
      auto* condition_rule_step = static_cast<ConditionRuleMinStep*>(rule_step);
      int min_step_length = condition_rule_step->get_min_step()->get_min_step_length();
      int max_edges = condition_rule_step->get_min_step()->has_max_edges() ? condition_rule_step->get_min_step()->get_max_edges() : 1;

      int edge_cnt = 1;
      bool is_violation = false;

      // find continuous small edges
      auto walk_check = [&](DrcBasicPoint* point, std::function<DrcBasicPoint*(DrcBasicPoint*)> iterate_func) {
        point->set_checked_min_step();
        auto* current_point = point;
        auto* iter_point = iterate_func(current_point);
        while (!is_violation && current_point->distance(iter_point) < min_step_length && iter_point != point) {
          current_point = iter_point;
          iter_point = iterate_func(current_point);
          edge_cnt++;

          llx = std::min(llx, current_point->get_x());
          lly = std::min(lly, current_point->get_y());
          urx = std::max(urx, current_point->get_x());
          ury = std::max(ury, current_point->get_y());

          net_ids.insert(current_point->get_id());

          if (edge_cnt > max_edges) {
            is_violation = true;
            b_result = false;
#ifdef DEBUG_IDRC_CONDITION_STEP
            auto gtl_pts_1 = DrcUtil::getPolygonPoints(point);
            auto polygon_1 = ieda_solver::GtlPolygon(gtl_pts_1.begin(), gtl_pts_1.end());
#endif
            // create violation
            auto violation_type = ViolationEnumType::kMinStep;
            DrcViolationRect* violation_rect = new DrcViolationRect(layer, net_ids, violation_type, llx, lly, urx, ury);
            auto* violation_manager = _condition_manager->get_violation_manager();
            auto& violation_list = violation_manager->get_violation_list(violation_type);
            violation_list.emplace_back(static_cast<DrcViolation*>(violation_rect));
          }

          iter_point->set_checked_min_step();
        }
      };

      // check both prev and next
      walk_check(point_prev, [&](DrcBasicPoint* point) { return point->prevEndpoint(); });
      walk_check(point_next, [&](DrcBasicPoint* point) { return point->nextEndpoint(); });
    }
  }

  return b_result;
}

bool DrcRuleConditionStep::checkMinStepLef58Segment(DrcBasicPoint* point_prev, DrcBasicPoint* point_next, idb::IdbLayer* layer,
                                                    std::map<int, std::vector<ConditionRule*>> rule_step_map)
{
  bool b_result = true;

  auto corner_prev = point_prev->getCornerType();
  auto corner_next = point_next->getCornerType();

  if (corner_prev == DrcCornerType::kNone || corner_next == DrcCornerType::kNone || corner_prev == corner_next) {
    return b_result;
  }

  int step_edge_length = point_prev->distance(point_next);
  // find rule and check
  for (auto& [value, rule_step_list] : rule_step_map) {
    if (value <= step_edge_length) {
      continue;
    }

    for (auto& rule_step : rule_step_list) {
      // violation rect data
      int llx = std::min(point_prev->get_x(), point_next->get_x());
      int lly = std::min(point_prev->get_y(), point_next->get_y());
      int urx = std::max(point_prev->get_x(), point_next->get_x());
      int ury = std::max(point_prev->get_y(), point_next->get_y());
      std::set<int> net_ids;
      net_ids.insert(point_prev->get_id());
      net_ids.insert(point_next->get_id());

      // get rule data
      auto* condition_rule_step = static_cast<ConditionRuleMinStepLef58*>(rule_step);
      if (!condition_rule_step->get_min_step()->get_min_adjacent_length().has_value()) {
        continue;
      }

      int min_adjacent_length = condition_rule_step->get_min_step()->get_min_adjacent_length().value().get_min_adj_length();

      if (corner_prev == DrcCornerType::kConvex) {
        auto* point_prev_prev = point_prev->prevEndpoint();
        int edge_length_prev = point_prev->distance(point_prev_prev);

        if (edge_length_prev < min_adjacent_length && point_prev_prev->getCornerType() == DrcCornerType::kConcave) {
          llx = std::min(llx, point_prev_prev->get_x());
          lly = std::min(lly, point_prev_prev->get_y());
          urx = std::max(urx, point_prev_prev->get_x());
          ury = std::max(ury, point_prev_prev->get_y());

          net_ids.insert(point_prev_prev->get_id());

          point_prev_prev->set_checked_min_step();
          point_prev->set_checked_min_step();
          point_next->set_checked_min_step();

#ifdef DEBUG_IDRC_CONDITION_STEP
          auto gtl_pts_1 = DrcUtil::getPolygonPoints(point_prev);
          auto polygon_1 = ieda_solver::GtlPolygon(gtl_pts_1.begin(), gtl_pts_1.end());
#endif
          // create violation
          auto violation_type = ViolationEnumType::kMinStep;
          DrcViolationRect* violation_rect = new DrcViolationRect(layer, net_ids, violation_type, llx, lly, urx, ury);
          auto* violation_manager = _condition_manager->get_violation_manager();
          auto& violation_list = violation_manager->get_violation_list(violation_type);
          violation_list.emplace_back(static_cast<DrcViolation*>(violation_rect));
        }
      }

      if (corner_next == DrcCornerType::kConvex) {
        auto* point_next_next = point_next->nextEndpoint();
        int edge_length_next = point_next->distance(point_next_next);

        if (edge_length_next < min_adjacent_length && point_next_next->getCornerType() == DrcCornerType::kConcave) {
          llx = std::min(llx, point_next_next->get_x());
          lly = std::min(lly, point_next_next->get_y());
          urx = std::max(urx, point_next_next->get_x());
          ury = std::max(ury, point_next_next->get_y());

          net_ids.insert(point_next_next->get_id());

          point_prev->set_checked_min_step();
          point_next->set_checked_min_step();
          point_next_next->set_checked_min_step();

#ifdef DEBUG_IDRC_CONDITION_STEP
          auto gtl_pts_1 = DrcUtil::getPolygonPoints(point_next);
          auto polygon_1 = ieda_solver::GtlPolygon(gtl_pts_1.begin(), gtl_pts_1.end());
#endif
          // create violation
          auto violation_type = ViolationEnumType::kMinStep;
          DrcViolationRect* violation_rect = new DrcViolationRect(layer, net_ids, violation_type, llx, lly, urx, ury);
          auto* violation_manager = _condition_manager->get_violation_manager();
          auto& violation_list = violation_manager->get_violation_list(violation_type);
          violation_list.emplace_back(static_cast<DrcViolation*>(violation_rect));
        }
      }
    }
  }

  return b_result;
}

}  // namespace idrc