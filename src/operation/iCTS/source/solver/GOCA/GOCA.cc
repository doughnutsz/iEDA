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
 * @file GOCA.cc
 * @author Dawn Li (dawnli619215645@gmail.com)
 */

#include "GOCA.hh"

#include <filesystem>
#include <ranges>

#include "BalanceClustering.hh"
#include "CtsDesign.h"
#include "CtsReport.h"
#include "TimingPropagator.hh"
#include "TreeBuilder.hh"
#include "log/Log.hh"
#include "time/Time.hh"
namespace icts {
void GOCA::run()
{
  auto assigns = globalAssign();
  // convert to inst
  std::vector<Inst*> insts;
  std::ranges::for_each(_instances, [&](CtsInstance* instance) {
    auto* inst = new Inst(instance);
    auto* load_pin = inst->get_load_pin();
    // update load pin cap
    TimingPropagator::updateCapLoad(load_pin);
    insts.push_back(inst);
  });
  _level_insts.push_back(insts);

  // clustering
  while (insts.size() > 1) {
    auto assign = _level > (int) assigns.size() ? assigns.back() : assigns[_level - 1];
    insts = assignApply(insts, assign);
    _level_insts.push_back(insts);
    std::ranges::for_each(insts, [](Inst* inst) {
      auto* load_pin = inst->get_load_pin();
      // update load pin cap
      TimingPropagator::updateCapLoad(load_pin);
    });
    ++_level;
  }
  auto* root = insts.front();
  auto origin_loc = root->get_location();
  auto* driver_pin = root->get_driver_pin();
  auto* net = driver_pin->get_net();
  auto feasible_cell = TreeBuilder::feasibleCell(root, TimingPropagator::getSkewBound());
  TreeBuilder::cancelPlace(root);
  root->set_location(origin_loc);
  root->set_cell_master(feasible_cell.back());
  TreeBuilder::place(root);
  TimingPropagator::update(net);
  TimingPropagator::initLoadPinDelay(root->get_load_pin());
  // generate clock topo
  genClockTopo();
  // report
  levelReport();
}
std::vector<Assign> GOCA::globalAssign()
{
  std::vector<Assign> global_assigns;
  auto max_net_dist = static_cast<int>(TimingPropagator::getMaxLength()) * TimingPropagator::getDbUnit();
  auto max_fanout = TimingPropagator::getMaxFanout();
  auto max_cap = TimingPropagator::getMaxCap();
  auto skew_bound = TimingPropagator::getSkewBound();
  // global_assigns.push_back({max_net_dist * 10000, max_fanout * 10000, max_cap * 10000, skew_bound * 10000, 0.98});
  // level 1 test
  global_assigns.push_back({max_net_dist * 6 / 8, max_fanout, max_cap * 1.0, skew_bound * 0.25, 0.98});
  // level 2 test
  global_assigns.push_back({max_net_dist * 6 / 8, max_fanout * 4 / 4, max_cap * 0.8, skew_bound * 0.6, 0.9});
  // level 3 test
  global_assigns.push_back({max_net_dist * 7 / 8, max_fanout * 3 / 4, max_cap * 0.7, skew_bound * 0.8, 0.9});
  // level 4 test
  global_assigns.push_back({max_net_dist * 6 / 8, max_fanout / 8, max_cap * 0.5, skew_bound * 1, 0.8});
  // level 5 test
  global_assigns.push_back({max_net_dist * 6 / 8, max_fanout / 8, max_cap * 0.5, skew_bound * 1, 0.8});
  // level 6 test
  global_assigns.push_back({max_net_dist * 6 / 8, max_fanout / 8, max_cap * 0.5, skew_bound * 1.0, 0.8});
  // global_assigns.push_back({max_net_dist, max_fanout, max_cap, skew_bound * 1.0, 0.8});
  return global_assigns;
}
std::vector<Inst*> GOCA::assignApply(const std::vector<Inst*>& insts, const Assign& assign)
{
  LOG_INFO << "Bounding HPWL: " << BalanceClustering::calcHPWL(insts) << std::endl;
  // pre-processing
  auto max_dist = assign.max_dist;
  auto max_net_len = 1.0 * max_dist / TimingPropagator::getDbUnit();
  auto max_fanout = assign.max_fanout;
  // auto max_cap = assign.max_cap;
  auto cluster_ratio = assign.ratio;
  auto skew_bound = assign.skew_bound;

  std::vector<Inst*> level_insts;
  auto target_insts = insts;
  if (_level > 1) {
    BalanceClustering::latencyOpt(insts, skew_bound, 0.7);
  }

  auto clusters = BalanceClustering::iterClustering(target_insts, max_fanout, 100, 5, cluster_ratio);
  // auto enhanced_clusters = clusters;
  auto enhanced_clusters = BalanceClustering::slackClustering(clusters, max_net_len, max_fanout);

  // enhanced_clusters = BalanceClustering::clusteringEnhancement(enhanced_clusters, max_fanout, max_cap, max_net_len);

  // BalanceClustering::writeClusterPy(enhanced_clusters, "cluster_level_" + std::to_string(_level));

  auto level_center = BalanceClustering::calcBoundCentroid(insts);
  std::ranges::for_each(enhanced_clusters, [&](const std::vector<Inst*>& cluster) {
    if (_level > 1) {
      BalanceClustering::latencyOpt(cluster, skew_bound, 0.7);
    }
    auto buf = netAssign(cluster, assign, level_center, (_level > 1 && enhanced_clusters.size() > 1) ? true : false);
    level_insts.push_back(buf);
  });
  // if (_level == 1) {
  //   std::ranges::for_each(enhanced_clusters, [&](const std::vector<Inst*>& cluster) {
  //     auto buf = netAssign(cluster, assign, level_center, false);
  //     level_insts.push_back(buf);
  //   });
  // } else {
  //   auto bound_divide = BalanceClustering::getBoundCluster(enhanced_clusters);
  //   auto shift_clusters = bound_divide.first;
  //   auto normal_clusters = bound_divide.second;
  //   std::ranges::for_each(shift_clusters, [&](const std::vector<Inst*>& cluster) {
  //     auto buf = netAssign(cluster, assign, level_center, true);
  //     level_insts.push_back(buf);
  //   });
  //   std::ranges::for_each(normal_clusters, [&](const std::vector<Inst*>& cluster) {
  //     auto buf = netAssign(cluster, assign, level_center, false);
  //     level_insts.push_back(buf);
  //   });
  // }
  return level_insts;
}
std::vector<Inst*> GOCA::topGuide(const std::vector<Inst*>& insts, const Assign& assign)
{
  auto max_dist = assign.max_dist;
  auto max_fanout = assign.max_fanout;
  auto sorted_insts = insts;
  auto est_net_dist = BalanceClustering::estimateNetLength(insts, 1.0 * max_dist / TimingPropagator::getDbUnit(), max_fanout)
                      * TimingPropagator::getDbUnit();
  while (est_net_dist > max_dist) {
    std::sort(sorted_insts.begin(), sorted_insts.end(),
              [](Inst* inst1, Inst* inst2) { return inst1->get_driver_pin()->get_max_delay() < inst2->get_driver_pin()->get_max_delay(); });
    auto min_delay_inst = sorted_insts.front();
    auto loc = min_delay_inst->get_location();
    sorted_insts.erase(sorted_insts.begin());
    auto center = BalanceClustering::calcBoundCentroid(sorted_insts);

    auto center_dist = TimingPropagator::calcDist(loc, center);
    auto shift_dist = std::min(static_cast<int64_t>(max_dist / 2), center_dist);
    auto new_loc = (center - loc) * (1.0 * shift_dist / center_dist) + loc;
    auto net_name = CTSAPIInst.toString(_net_name, "_", CTSAPIInst.genId());
    auto* buffer = TreeBuilder::genBufInst(net_name, new_loc);
    buffer->set_cell_master(TimingPropagator::getMinSizeLib()->get_cell_master());
    TreeBuilder::place(buffer);
    auto* load_pin = min_delay_inst->get_load_pin();
    auto* driver_pin = buffer->get_driver_pin();
    TreeBuilder::directConnectTree(driver_pin, load_pin);

    auto* net = TimingPropagator::genNet(net_name, driver_pin, {load_pin});
    TimingPropagator::update(net);
    _nets.push_back(net);
    est_net_dist = BalanceClustering::estimateNetLength(sorted_insts, 1.0 * max_dist / TimingPropagator::getDbUnit(), max_fanout)
                   * TimingPropagator::getDbUnit();
    sorted_insts.push_back(buffer);
  }
  return sorted_insts;
}
Inst* GOCA::netAssign(const std::vector<Inst*>& insts, const Assign& assign, const Point& level_center, const bool& shift)
{
  auto max_dist = assign.max_dist;
  auto max_fanout = assign.max_fanout;
  auto skew_bound = assign.skew_bound;
  auto center = BalanceClustering::calcBoundCentroid(insts);
  auto guide_loc = center;
  // center shift
  auto net_dist = BalanceClustering::estimateNetLength(insts, 1.0 * max_dist / TimingPropagator::getDbUnit(), max_fanout)
                  * TimingPropagator::getDbUnit();
  if (shift && net_dist <= max_dist) {
    auto center_dist = TimingPropagator::calcDist(center, level_center);
    auto shift_dist = std::min(static_cast<int64_t>(max_dist - net_dist), center_dist);
    guide_loc = center_dist > 0 ? (level_center - center) * (1.0 * shift_dist / center_dist) + center : center;
  }
  auto net_name = CTSAPIInst.toString(_net_name, "_", CTSAPIInst.genId());
  std::vector<Pin*> cluster_load_pins;
  std::ranges::for_each(insts, [&cluster_load_pins](Inst* inst) {
    auto load_pin = inst->get_load_pin();
    cluster_load_pins.push_back(load_pin);
  });
  auto* buffer = TreeBuilder::genBufInst(net_name, guide_loc);
  // set min size cell master
  buffer->set_cell_master(TimingPropagator::getMinSizeLib()->get_cell_master());
  // location legitimization
  TreeBuilder::place(buffer);
  auto* driver_pin = buffer->get_driver_pin();
  if (cluster_load_pins.size() == 1) {
    auto* load_pin = cluster_load_pins.front();
    TreeBuilder::directConnectTree(driver_pin, load_pin);
  } else {
    TreeBuilder::shallowLightTree(driver_pin, cluster_load_pins);
  }
  auto* net = TimingPropagator::genNet(net_name, driver_pin, cluster_load_pins);
  TimingPropagator::update(net);
  if (TimingPropagator::skewFeasible(driver_pin, skew_bound)) {
    _nets.push_back(net);
    return buffer;
  } else {
    auto feasible_cell = TreeBuilder::feasibleCell(buffer, skew_bound);
    if (!feasible_cell.empty()) {
      buffer->set_cell_master(feasible_cell.front());
      TimingPropagator::update(net);
      _nets.push_back(net);
      return buffer;
    }
  }
  // remove salt
  TreeBuilder::cancelPlace(buffer);
  TreeBuilder::recoverNet(net);

  // skew violation, try to opt salt
  auto* opt_salt_net = saltOpt(insts, assign);
  if (opt_salt_net) {
    _nets.push_back(opt_salt_net);
    return opt_salt_net->get_driver_pin()->get_inst();
  }

  // build DME
  auto bufs = TreeBuilder::dmeTree(_net_name, cluster_load_pins, skew_bound, guide_loc);
  CTSAPIInst.saveToLog("num: ", bufs.size());
  std::ranges::for_each(bufs, [&](Inst* inst) {
    auto* driver_pin = inst->get_driver_pin();
    auto* dme_net = driver_pin->get_net();
    CTSAPIInst.saveToLog(driver_pin->get_sub_len(), '\n');
    _nets.push_back(dme_net);
  });
  return bufs.back();
}
Net* GOCA::saltOpt(const std::vector<Inst*>& insts, const Assign& assign)
{
  struct Buffering
  {
    Point loc;
    size_t cell_id;
    double skew;
  };
  auto skew_bound = assign.skew_bound;

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();
  int max_y = std::numeric_limits<int>::min();
  std::ranges::for_each(insts, [&](Inst* inst) {
    auto loc = inst->get_location();
    min_x = std::min(min_x, loc.x());
    min_y = std::min(min_y, loc.y());
    max_x = std::max(max_x, loc.x());
    max_y = std::max(max_y, loc.y());
  });
  Point lb = Point(min_x, min_y);
  Point rb = Point(max_x, min_y);
  Point lt = Point(min_x, max_y);
  Point rt = Point(max_x, max_y);
  Point center = BalanceClustering::calcCentroid(insts);
  Point bound_center = BalanceClustering::calcBoundCentroid(insts);

  std::vector<Point> loc_list = {lb, rb, lt, rt, center, bound_center};
  auto lib_list = TimingPropagator::getDelayLibs();

  std::vector<Buffering> feasible_assign;

  std::vector<Pin*> cluster_load_pins;
  std::ranges::for_each(insts, [&cluster_load_pins](Inst* inst) {
    auto load_pin = inst->get_load_pin();
    cluster_load_pins.push_back(load_pin);
  });
  std::ranges::for_each(loc_list, [&](const Point& loc) {
    auto net_name = CTSAPIInst.toString(_net_name, "_", CTSAPIInst.genId());
    for (size_t i = 0; i < lib_list.size(); ++i) {
      auto* lib = lib_list[i];
      auto cell_master = lib->get_cell_master();
      auto* buffer = TreeBuilder::genBufInst(net_name, loc);
      auto* driver_pin = buffer->get_driver_pin();
      buffer->set_cell_master(cell_master);
      TreeBuilder::place(buffer);
      TreeBuilder::shallowLightTree(driver_pin, cluster_load_pins);
      auto* net = TimingPropagator::genNet(net_name, driver_pin, cluster_load_pins);
      TimingPropagator::update(net);
      if (TimingPropagator::skewFeasible(driver_pin, skew_bound)) {
        auto skew = TimingPropagator::calcSkew(driver_pin);
        feasible_assign.push_back({buffer->get_location(), i, skew});
      }
      TreeBuilder::cancelPlace(buffer);
      TreeBuilder::recoverNet(net);
    }
  });

  if (feasible_assign.empty()) {
    return nullptr;
  }
  std::sort(feasible_assign.begin(), feasible_assign.end(), [&](const Buffering& assign1, const Buffering& assign2) {
    if (assign1.cell_id == assign2.cell_id) {
      return assign1.skew < assign2.skew;  // sort by skew
    }
    return assign1.cell_id < assign2.cell_id;  // sort by cell size
  });
  // assign
  auto best_assign = feasible_assign.front();
  auto net_name = CTSAPIInst.toString(_net_name, "_", CTSAPIInst.genId());
  auto* buffer = TreeBuilder::genBufInst(net_name, best_assign.loc);
  auto* driver_pin = buffer->get_driver_pin();
  auto cell_master = lib_list[best_assign.cell_id]->get_cell_master();
  buffer->set_cell_master(cell_master);
  TreeBuilder::place(buffer);
  TreeBuilder::shallowLightTree(driver_pin, cluster_load_pins);
  auto* net = TimingPropagator::genNet(net_name, driver_pin, cluster_load_pins);
  TimingPropagator::update(net);
  return net;
}
void GOCA::genClockTopo()
{
  std::ranges::for_each(_nets, [&](Net* net) {
    auto* design = CTSAPIInst.get_design();
    design->addGocaNet(net);
    auto* driver_pin = net->get_driver_pin();
    auto* driver_inst = driver_pin->get_inst();
    ClockTopo topo(net->get_name());
    topo.add_driver(driver_inst->get_cts_inst());
    std::ranges::for_each(net->get_load_pins(), [&](Pin* load_pin) {
      auto* load_inst = load_pin->get_inst();
      topo.add_load(load_inst->get_cts_inst());
    });
    driver_pin->preOrder([&](Node* node) {
      auto* parent = node->get_parent();
      if (parent == nullptr) {
        return;
      }
      auto current_loc = node->get_location();
      auto parent_loc = parent->get_location();
      auto parent_name = parent->isPin() ? dynamic_cast<Pin*>(parent)->get_inst()->get_name() : parent->get_name();
      auto current_name = node->isPin() ? dynamic_cast<Pin*>(node)->get_inst()->get_name() : node->get_name();
      auto require_nake = node->get_required_snake();
      if (require_nake > 0) {
        auto require_snake = std::ceil(require_nake * TimingPropagator::getDbUnit());
        auto delta_x = std::abs(current_loc.x() - parent_loc.x());
        auto trunk_x = (parent_loc.x() + current_loc.x() + delta_x + require_snake) / 2;
        auto snake_p1 = Point(trunk_x, parent_loc.y());
        auto snake_p2 = Point(trunk_x, current_loc.y());
        if (!(CTSAPIInst.isInDie(snake_p1) && CTSAPIInst.isInDie(snake_p2))) {
          // is not in die
          trunk_x = (parent_loc.x() + current_loc.x() - delta_x - require_snake) / 2;
          snake_p1 = Point(trunk_x, parent_loc.y());
          snake_p2 = Point(trunk_x, current_loc.y());
        }
        std::vector<std::string> name_vec
            = {parent_name, "steiner_" + std::to_string(CTSAPIInst.genId()), "steiner_" + std::to_string(CTSAPIInst.genId()), current_name};
        std::vector<Point> point_vec = {parent_loc, snake_p1, snake_p2, current_loc};
        for (size_t i = 0; i < name_vec.size() - 1; ++i) {
          topo.add_signal_wire(CtsSignalWire(Endpoint(name_vec[i], point_vec[i]), Endpoint(name_vec[i + 1], point_vec[i + 1])));
        }
      } else {
        if (pgl::rectilinear(parent_loc, current_loc)) {
          topo.add_signal_wire(CtsSignalWire(Endpoint(parent_name, parent_loc), Endpoint(current_name, current_loc)));
        } else {
          auto trunk_loc = Point(parent_loc.x(), current_loc.y());
          auto trunk_name = "steiner_" + std::to_string(CTSAPIInst.genId());
          topo.add_signal_wire(CtsSignalWire(Endpoint(parent_name, parent_loc), Endpoint(trunk_name, trunk_loc)));
          topo.add_signal_wire(CtsSignalWire(Endpoint(trunk_name, trunk_loc), Endpoint(current_name, current_loc)));
        }
      }
    });
    _clock_topos.push_back(topo);
  });
}
void GOCA::writeNetPy(Pin* root, const std::string& save_name) const
{
  LOG_INFO << "Writing net to python file...";
  // write the cluster to python file
  auto* config = CTSAPIInst.get_config();
  auto path = config->get_sta_workspace();
  std::ofstream ofs(path + "/" + save_name + ".py");
  ofs << "import matplotlib.pyplot as plt" << std::endl;
  ofs << "fig = plt.figure(figsize=(8,6), dpi=300)" << std::endl;
  auto write_node = [&ofs](Node* node) {
    ofs << "x = [";
    ofs << node->get_location().x();
    ofs << "]" << std::endl;
    ofs << "y = [";
    ofs << node->get_location().y();
    ofs << "]" << std::endl;
    ofs << "plt.scatter(x, y)" << std::endl;
    ofs << "plt.text(x[0], y[0], '" << node->get_name() << "')" << std::endl;
    auto parent = node->get_parent();
    if (parent) {
      // add fly line
      ofs << "plt.plot([";
      ofs << node->get_location().x();
      ofs << ", ";
      ofs << parent->get_location().x();
      ofs << "], [";
      ofs << node->get_location().y();
      ofs << ", ";
      ofs << parent->get_location().y();
      ofs << "], color='black', linestyle='-', linewidth=0.5)" << std::endl;
    }
  };
  root->preOrder(write_node);
  ofs << "plt.show()" << std::endl;
  ofs << "plt.savefig('" + save_name + ".png')" << std::endl;
  ofs.close();
}
void GOCA::levelReport() const
{
  using Inst_Func = std::function<double(const Inst*)>;
  auto gen_level_rpt = [&](const CtsReportType& rpt_type, const std::string& rpt_tittle, const std::string& file_name,
                           Inst_Func get_val_func, Inst_Func vio_func = nullptr) {
    auto dir = CTSAPIInst.get_config()->get_sta_workspace() + "/level_log";
    if (!std::filesystem::exists(dir)) {
      std::filesystem::create_directories(dir);
    }

    auto rpt = CtsReportTable::createReportTable("Level " + rpt_tittle + " Log", rpt_type);
    for (size_t level = 1; level < _level_insts.size(); ++level) {
      double min_val = std::numeric_limits<double>::max();
      double max_val = std::numeric_limits<double>::min();
      double avg_val = 0;
      double vio_num = 0;
      auto insts = _level_insts[level];
      std::ranges::for_each(insts, [&min_val, &max_val, &avg_val, &vio_num, &get_val_func, &vio_func](const Inst* inst) {
        auto val = get_val_func(inst);
        min_val = std::min(min_val, val);
        max_val = std::max(max_val, val);
        avg_val += val;
        if (vio_func != nullptr) {
          vio_num += vio_func(inst);
        }
      });
      avg_val /= insts.size();
      if (vio_func == nullptr) {
        (*rpt) << level << insts.size() << min_val << max_val << avg_val << "None" << TABLE_ENDLINE;
      } else {
        (*rpt) << level << insts.size() << min_val << max_val << avg_val << vio_num << TABLE_ENDLINE;
      }
    }

    auto save_file_name = _net_name + "_" + file_name + ".rpt";
    auto save_path = dir + "/" + save_file_name;
    std::ofstream outfile(save_path);
    outfile << "Generate the report at " << Time::getNowWallTime() << std::endl;
    outfile << rpt->c_str();
    outfile.close();
  };
  // fanout rpt
  gen_level_rpt(
      CtsReportType::kLEVEL_FANOUT, "Fanout", "fanout",
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        auto* net = driver_pin->get_net();
        return net->getFanout();
      },
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        auto* net = driver_pin->get_net();
        return net->getFanout() > TimingPropagator::getMaxFanout();
      });
  // net len rpt
  gen_level_rpt(
      CtsReportType::kLEVEL_NET_LEN, "Net Length", "net_len",
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        return driver_pin->get_sub_len();
      },
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        return driver_pin->get_sub_len() > TimingPropagator::getMaxLength();
      });
  // cap rpt
  gen_level_rpt(
      CtsReportType::kLEVEL_CAP, "Cap", "cap",
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        return driver_pin->get_cap_load();
      },
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        return driver_pin->get_cap_load() > TimingPropagator::getMaxCap();
      });
  // slew rpt
  gen_level_rpt(
      CtsReportType::kLEVEL_SLEW, "Slew", "slew",
      [](const Inst* inst) {
        auto* load_pin = inst->get_load_pin();
        return load_pin->get_slew_in();
      },
      [](const Inst* inst) {
        auto* load_pin = inst->get_load_pin();
        return load_pin->get_slew_in() > TimingPropagator::getMaxBufTran();
      });
  // min delay rpt
  gen_level_rpt(CtsReportType::kLEVEL_DELAY, "Min Delay", "min_delay", [](const Inst* inst) {
    auto* driver_pin = inst->get_driver_pin();
    return driver_pin->get_min_delay();
  });
  // max delay rpt
  gen_level_rpt(CtsReportType::kLEVEL_DELAY, "Max Delay", "max_delay", [](const Inst* inst) {
    auto* driver_pin = inst->get_driver_pin();
    return driver_pin->get_max_delay();
  });
  // insert delay rpt
  gen_level_rpt(CtsReportType::kLEVEL_INSERT_DELAY, "Insert Delay", "insert_delay",
                [](const Inst* inst) { return inst->get_insert_delay(); });
  // skew rpt
  gen_level_rpt(
      CtsReportType::kLEVEL_SKEW, "Skew", "skew",
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        return driver_pin->get_max_delay() - driver_pin->get_min_delay();
      },
      [](const Inst* inst) {
        auto* driver_pin = inst->get_driver_pin();
        return !TimingPropagator::skewFeasible(driver_pin);
      });
}
}  // namespace icts