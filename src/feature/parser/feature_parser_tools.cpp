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
/**
 * @project		iEDA
 * @file		feature_parser.cpp
 * @author		Yell
 * @date		10/08/2023
 * @version		0.1
 * @description


        feature parser
 *
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "feature_irt.h"
#include "feature_parser.h"
#include "feature_summary.h"

namespace ieda_feature {

json FeatureParser::buildSummaryRT()
{
  json summary_rt;

  RTSummary& rt_sum = _summary->get_summary_irt();

  json rt_pa;
  for (auto routing_access_point_num : rt_sum.pa_summary.routing_access_point_num_map) {
    rt_pa["routing_access_point_num_map"][std::to_string(routing_access_point_num.first)] = routing_access_point_num.second;
  }
  for (auto type_access_point_num : rt_sum.pa_summary.type_access_point_num_map) {
    rt_pa["routing_access_point_num_map"][type_access_point_num.first] = type_access_point_num.second;
  }
  rt_pa["routing_access_point_num_map"]["total_access_point_num"] = rt_sum.pa_summary.total_access_point_num;
  summary_rt["PA"] = rt_pa;

  auto& sa_sum = rt_sum.sa_summary;
  json rt_sa;
  for (auto routing_supply_num : rt_sum.sa_summary.routing_supply_map) {
    rt_sa["routing_supply_num_map"][std::to_string(routing_supply_num.first)] = routing_supply_num.second;
  }
  rt_sa["routing_supply_num_map"]["total_supply_num"] = rt_sum.sa_summary.total_supply;

  json rt_ir;
  for (auto demand : rt_sum.ir_summary.routing_demand_map) {
    rt_ir["routing_demand_map"][std::to_string(demand.first)] = demand.second;
  }
  rt_ir["routing_demand_map"]["total_demand"] = rt_sum.ir_summary.total_demand;
  for (auto routing_overflow : rt_sum.ir_summary.routing_overflow_map) {
    rt_ir["routing_overflow_map"][std::to_string(routing_overflow.first)] = routing_overflow.second;
  }
  rt_ir["routing_overflow_map"]["total_overflow"] = rt_sum.ir_summary.total_overflow;
  for (auto routing_wire_length : rt_sum.ir_summary.routing_wire_length_map) {
    rt_ir["routing_wire_length_map"][std::to_string(routing_wire_length.first)] = routing_wire_length.second;
  }
  rt_ir["routing_wire_length_map"]["total_wire_length"] = rt_sum.ir_summary.total_wire_length;
  for (auto cut_via_num : rt_sum.ir_summary.cut_via_num_map) {
    rt_ir["routing_cut_via_num_map"][std::to_string(cut_via_num.first)] = cut_via_num.second;
  }
  rt_ir["routing_cut_via_num_map"]["total_cut_via_num"] = rt_sum.ir_summary.total_via_num;
  for (auto timing : rt_sum.ir_summary.timing) {
    rt_ir["routing_timing_map"][timing.first] = timing.second;
  }
  summary_rt["IR"] = rt_ir;

  // // GR
  for (auto [id, gr_sum] : rt_sum.iter_gr_summary_map) {
    json rt_gr;
    // 和ir一样
    for (auto demand : gr_sum.routing_demand_map) {
      rt_gr["routing_demand_map"][std::to_string(demand.first)] = demand.second;
    }
    rt_gr["routing_demand_map"]["total_demand"] = gr_sum.total_demand;
    for (auto routing_overflow : gr_sum.routing_overflow_map) {
      rt_gr["routing_overflow_map"][std::to_string(routing_overflow.first)] = routing_overflow.second;
    }
    rt_gr["routing_overflow_map"]["total_overflow"] = gr_sum.total_overflow;
    for (auto routing_wire_length : gr_sum.routing_wire_length_map) {
      rt_gr["routing_wire_length_map"][std::to_string(routing_wire_length.first)] = routing_wire_length.second;
    }
    rt_gr["routing_wire_length_map"]["total_wire_length"] = gr_sum.total_wire_length;
    for (auto cut_via_num : gr_sum.cut_via_num_map) {
      rt_gr["routing_cut_via_num_map"][std::to_string(cut_via_num.first)] = cut_via_num.second;
    }
    rt_gr["routing_cut_via_num_map"]["total_cut_via_num"] = gr_sum.total_via_num;
    for (auto timing : gr_sum.timing) {
      rt_gr["routing_timing_map"][timing.first] = timing.second;
    }
    summary_rt["GR"][std::to_string(id)] = rt_gr;
  }
  // TA
  json rt_ta;
  // wirelength, violation
  for (auto routing_wire_length : rt_sum.ta_summary.routing_wire_length_map) {
    rt_ta["routing_wire_length_map"][std::to_string(routing_wire_length.first)] = routing_wire_length.second;
  }
  rt_ta["routing_wire_length_map"]["total_wire_length"] = rt_sum.ta_summary.total_wire_length;
  for (auto routing_violation : rt_sum.ta_summary.routing_violation_num_map) {
    rt_ta["routing_violation_map"][std::to_string(routing_violation.first)] = routing_violation.second;
  }
  rt_ta["routing_violation_map"]["total_violation"] = rt_sum.ta_summary.total_violation_num;
  summary_rt["TA"] = rt_ta;

  // DR
  for (auto [id, dr_sum] : rt_sum.iter_dr_summary_map) {
    json rt_dr;
    for (auto routing_wire_length : dr_sum.routing_wire_length_map) {
      rt_dr["routing_wire_length_map"][std::to_string(routing_wire_length.first)] = routing_wire_length.second;
    }
    rt_dr["routing_wire_length_map"]["total_wire_length"] = dr_sum.total_wire_length;
    for (auto cut_via_num : dr_sum.cut_via_num_map) {
      rt_dr["routing_cut_via_num_map"][std::to_string(cut_via_num.first)] = cut_via_num.second;
    }
    rt_dr["routing_cut_via_num_map"]["total_cut_via_num"] = dr_sum.total_via_num;
    // violation
    for (auto routing_violation : dr_sum.routing_violation_num_map) {
      rt_dr["routing_violation_map"][std::to_string(routing_violation.first)] = routing_violation.second;
    }
    rt_dr["routing_violation_map"]["total_violation"] = dr_sum.total_violation_num;
    for (auto routing_patch_num : dr_sum.routing_patch_num_map) {
      rt_dr["routing_patch_num_map"][std::to_string(routing_patch_num.first)] = routing_patch_num.second;
    }
    rt_dr["routing_patch_num_map"]["total_patch_num"] = dr_sum.total_patch_num;
    for (auto timing : dr_sum.timing) {
      rt_dr["routing_timing_map"][timing.first] = timing.second;
    }
    summary_rt["DR"][std::to_string(id)] = rt_dr;
  }
  return summary_rt;
}

json FeatureParser::buildSummaryPL(std::string step)
{
  json summary_pl;

  PlaceSummary& pl_summary = _summary->get_summary_ipl();

  if (step == "place") {
    summary_pl["gplace"]["place_density"] = pl_summary.gplace.place_density;
    summary_pl["gplace"]["pin_density"] = pl_summary.gplace.pin_density;
    summary_pl["gplace"]["HPWL"] = pl_summary.gplace.HPWL;
    summary_pl["gplace"]["STWL"] = pl_summary.gplace.STWL;
    summary_pl["gplace"]["GRWL"] = pl_summary.gplace.GRWL;
    summary_pl["gplace"]["congestion"] = pl_summary.gplace.congestion;
    summary_pl["gplace"]["tns"] = pl_summary.gplace.tns;
    summary_pl["gplace"]["wns"] = pl_summary.gplace.wns;
    summary_pl["gplace"]["suggest_freq"] = pl_summary.gplace.suggest_freq;

    summary_pl["dplace"]["place_density"] = pl_summary.dplace.place_density;
    summary_pl["dplace"]["pin_density"] = pl_summary.dplace.pin_density;
    summary_pl["dplace"]["HPWL"] = pl_summary.dplace.HPWL;
    summary_pl["dplace"]["STWL"] = pl_summary.dplace.STWL;
    summary_pl["dplace"]["GRWL"] = pl_summary.dplace.GRWL;
    summary_pl["dplace"]["congestion"] = pl_summary.dplace.congestion;
    summary_pl["dplace"]["tns"] = pl_summary.dplace.tns;
    summary_pl["dplace"]["wns"] = pl_summary.dplace.wns;
    summary_pl["dplace"]["suggest_freq"] = pl_summary.dplace.suggest_freq;

    summary_pl["instance_cnt"] = pl_summary.instance_cnt;
    summary_pl["fix_inst_cnt"] = pl_summary.fix_inst_cnt;
    summary_pl["net_cnt"] = pl_summary.net_cnt;
    summary_pl["total_pins"] = pl_summary.total_pins;
    summary_pl["bin_number"] = pl_summary.bin_number;
    summary_pl["bin_size_x"] = pl_summary.bin_size_x;
    summary_pl["bin_size_y"] = pl_summary.bin_size_y;
    summary_pl["overflow_number"] = pl_summary.overflow_number;
    summary_pl["overflow"] = pl_summary.overflow;
  }
  // 3:合法化需要存储的数据参数
  else if (step == "legalization") {
    summary_pl["legalization"]["place_density"] = pl_summary.lg_summary.pl_common_summary.place_density;
    summary_pl["legalization"]["pin_density"] = pl_summary.lg_summary.pl_common_summary.pin_density;
    summary_pl["legalization"]["HPWL"] = pl_summary.lg_summary.pl_common_summary.HPWL;
    summary_pl["legalization"]["STWL"] = pl_summary.lg_summary.pl_common_summary.STWL;
    summary_pl["legalization"]["GRWL"] = pl_summary.lg_summary.pl_common_summary.GRWL;
    summary_pl["legalization"]["congestion"] = pl_summary.lg_summary.pl_common_summary.congestion;
    summary_pl["legalization"]["tns"] = pl_summary.lg_summary.pl_common_summary.tns;
    summary_pl["legalization"]["wns"] = pl_summary.lg_summary.pl_common_summary.wns;
    summary_pl["legalization"]["suggest_freq"] = pl_summary.lg_summary.pl_common_summary.suggest_freq;

    summary_pl["legalization"]["total_movement"] = pl_summary.lg_summary.lg_total_movement;
    summary_pl["legalization"]["max_movement"] = pl_summary.lg_summary.lg_max_movement;
  }

  return summary_pl;
}

json FeatureParser::buildSummaryCTS()
{
  json json_cts;

  CTSSummary& summary = _summary->get_summary_icts();

  json_cts["buffer_num"] = summary.buffer_num;
  json_cts["buffer_area"] = summary.buffer_area;
  json_cts["clock_path_min_buffer"] = summary.clock_path_min_buffer;
  json_cts["clock_path_max_buffer"] = summary.clock_path_max_buffer;
  json_cts["max_level_of_clock_tree"] = summary.max_level_of_clock_tree;
  json_cts["max_clock_wirelength"] = summary.max_clock_wirelength;
  json_cts["total_clock_wirelength"] = summary.total_clock_wirelength;

  json json_timing;
  for (int i = 0; i < summary.nets_timing.size(); ++i) {
    auto net_timing = summary.nets_timing[i];

    json_timing[i]["net_name"] = net_timing.net_name;
    json_timing[i]["setup_tns"] = net_timing.setup_tns;
    json_timing[i]["setup_wns"] = net_timing.setup_wns;
    json_timing[i]["hold_tns"] = net_timing.hold_tns;
    json_timing[i]["hold_wns"] = net_timing.hold_wns;
    json_timing[i]["suggest_freq"] = net_timing.suggest_freq;
  }

  json_cts["nets_timing"] = json_timing;

  return json_cts;
}

json FeatureParser::buildSummaryNetOpt()
{
  json json_netopt;

  NetOptSummary& summary = _summary->get_summary_ino();

  json json_net_timings;
  for (int i = 0; i < summary.net_timings.size(); ++i) {
    NONetTimingCmp net_timing = summary.net_timings[i];

    json_net_timings[i]["clock_name"] = net_timing.net_name;
    json_net_timings[i]["origin_setup_tns"] = net_timing.origin.setup_tns;
    json_net_timings[i]["origin_setup_wns"] = net_timing.origin.setup_wns;
    json_net_timings[i]["origin_hold_tns"] = net_timing.origin.hold_tns;
    json_net_timings[i]["origin_hold_wns"] = net_timing.origin.hold_wns;
    json_net_timings[i]["origin_suggest_freq"] = net_timing.origin.suggest_freq;
    json_net_timings[i]["opt_setup_tns"] = net_timing.opt.setup_tns;
    json_net_timings[i]["opt_setup_wns"] = net_timing.opt.setup_wns;
    json_net_timings[i]["opt_hold_tns"] = net_timing.opt.hold_tns;
    json_net_timings[i]["opt_hold_wns"] = net_timing.opt.hold_wns;
    json_net_timings[i]["opt_suggest_freq"] = net_timing.opt.suggest_freq;
    json_net_timings[i]["detal_setup_tns"] = net_timing.detal.setup_tns;
    json_net_timings[i]["detal_setup_wns"] = net_timing.detal.setup_wns;
    json_net_timings[i]["detal_hold_tns"] = net_timing.detal.hold_tns;
    json_net_timings[i]["detal_hold_wns"] = net_timing.detal.hold_wns;
    json_net_timings[i]["detal_suggest_freq"] = net_timing.detal.suggest_freq;
  }

  json_netopt["nets_timing"] = json_net_timings;

  return json_netopt;
}

json FeatureParser::buildSummaryTO(std::string step)
{
  auto step_summary = [](std::string step, FeatureSummary* summary) {
    if (step == "optDrv") {
      return summary->get_summary_ito_optdrv();
    } else if (step == "optHold") {
      return summary->get_summary_ito_opthold();
    } else {
      return summary->get_summary_ito_optsetup();
    }
  };

  json summary_to;

  TimingOptSummary summary = step_summary(step, _summary);

  summary_to["HPWL"] = summary.HPWL;
  summary_to["STWL"] = summary.STWL;

  json json_net_timings;
  for (int i = 0; i < summary.net_timings.size(); ++i) {
    TONetTimingCmp net_timing = summary.net_timings[i];

    json_net_timings[i]["net_name"] = net_timing.net_name;
    json_net_timings[i]["origin_tns"] = net_timing.origin.tns;
    json_net_timings[i]["origin_wns"] = net_timing.origin.wns;
    json_net_timings[i]["origin_suggest_freq"] = net_timing.origin.suggest_freq;
    json_net_timings[i]["opt_tns"] = net_timing.opt.tns;
    json_net_timings[i]["opt_wns"] = net_timing.opt.wns;
    json_net_timings[i]["opt_suggest_freq"] = net_timing.opt.suggest_freq;
    json_net_timings[i]["detal_tns"] = net_timing.detal.tns;
    json_net_timings[i]["detal_wns"] = net_timing.detal.wns;
    json_net_timings[i]["detal_suggest_freq"] = net_timing.detal.suggest_freq;
  }

  summary_to["nets_timing"] = json_net_timings;

  return summary_to;
}

json FeatureParser::buildSummarySTA()
{
  json summary_sta;
  //   auto timing_engine = ista::TimingEngine::getOrCreateTimingEngine();
  //   auto* ista = timing_engine->get_ista();
  //   auto& all_clocks = ista->get_clocks();

  //   // iterate the clock group of all the clocks.
  //   for (unsigned id = 1; auto& clock_group : all_clocks) {
  //     json::value_type path_group;
  //     path_group["timing_path_group"]["id"] = id++;
  //     std::string group_name = clock_group->get_clock_name();
  //     path_group["timing_path_group"]["name"] = group_name;
  //     double wns = ista->getWNS(group_name.c_str(), ista::AnalysisMode::kMax);
  //     double tns = ista->getTNS(group_name.c_str(), ista::AnalysisMode::kMax);
  //     path_group["timing_path_group"]["WNS"] = wns;
  //     path_group["timing_path_group"]["TNS"] = tns;
  //     path_group["timing_path_group"]["NVP"] = 0;  // TBD for negative violated points.
  //     double freq = 1000.0 / (clock_group->getPeriodNs() - wns);
  //     path_group["timing_path_group"]["FREQ"] = freq;
  //     double hold_wns = ista->getWNS(group_name.c_str(), ista::AnalysisMode::kMin);
  //     double hold_tns = ista->getTNS(group_name.c_str(), ista::AnalysisMode::kMin);
  //     path_group["timing_path_group"]["hold_WNS"] = wns;
  //     path_group["timing_path_group"]["hold_TNS"] = tns;
  //     path_group["timing_path_group"]["hold_NVP"] = 0;  // TBD for hold negative violated points.

  //     FOREACH_MODE(mode)
  //     {
  //       json::value_type analysis_mode;
  //       analysis_mode["analysis_mode"] = (mode == AnalysisMode::kMax) ? "max_delay/setup" : "min_delay/hold";
  //       analysis_mode["levels_of_logic"] = 0;       // TBD
  //       analysis_mode["critical_path_length"] = 0;  // TBD
  //       analysis_mode["critical_path_slack"] = (mode == AnalysisMode::kMax) ? wns : hold_wns;
  //       analysis_mode["total_negative_slack"] = (mode == AnalysisMode::kMax) ? tns : hold_tns;
  //       path_group["timing_path_group"]["analysis_mode_infos"].push_back(analysis_mode);
  //     }
  //     summary_sta.push_back(path_group);
  //   }

  return summary_sta;
}

json FeatureParser::buildSummaryDRC()
{
  json summary_drc;

  //   auto drc_map = idrc::DrcAPIInst.getCheckResult();
  //   // summary_drc["short_nums"] = drc_map
  //   for (auto& [key, value] : drc_map) {
  //     summary_drc[key] = value;
  //   }

  return summary_drc;
}

}  // namespace ieda_feature
