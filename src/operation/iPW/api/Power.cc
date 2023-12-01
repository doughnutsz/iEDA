// ***************************************************************************************
// Copyright (c) 2023-2025 Peng Cheng Laboratory
// Copyright (c) 2023-2025 Institute of Computing Technology, Chinese Academy of
// Sciences Copyright (c) 2023-2025 Beijing Institute of Open Source Chip
//
// iEDA is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan
// PSL v2. You may obtain a copy of Mulan PSL v2 at:
// http://license.coscl.org.cn/MulanPSL2
//
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
/**
 * @file Power.cc
 * @author simin tao (taosm@pcl.ac.cn)
 * @brief The top class of power analysis, should include the api wrapper etc.
 * @version 0.1
 * @date 2023-01-02
 */

#include "Power.hh"

#include <array>

#include "ops/annotate_toggle_sp/AnnotateToggleSP.hh"
#include "ops/build_graph/PwrBuildGraph.hh"
#include "ops/calc_power/PwrCalcInternalPower.hh"
#include "ops/calc_power/PwrCalcLeakagePower.hh"
#include "ops/calc_power/PwrCalcSwitchPower.hh"
#include "ops/dump/PwrDumpGraph.hh"
#include "ops/dump/PwrDumpSeqGraph.hh"
#include "ops/levelize_seq_graph/PwrBuildSeqGraph.hh"
#include "ops/levelize_seq_graph/PwrCheckPipelineLoop.hh"
#include "ops/levelize_seq_graph/PwrLevelizeSeqGraph.hh"
#include "ops/plot_power/PwrReport.hh"
#include "ops/plot_power/PwrReportInstance.hh"
#include "ops/propagate_toggle_sp/PwrPropagateClock.hh"
#include "ops/propagate_toggle_sp/PwrPropagateConst.hh"
#include "ops/propagate_toggle_sp/PwrPropagateToggleSP.hh"
#include "ops/read_vcd/VCDParserWrapper.hh"

namespace ipower {

Power* Power::_power = nullptr;

/**
 * @brief Get the top power instance, if not, create one.
 *
 * @return Power*
 */
Power* Power::getOrCreatePower(StaGraph* sta_graph) {
  static std::mutex mt;
  if (_power == nullptr) {
    if (_power == nullptr) {
      std::lock_guard<std::mutex> lock(mt);
      _power = new Power(sta_graph);
    }
  }
  return _power;
}

/**
 * @brief Destroy the power.
 *
 */
void Power::destroyPower() {
  delete _power;
  _power = nullptr;
}

/**
 * @brief build power graph.
 *
 * @return unsigned
 */
unsigned Power::buildGraph() {
  // TODO build graph use power graph in Power class.
  PwrBuildGraph build_graph(_power_graph);
  build_graph(_power_graph.get_sta_graph());
  _power_graph.set_pwr_seq_graph(&_power_seq_graph);
  return 1;
}

/**
 * @brief setup power relative clock.
 *
 * @param fastest_clock
 * @param sta_clocks
 * @return unsigned
 */
unsigned Power::setupClock(PwrClock&& fastest_clock,
                           Vector<StaClock*>&& sta_clocks) {
  _power_graph.set_fastest_clock(std::move(fastest_clock));
  _power_graph.set_sta_clocks(std::move(sta_clocks));
  return 1;
}

/**
 * @brief read a VCD file
 *
 * @param vcd_path
 * @param begin_end_time
 * @return unsigned
 */
unsigned Power::readVCD(
    std::string_view vcd_path, std::string top_instance_name,
    std::optional<std::pair<int64_t, int64_t>> begin_end_time) {
  LOG_INFO << "read vcd start";
  _vcd_wrapper.readVCD(vcd_path, begin_end_time);
  _vcd_wrapper.buildAnnotateDB(top_instance_name);
  _vcd_wrapper.calcScopeToggleAndSp();

  // TODO make a opt for out file
  // std::ofstream out_file;
  // out_file.open("vcd_out_file.txt", std::ios::out | std::ios::trunc);
  // _vcd_wrapper.printAnnotateDB(out_file);
  // out_file.close();
  LOG_INFO << "read vcd end";
  return 1;
}

/**
 * @brief read a VCD file by rust vcd parser
 *
 * @param vcd_path
 * @param top_instance_name
 * @return unsigned
 */
unsigned Power::readRustVCD(const char* vcd_path,
                            const char* top_instance_name) {
  LOG_INFO << "read vcd start";
  _rust_vcd_wrapper.readVcdFile(vcd_path);
  _rust_vcd_wrapper.buildAnnotateDB(top_instance_name);
  _rust_vcd_wrapper.calcScopeToggleAndSp(top_instance_name);
  LOG_INFO << "read vcd end";

  return 1;
}

/**
 * @brief annotate vcd toggle sp to pwr vertex.
 *
 * @param annotate_db
 * @return unsigned
 */
unsigned Power::annotateToggleSP() {
  LOG_INFO << "annotate toggle sp start";

  AnnotateToggleSP annotate_toggle_SP;
  annotate_toggle_SP.set_annotate_db(_vcd_wrapper.get_annotate_db());

  unsigned is_ok = annotate_toggle_SP(&_power_graph);
  LOG_INFO << "annotate toggle sp end";

  return is_ok;
}

/**
 * @brief build sequential graph.
 *
 * @return unsigned
 */
unsigned Power::buildSeqGraph() {
  PwrBuildSeqGraph build_seq_graph;
  build_seq_graph(&_power_graph);
  _power_seq_graph = std::move(build_seq_graph.takePwrSeqGraph());
  return 1;
}

/**
 * @brief dump sequential graph in graphviz format.
 *
 * @return unsigned
 */
unsigned Power::dumpSeqGraphViz() {
  PwrDumpSeqGraphViz dump_seq_graph_viz;
  return dump_seq_graph_viz(&_power_seq_graph);
}

/**
 * @brief check pipline loop for break loop.
 *
 * @return unsigned
 */
unsigned Power::checkPipelineLoop() {
  PwrCheckPipelineLoop check_pipeline_loop;
  return check_pipeline_loop(&_power_seq_graph);
}

/**
 * @brief levelize the sequential graph.
 *
 * @return unsigned
 */
unsigned Power::levelizeSeqGraph() {
  PwrLevelizeSeq levelize_seq;
  return levelize_seq(&_power_seq_graph);
}

/**
 * @brief dump the power graph.
 *
 * @return unsigned
 */
unsigned Power::dumpGraph() {
  PwrDumpGraphYaml dump_graph;
  return dump_graph(&_power_graph);
}

/**
 * @brief Propagate clock vertexes.
 *
 * @param sta_clocks
 * @return unsigned
 */
unsigned Power::propagateClock() {
  PwrPropagateClock propagate_clock;
  return propagate_clock(&_power_graph);
}

/**
 * @brief propagate const to set const node.
 *
 * @return unsigned
 */
unsigned Power::propagateConst() {
  PwrPropagateConst propagate_const;
  return propagate_const(&_power_graph);
}

/**
 * @brief propagate toggle and sp.
 *
 * @return unsigned
 */
unsigned Power::propagateToggleSP() {
  PwrPropagateToggleSP propagate_toggle_sp;
  return propagate_toggle_sp(&_power_graph);
}

/**
 * @brief calc leakage power.
 *
 * @return unsigned
 */
unsigned Power::calcLeakagePower() {
  PwrCalcLeakagePower calc_leakage_power;
  calc_leakage_power(&_power_graph);
  _leakage_powers = std::move(calc_leakage_power.takeLeakagePowers());
  return 1;
}

/**
 * @brief calc internal power.
 *
 * @return unsigned
 */
unsigned Power::calcInternalPower() {
  PwrCalcInternalPower calc_internal_power;
  calc_internal_power(&_power_graph);
  _internal_powers = std::move(calc_internal_power.takeInternalPowers());
  return 1;
}

/**
 * @brief  calc switch power.
 *
 * @return unsigned
 */
unsigned Power::calcSwitchPower() {
  PwrCalcSwitchPower calc_switch_power;
  calc_switch_power(&_power_graph);
  _switch_powers = std::move(calc_switch_power.takeSwitchPowers());
  return 1;
}

/**
 * @brief the wrapper for levelization seq graph, const propagation, toggle/sp
 * propagation, analyze power.
 *
 * @return unsigned
 */
unsigned Power::updatePower() {
  {
    ieda::Stats stats;
    LOG_INFO << "power propagation start";

    // firstly levelization.
    Vector<std::function<unsigned(PwrSeqGraph*)>> seq_funcs = {
        PwrCheckPipelineLoop(), PwrLevelizeSeq()};
    auto& the_seq_graph = get_power_seq_graph();
    for (auto& func : seq_funcs) {
      the_seq_graph.exec(func);
    }

    // secondly propagation toggle and sp.
    Vector<std::function<unsigned(PwrGraph*)>> prop_funcs = {
        PwrPropagateConst(), PwrPropagateToggleSP(), PwrPropagateClock()};
    auto& the_pwr_graph = get_power_graph();
    for (auto& func : prop_funcs) {
      the_pwr_graph.exec(func);
    }

    LOG_INFO << "power propagation end";
    double memory_delta = stats.memoryDelta();
    LOG_INFO << "power propagation memory usage " << memory_delta << "MB";
    double time_delta = stats.elapsedRunTime();
    LOG_INFO << "power propagation time elapsed " << time_delta << "s";
  }

  {
    ieda::Stats stats;
    LOG_INFO << "power calculation start";

    // thirdly analyze power.
    calcLeakagePower();
    calcInternalPower();
    calcSwitchPower();
    analyzeGroupPower();

    LOG_INFO << "power calculation end";
    double memory_delta = stats.memoryDelta();
    LOG_INFO << "power calculation memory usage " << memory_delta << "MB";
    double time_delta = stats.elapsedRunTime();
    LOG_INFO << "power calculation time elapsed " << time_delta << "s";
  }

  return 1;
}

/**
 * @brief analyze power by group.
 *
 * @return unsigned
 */
unsigned Power::analyzeGroupPower() {
  auto add_group_data_from_analysis_data = [this](auto group_type,
                                                  PwrAnalysisData* power_data) {
    /*the lambda of set power data*/
    auto set_power_data = [this, &power_data](PwrGroupData* group_data) {
      double power_data_value = power_data->getPowerDataValue();
      if (power_data->isLeakageData()) {
        group_data->set_leakage_power(NW_TO_W(power_data_value));
      } else if (power_data->isInternalData()) {
        group_data->set_internal_power(MW_TO_W(power_data_value));
      } else {
        group_data->set_switch_power(MW_TO_W(power_data_value));
      }
    };

    // find the degisn object of power data
    auto* this_obj = power_data->get_design_obj();
    auto this_data = _obj_to_datas.find(this_obj);
    if (this_data != _obj_to_datas.end()) {
      set_power_data(this_data->second.get());
    } else {
      auto group_data = std::make_unique<PwrGroupData>(
          group_type, power_data->get_design_obj());
      set_power_data(group_data.get());
      addGroupData(std::move(group_data));
    }
  };

  PwrLeakageData* leakage_power_data;
  FOREACH_PWR_LEAKAGE_POWER(this, leakage_power_data) {
    auto* inst = dynamic_cast<Instance*>(leakage_power_data->get_design_obj());
    LOG_FATAL_IF(!inst) << "leakage power instance is not exist.";
    auto group_type = getInstPowerGroup(inst);
    if (group_type) {
      add_group_data_from_analysis_data(group_type.value(), leakage_power_data);
    } else {
      LOG_INFO << "can not find group type for" << inst->get_name();
    }
  }

  PwrInternalData* internal_power_data;
  FOREACH_PWR_INTERNAL_POWER(this, internal_power_data) {
    auto* inst = dynamic_cast<Instance*>(internal_power_data->get_design_obj());
    LOG_FATAL_IF(!inst) << "internal power instance is not exist.";
    auto group_type = getInstPowerGroup(inst);
    if (group_type) {
      add_group_data_from_analysis_data(group_type.value(),
                                        internal_power_data);
    } else {
      LOG_INFO << "can not find group type for" << inst->get_name();
    }
  }

  PwrSwitchData* switch_power_data;
  FOREACH_PWR_SWITCH_POWER(this, switch_power_data) {
    auto* net = dynamic_cast<Net*>(switch_power_data->get_design_obj());
    auto* driver_obj = net->getDriver();

    auto* the_sta_graph = _power_graph.get_sta_graph();
    auto driver_sta_vertex = the_sta_graph->findVertex(driver_obj);

    PwrVertex* driver_pwr_vertex = nullptr;
    if (driver_sta_vertex) {
      driver_pwr_vertex = _power_graph.staToPwrVertex(*driver_sta_vertex);
    } else {
      LOG_FATAL << "not found driver sta vertex.";
    }

    // TODO  input port
    if (driver_pwr_vertex->is_input_port()) {
      continue;
    }

    auto* driver_inst = driver_pwr_vertex->getOwnInstance();
    if (!driver_inst) {
      LOG_FATAL << "not found driver instance.";
    }

    auto group_type = getInstPowerGroup(driver_inst);
    if (group_type) {
      add_group_data_from_analysis_data(group_type.value(), switch_power_data);
    } else {
      LOG_INFO << "can not find group type for" << driver_inst->get_name();
    }
  }
  return 1;
}

/**
 * @brief report power
 *
 * @param rpt_file_name
 * @return unsigned
 */
unsigned Power::reportSummaryPower(const char* rpt_file_name,
                                   PwrAnalysisMode pwr_analysis_mode) {
  PwrReportPowerSummary report_power(rpt_file_name, pwr_analysis_mode);
  report_power(this);
  auto& report_summary_data = report_power.get_report_summary_data();
  auto report_tbl = report_power.createReportTable("Power Analysis Report");

  std::map<PwrGroupData::PwrGroupType, std::string> group_type_to_string = {
      {PwrGroupData::PwrGroupType::kIOPad, "io_pad"},
      {PwrGroupData::PwrGroupType::kMemory, "memory"},
      {PwrGroupData::PwrGroupType::kBlackBox, "black_box"},
      {PwrGroupData::PwrGroupType::kClockNetwork, "clock_network"},
      {PwrGroupData::PwrGroupType::kRegister, "register"},
      {PwrGroupData::PwrGroupType::kComb, "combinational"},
      {PwrGroupData::PwrGroupType::kSeq, "sequential"}};

  // lambda for print power data float to string.
  auto data_str = [](double data) { return Str::printf("%.3e", data); };
  auto data_str_f = [](double data) { return Str::printf("%.3f", data); };

  double total_power = report_summary_data.get_total_power();
  /*Add group data to report table.*/
  PwrReportGroupSummaryData* report_group_data;
  FOREACH_REPORT_GROUP_DATA(&report_summary_data, report_group_data) {
    double group_total_power = report_group_data->get_total_power();
    // calc percentage
    double percentage = CalcPercentage(group_total_power / total_power);

    std::string str_percentage =
        std::string("(") + data_str_f(percentage) + std::string("%)");

    (*report_tbl) << group_type_to_string[report_group_data->get_group_type()]
                  << data_str(report_group_data->get_internal_power())
                  << data_str(report_group_data->get_switch_power())
                  << data_str(report_group_data->get_leakage_power())
                  << data_str(report_group_data->get_total_power())
                  << str_percentage << TABLE_ENDLINE;
  }

  LOG_INFO << "\n" << report_tbl->c_str();

  auto close_file = [](std::FILE* fp) { std::fclose(fp); };

  std::unique_ptr<std::FILE, decltype(close_file)> f(
      std::fopen(rpt_file_name, "w"), close_file);

  std::fprintf(f.get(), "Generate the report at %s\n", Time::getNowWallTime());

  std::map<PwrAnalysisMode, std::string> analysis_mode_to_string = {
      {PwrAnalysisMode::kAveraged, "Averaged"},
      {PwrAnalysisMode::kTimeBase, "TimeBase"},
      {PwrAnalysisMode::kClockCycle, "ClockCycle"}};

  std::fprintf(f.get(), "Report : %s Power\n ",
               analysis_mode_to_string[pwr_analysis_mode].c_str());

  std::fprintf(f.get(), "%s", report_tbl->c_str());

  // print switch power
  double summary_switch_power = report_summary_data.get_net_switching_power();
  std::string summary_switch_power_percentage =
      std::string("(") +
      data_str_f(CalcPercentage(report_summary_data.get_net_switching_power() /
                                total_power)) +
      std::string("%)");
  std::fprintf(f.get(), "Net Switch Power   ==    %.3e %s\n",
               summary_switch_power, summary_switch_power_percentage.c_str());

  // print internal power
  double summary_internal_power = report_summary_data.get_cell_internal_power();
  std::string summary_internal_power_percentage =
      std::string("(") +
      data_str_f(CalcPercentage(report_summary_data.get_cell_internal_power() /
                                total_power)) +
      std::string("%)");
  std::fprintf(f.get(), "Cell Internal Power   ==    %.3e %s\n",
               summary_internal_power,
               summary_internal_power_percentage.c_str());

  // print leakage power
  double summary_leakage_power = report_summary_data.get_cell_leakage_power();
  std::string summary_leakage_power_percentage =
      std::string("(") +
      data_str_f(CalcPercentage(report_summary_data.get_cell_leakage_power() /
                                total_power)) +
      std::string("%)");
  std::fprintf(f.get(), "Cell Leakage Power   ==    %.3e %s\n",
               summary_leakage_power, summary_leakage_power_percentage.c_str());

  std::fprintf(f.get(), "Total Power   ==  %.3e\n", total_power);

  LOG_INFO << "Total Power   ==  " << total_power << " mW";
  return 1;
};

/**
 * @brief report instance power
 *
 * @param rpt_file_name
 * @param pwr_analysis_mode
 * @return unsigned
 */
unsigned Power::reportInstancePower(const char* rpt_file_name,
                                    PwrAnalysisMode pwr_analysis_mode) {
  PwrReportInstance report_instance_power(rpt_file_name, pwr_analysis_mode);
  auto report_tbl =
      report_instance_power.createReportTable("Power Analysis Instance Report");

  // lambda for print power data float to string.
  auto data_str = [](double data) { return Str::printf("%.3e", data); };
  auto data_str_f = [](double data) { return Str::printf("%.3f", data); };

  PwrGroupData* group_data;
  FOREACH_PWR_GROUP_DATA(this, group_data) {
    // TODO net
    if (dynamic_cast<Net*>(group_data->get_obj())) {
      continue;
    }
    auto* inst = dynamic_cast<Instance*>(group_data->get_obj());
    (*report_tbl) << inst->get_name()
                  << data_str(group_data->get_internal_power())
                  << data_str(group_data->get_switch_power())
                  << data_str(group_data->get_leakage_power())
                  << data_str(group_data->get_total_power()) << TABLE_ENDLINE;
  };

  auto close_file = [](std::FILE* fp) { std::fclose(fp); };

  std::unique_ptr<std::FILE, decltype(close_file)> f(
      std::fopen(rpt_file_name, "w"), close_file);

  std::fprintf(f.get(), "Generate the report at %s\n", Time::getNowWallTime());

  std::map<PwrAnalysisMode, std::string> analysis_mode_to_string = {
      {PwrAnalysisMode::kAveraged, "Averaged"},
      {PwrAnalysisMode::kTimeBase, "TimeBase"},
      {PwrAnalysisMode::kClockCycle, "ClockCycle"}};

  std::fprintf(f.get(), "Report : %s Power\n ",
               analysis_mode_to_string[pwr_analysis_mode].c_str());

  std::fprintf(f.get(), "%s", report_tbl->c_str());

  return 1;
  
}

/**
 * @brief report csv file
 *
 * @param rpt_file_name
 * @return unsigned
 */
unsigned Power::reportInstancePowerCSV(const char* rpt_file_name) {
  std::ofstream csv_file(rpt_file_name);
  auto data_str = [](double data) { return Str::printf("%.3e", data); };
  PwrGroupData* group_data;
  FOREACH_PWR_GROUP_DATA(this, group_data) {
    if (dynamic_cast<Net*>(group_data->get_obj())) {
      continue;
    }
    auto* inst = dynamic_cast<Instance*>(group_data->get_obj());
    csv_file << inst->get_name() << ","
             << data_str(group_data->get_internal_power()) << ","
             << data_str(group_data->get_switch_power()) << ","
             << data_str(group_data->get_leakage_power()) << ","
             << data_str(group_data->get_total_power()) << "\n";
  }
  csv_file.close();
  return 1;

}
/**
 * @brief run report ipower
 *
 * @return unsigned
 */
unsigned Power::runCompleteFlow() {
  Sta* ista = Sta::getOrCreateSta();
  Power* ipower = Power::getOrCreatePower(&(ista->get_graph()));

  {
    ieda::Stats stats;

    // set fastest clock for default toggle
    auto* fastest_clock = ista->getFastestClock();
    ipower::PwrClock pwr_fastest_clock(fastest_clock->get_clock_name(),
                                       fastest_clock->getPeriodNs());
    // get sta clocks
    auto clocks = ista->getClocks();

    ipower->setupClock(std::move(pwr_fastest_clock), std::move(clocks));

    LOG_INFO << "build graph and seq graph start";
    // build power graph
    buildGraph();

    // build seq graph
    buildSeqGraph();

    LOG_INFO << "build graph and seq graph end";
    double memory_delta = stats.memoryDelta();
    LOG_INFO << "build graph and seq graph memory usage " << memory_delta
             << "MB";
    double time_delta = stats.elapsedRunTime();
    LOG_INFO << "build graph and seq graph time elapsed " << time_delta << "s";
  }

  {
    ieda::Stats stats;
    LOG_INFO << "power annotate vcd start";
    // std::pair begin_end = {0, 50000000};
    // readVCD("/home/taosimin/T28/vcd/asic_top.vcd", "u0_asic_top",
    //                 begin_end);
    // annotate toggle sp
    annotateToggleSP();

    LOG_INFO << "power vcd annotate end";
    double memory_delta = stats.memoryDelta();
    LOG_INFO << "power vcd annotate memory usage " << memory_delta << "MB";
    double time_delta = stats.elapsedRunTime();
    LOG_INFO << "power vcd annotate time elapsed " << time_delta << "s";
  }

  // update power.
  ipower->updatePower();

  {
    // report power.
    ieda::Stats stats;
    LOG_INFO << "power report start";

    std::string output_path = ista->get_design_work_space();
    output_path += Str::printf("/%s.pwr", ista->get_design_name().c_str());
    reportSummaryPower(output_path.c_str(), PwrAnalysisMode::kAveraged);

    LOG_INFO << "power report end";
    double memory_delta = stats.memoryDelta();
    LOG_INFO << "power report memory usage " << memory_delta << "MB";
    double time_delta = stats.elapsedRunTime();
    LOG_INFO << "power report time elapsed " << time_delta << "s";
  }
  return 1;
}

/**
 * @brief get the instance owned group.
 *
 * @param inst
 * @return std::optional<PwrGroupData::PwrGroupType>
 */
std::optional<PwrGroupData::PwrGroupType> Power::getInstPowerGroup(
    Instance* the_inst) {
  auto* lib_cell = the_inst->get_inst_cell();
  std::array<std::function<std::optional<PwrGroupData::PwrGroupType>(Instance *
                                                                     the_inst)>,
             7>
      group_prioriy_array{
          [this, lib_cell](
              Instance* the_inst) -> std::optional<PwrGroupData::PwrGroupType> {
            // judge whether io cell.
            return std::nullopt;
          },
          [this, lib_cell](
              Instance* the_inst) -> std::optional<PwrGroupData::PwrGroupType> {
            // judge whether memory.
            return std::nullopt;
          },
          [this, lib_cell](
              Instance* the_inst) -> std::optional<PwrGroupData::PwrGroupType> {
            // judge whether black box.
            return std::nullopt;
          },
          [this, lib_cell](
              Instance* the_inst) -> std::optional<PwrGroupData::PwrGroupType> {
            // judge whether clock network.
            Pin* pin;
            FOREACH_INSTANCE_PIN(the_inst, pin) {
              auto* the_pwr_vertex = _power_graph.getPowerVertex(pin);
              if (the_pwr_vertex->is_clock_network()) {
                return PwrGroupData::PwrGroupType::kClockNetwork;
              }
            }
            return std::nullopt;
          },
          [this, lib_cell](
              Instance* the_inst) -> std::optional<PwrGroupData::PwrGroupType> {
            // judge whether register.
            if (!lib_cell->isSequentialCell()) {
              return PwrGroupData::PwrGroupType::kComb;
            }
            return std::nullopt;
          },
          [this, lib_cell](
              Instance* the_inst) -> std::optional<PwrGroupData::PwrGroupType> {
            // judge whether register.
            if (lib_cell->isSequentialCell()) {
              return PwrGroupData::PwrGroupType::kSeq;
            }
            return std::nullopt;
          }};

  for (auto group_type_func : group_prioriy_array) {
    auto power_type = group_type_func(the_inst);
    if (power_type) {
      return power_type;
    }
  }
  return std::nullopt;
}

}  // namespace ipower
