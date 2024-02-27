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
 * @file Sta.cc
 * @author simin tao (taosm@pcl.ac.cn)
 * @brief The implemention of the top Sta class.
 * @version 0.1
 * @date 2020-11-27
 */

#include "Sta.hh"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <tuple>
#include <utility>

#include "StaAnalyze.hh"
#include "StaApplySdc.hh"
#include "StaBuildClockTree.hh"
#include "StaBuildGraph.hh"
#include "StaBuildPropTag.hh"
#include "StaBuildRCTree.hh"
#include "StaCheck.hh"
#include "StaClockPropagation.hh"
#include "StaConstPropagation.hh"
#include "StaCrossTalkPropagation.hh"
#include "StaDataPropagation.hh"
#include "StaDelayPropagation.hh"
#include "StaDump.hh"
#include "StaFindStartOrEnd.hh"
#include "StaGraph.hh"
#include "StaLevelization.hh"
#include "StaPathData.hh"
#include "StaReport.hh"
#include "StaSlewPropagation.hh"
#include "ThreadPool/ThreadPool.h"
#include "include/Version.hh"
#include "json/json.hpp"
#include "liberty/Liberty.hh"
#include "log/Log.hh"
#include "netlist/NetlistWriter.hh"
#include "netlist/Pin.hh"
#include "sdc-cmd/Cmd.hh"
#include "sdc/SdcConstrain.hh"
#include "tcl/ScriptEngine.hh"
#include "time/Time.hh"
#include "usage/usage.hh"

// // Swig uses C linkage for init functions.
// extern "C" {
// extern int Ista_Init(Tcl_Interp *interp);
// }

namespace ista {

Sta *Sta::_sta = nullptr;

Sta::Sta()
    : _num_threads(32),
      _constrains(nullptr),
      _analysis_mode(AnalysisMode::kMaxMin),
      _graph(&_netlist),
      _clock_groups(sta_clock_cmp) {
  _report_tbl_summary = StaReportPathSummary::createReportTable("sta");
  _report_tbl_TNS = StaReportClockTNS::createReportTable("TNS");
}

Sta::~Sta() = default;

/**
 * @brief Get the top sta instance, if not, create one.
 *
 * @return Sta*
 */
Sta *Sta::getOrCreateSta() {
  static std::mutex mt;
  if (_sta == nullptr) {
    std::lock_guard<std::mutex> lock(mt);
    if (_sta == nullptr) {
      _sta = new Sta();
    }
  }
  return _sta;
}

/**
 * @brief Destroy the sta.
 *
 */
void Sta::destroySta() {
  delete _sta;
  _sta = nullptr;
}

/**
 * @brief set sta report path.
 *
 * @param design_work_space
 */
void Sta::set_design_work_space(const char *design_work_space) {
  _design_work_space = design_work_space;
}

/**
 * @brief Get the constrains, if not, create one.
 *
 * @return sdcConstrain* The constrains.
 */
SdcConstrain *Sta::getConstrain() {
  if (!_constrains) {
    _constrains = std::make_unique<SdcConstrain>();
  }

  return _constrains.get();
}

// /**
//  * @brief Init the script engine for sdc tcl file interpretion.
//  *
//  */
// void Sta::initScriptEngine() {
//   ScriptEngine *the_script_engine = ScriptEngine::getOrCreateInstance();
//   Tcl_Interp *the_interp = the_script_engine->get_interp();

//   Tcl_Init(the_interp);
//   // registe the swig tcl cmd
//   Ista_Init(the_interp);

//   // we direct source the tcl file now, in the future we need
//   // encode the tcl file to code memory TODO fix me
//   int result = the_script_engine->evalScriptFile(
//       "/home/smtao/peda/iEDA/src/iSTA/cmd/sdc.tcl");
//   DLOG_FATAL_IF(result == TCL_ERROR);

//   result = the_script_engine->evalString("namespace import ista::*");
//   DLOG_FATAL_IF(result == TCL_ERROR);
// }

/**
 * @brief Read the design verilog file.
 *
 * @param verilog_file
 * @return unsigned
 */
unsigned Sta::readDesign(const char *verilog_file) {
  readVerilog(verilog_file);
  auto &top_module_name = get_top_module_name();
  linkDesign(top_module_name.c_str());
  return 1;
}

unsigned Sta::readDesignWithRustParser(const char *file_name) { return 1; }

/**
 * @brief read the sdc file.
 *
 * @param sdc_file
 * @return unsigned
 */
unsigned Sta::readSdc(const char *sdc_file) {
  LOG_INFO << "read sdc " << sdc_file << " start ";
  Sta::initSdcCmd();

  _constrains.reset();
  getConstrain();

  auto *script_engine = ScriptEngine::getOrCreateInstance();
  unsigned result =
      script_engine->evalString(Str::printf("source %s", sdc_file));

  LOG_FATAL_IF(result == 1)
      << ScriptEngine::getOrCreateInstance()->evalString(R"(puts $errorInfo)");

  LOG_INFO << "read sdc end";

  return 1;
}

/**
 * @brief read spef file.
 *
 * @param spef_file
 * @return unsigned
 */
unsigned Sta::readSpef(const char *spef_file) {
  StaGraph &the_graph = get_graph();

  StaBuildRCTree func(spef_file, DelayCalcMethod::kElmore);
  func(&the_graph);

  return 1;
}

/**
 * @brief read one aocv file.
 *
 * @param aocv_file
 * @return unsigned
 */
unsigned Sta::readAocv(const char *aocv_file) {
  AocvReader aocv_reader(aocv_file);
  auto load_aocv = aocv_reader.readAocvLibrary();
  addAocv(std::move(load_aocv));
  return 1;
}

/**
 * @brief read aocv files.
 *
 * @param aocv_files
 * @return unsigned
 */
unsigned Sta::readAocv(std::vector<std::string> &aocv_files) {
  LOG_INFO << "load aocv start";

#if 0
  for (auto &aocv_file : aocv_files) {
    readAocv(aocv_file.c_str());
  }

#else

  {
    ThreadPool pool(get_num_threads());

    for (auto &aocv_file : aocv_files) {
      pool.enqueue([this, aocv_file]() { readAocv(aocv_file.c_str()); });
    }
  }

#endif

  LOG_INFO << "load aocv end";

  return 1;
}

/**
 * @brief read one lib file.
 *
 * @param lib_file
 * @return unsigned
 */
unsigned Sta::readLiberty(const char *lib_file) {
  Liberty lib;
  auto load_lib = lib.loadLibertyWithRustParser(lib_file);
  addLib(std::move(load_lib));

  return 1;
}

/**
 * @brief read liberty files.
 *
 * @param lib_files
 * @return unsigned
 */
unsigned Sta::readLiberty(std::vector<std::string> &lib_files) {
  LOG_INFO << "load lib start";

#if 0
  for (auto &lib_file : lib_files) {
    readLiberty(lib_file.c_str());
  }

#else

  {
    ThreadPool pool(get_num_threads());

    for (auto &lib_file : lib_files) {
      pool.enqueue([this, lib_file]() { readLiberty(lib_file.c_str()); });
    }
  }

#endif

  LOG_INFO << "load lib end";

  return 1;
}

/**
 * @brief Read the verilog file.
 *
 * @param verilog_file
 */
void Sta::readVerilogWithRustParser(const char *verilog_file) {
  LOG_INFO << "read verilog file " << verilog_file << " start";
  bool is_ok = _rust_verilog_reader.readVerilog(verilog_file);
  _rust_top_module = _rust_verilog_reader.get_top_module();
  LOG_FATAL_IF(!is_ok) << "read verilog file " << verilog_file << " failed.";
  LOG_INFO << "read verilog end";
}

/**
 * @brief Read the verilog file.
 *
 * @param verilog_file
 */
void Sta::readVerilog(const char *verilog_file) {
  LOG_INFO << "read verilog file " << verilog_file << " start";
  bool is_ok = _verilog_reader.read(verilog_file);
  LOG_FATAL_IF(!is_ok) << "read verilog file " << verilog_file << " failed.";

  LOG_INFO << "read verilog end";
}

/**
 * @brief find the module.
 *
 * @param module_name
 * @return VerilogModule*
 */
VerilogModule *Sta::findModule(const char *module_name) {
  auto found_module = std::find_if(
      std::begin(_verilog_modules), std::end(_verilog_modules),
      [=](std::unique_ptr<VerilogModule> &verilog_module) -> bool {
        return Str::equal(verilog_module->get_module_name(), module_name);
      });

  if (found_module != _verilog_modules.end()) {
    return found_module->get();
  }
  return nullptr;
}

/**
 * @brief Link the design file to design netlist .
 *
 */
void Sta::linkDesign(const char *top_cell_name) {
  LOG_INFO << "link design " << top_cell_name << " start";

  _verilog_reader.flattenModule(top_cell_name);
  auto &verilog_modules = _verilog_reader.get_verilog_modules();
  _verilog_modules = std::move(verilog_modules);

  _top_module = findModule(top_cell_name);
  LOG_FATAL_IF(!_top_module) << "top module not found.";
  set_design_name(_top_module->get_module_name());

  auto &top_module_stmts = _top_module->get_module_stmts();
  Netlist &design_netlist = _netlist;
  design_netlist.set_name(_top_module->get_module_name());

  /*The verilog decalre statement process lookup table.*/
  std::map<VerilogDcl::DclType,
           std::function<DesignObject *(VerilogDcl::DclType, const char *)>>
      dcl_process = {
          {VerilogDcl::DclType::kInput,
           [&design_netlist](VerilogDcl::DclType dcl_type,
                             const char *dcl_name) {
             Port in_port(dcl_name, PortDir::kIn);
             auto &ret_val = design_netlist.addPort(std::move(in_port));
             return &ret_val;
           }},
          {VerilogDcl::DclType::kOutput,
           [&design_netlist](VerilogDcl::DclType dcl_type,
                             const char *dcl_name) {
             Port out_port(dcl_name, PortDir::kOut);
             auto &ret_val = design_netlist.addPort(std::move(out_port));
             return &ret_val;
           }},
          {VerilogDcl::DclType::kInout,
           [&design_netlist](VerilogDcl::DclType dcl_type,
                             const char *dcl_name) {
             Port out_port(dcl_name, PortDir::kInOut);
             auto &ret_val = design_netlist.addPort(std::move(out_port));
             return &ret_val;
           }},
          {VerilogDcl::DclType::kWire,
           [&design_netlist](VerilogDcl::DclType dcl_type,
                             const char *dcl_name) {
             Net net(dcl_name);
             auto &ret_val = design_netlist.addNet(std::move(net));
             return &ret_val;
           }}};

  /*process the verilog declare statement.*/
  auto process_dcl_stmt = [&dcl_process, &design_netlist](auto *dcl_stmt) {
    auto dcl_type = dcl_stmt->get_dcl_type();
    const auto *dcl_name = dcl_stmt->get_dcl_name();
    auto dcl_range = dcl_stmt->get_range();

    if (!dcl_range) {
      if (dcl_process.contains(dcl_type)) {
        dcl_process[dcl_type](dcl_type, dcl_name);
      } else {
        LOG_INFO << "not support the declaration " << dcl_name;
      }
    } else {
      auto bus_range = *(dcl_range);
      for (int index = bus_range.second; index <= bus_range.first; index++) {
        // for port or wire bus, we split to one bye one port.
        const char *one_name = Str::printf("%s[%d]", dcl_name, index);
        if (dcl_process.contains(dcl_type)) {
          auto *design_obj = dcl_process[dcl_type](dcl_type, one_name);
          if (design_obj->isPort()) {
            auto *port = dynamic_cast<Port *>(design_obj);
            if (index == bus_range.second) {
              unsigned bus_size = bus_range.first + 1;
              PortBus port_bus(dcl_name, bus_range.first, bus_range.second,
                               bus_size, port->get_port_dir());
              port_bus.addPort(index, port);
              auto &ret_val = design_netlist.addPortBus(std::move(port_bus));
              port->set_port_bus(&ret_val);
            } else {
              auto *found_port_bus = design_netlist.findPortBus(dcl_name);
              found_port_bus->addPort(index, port);
              port->set_port_bus(found_port_bus);
            }
          }

        } else {
          LOG_INFO << "not support the declaration " << one_name;
        }
      }
    }
  };

  for (auto &stmt : top_module_stmts) {
    if (stmt->isVerilogDclsStmt()) {
      auto *dcls_stmt = dynamic_cast<VerilogDcls *>(stmt.get());
      auto &dcls = dcls_stmt->get_verilog_dcls();
      for (auto &dcl_stmt : dcls) {
        process_dcl_stmt(dcl_stmt.get());
      }
    } else if (stmt->isVerilogDclStmt()) {
      auto *dcl_stmt = dynamic_cast<VerilogDcl *>(stmt.get());
      process_dcl_stmt(dcl_stmt);
    } else if (stmt->isModuleInstStmt()) {
      auto *inst_stmt = dynamic_cast<VerilogInst *>(stmt.get());
      const char *liberty_cell_name = inst_stmt->get_cell_name();
      auto &port_connections = inst_stmt->get_port_connections();
      const char *inst_name = inst_stmt->get_inst_name();

      auto *inst_cell = findLibertyCell(liberty_cell_name);
      if (!inst_cell) {
        LOG_INFO << "liberty cell " << liberty_cell_name << " is not exist.";
        continue;
      }

      Instance inst(inst_name, inst_cell);

      /*lambda function create net for connect instance pin*/
      auto create_net_connection = [inst_stmt, inst_cell, &inst,
                                    &design_netlist](auto *cell_port_id,
                                                     auto *net_expr,
                                                     std::optional<int> index,
                                                     PinBus *pin_bus) {
        const char *cell_port_name = cell_port_id->getName();

        auto *library_port_or_port_bus =
            inst_cell->get_cell_port_or_port_bus(cell_port_name);
        LOG_INFO_IF(!library_port_or_port_bus)
            << cell_port_name << " port is not found.";
        if (!library_port_or_port_bus) {
          return;
        }

        auto add_pin_to_net = [&design_netlist](Pin *inst_pin,
                                                std::string &net_name) {
          if (net_name.empty()) {
            return;
          }

          Net *the_net = design_netlist.findNet(net_name.c_str());
          if (the_net) {
            the_net->addPinPort(inst_pin);
          } else {
            // DLOG_INFO << "create net " << net_name;
            auto &created_net = design_netlist.addNet(Net(net_name.c_str()));

            created_net.addPinPort(inst_pin);
            the_net = &created_net;
          }
          // The same name port is default connect to net.
          if (auto *design_port = design_netlist.findPort(net_name.c_str());
              design_port && !the_net->isNetPinPort(design_port)) {
            the_net->addPinPort(design_port);
          }
        };

        auto add_pin_to_inst = [&inst, &add_pin_to_net, pin_bus](
                                   auto *pin_name, auto *library_port,
                                   std::optional<int> pin_index) -> Pin * {
          auto *inst_pin = inst.addPin(pin_name, library_port);
          if (pin_bus) {
            pin_bus->addPin(pin_index.value(), inst_pin);
          }

          return inst_pin;
        };

        LibertyPort *library_port = nullptr;
        std::string pin_name;
        std::string net_name;

        if (net_expr) {
          if (net_expr->isIDExpr()) {
            auto *net_id = net_expr->get_verilog_id();
            LOG_FATAL_IF(!net_id)
                << "The port connection " << cell_port_id->getName()
                << " net id is not exist "
                << "at line " << inst_stmt->get_line();
            net_name = net_id->getName();
            // fix net name contain backslash
            net_name = Str::trimBackslash(net_name);
          } else if (net_expr->isConstant()) {
            LOG_INFO_FIRST_N(5) << "for the constant net need TODO.";
          }
        }

        if (!library_port_or_port_bus->isLibertyPortBus()) {
          library_port = dynamic_cast<LibertyPort *>(library_port_or_port_bus);
          pin_name = cell_port_name;
          auto *inst_pin =
              add_pin_to_inst(pin_name.c_str(), library_port, std::nullopt);

          add_pin_to_net(inst_pin, net_name);

        } else {
          auto *library_port_bus =
              dynamic_cast<LibertyPortBus *>(library_port_or_port_bus);
          if (index) {
            library_port = (*library_port_bus)[index.value()];
            pin_name = Str::printf("%s[%d]", cell_port_name, index.value());
            auto *inst_pin =
                add_pin_to_inst(pin_name.c_str(), library_port, index.value());

            add_pin_to_net(inst_pin, net_name);

          } else {
            for (size_t i = 0; i < library_port_bus->getBusSize(); ++i) {
              library_port = (*library_port_bus)[i];
              pin_name = Str::printf("%s[%d]", cell_port_name, i);
              auto *inst_pin =
                  add_pin_to_inst(pin_name.c_str(), library_port, i);

              std::string net_index_name;
              if (net_expr->get_verilog_id()->isBusSliceID()) {
                auto *net_slice_id =
                    dynamic_cast<VerilogSliceID *>(net_expr->get_verilog_id());

                net_index_name =
                    net_slice_id->getName(i + net_slice_id->get_range_base());
              } else {
                net_index_name = Str::printf("%s[%d]", net_name.c_str(), i);
              }

              add_pin_to_net(inst_pin, net_index_name);
            }
          }
        }
      };

      /*lambda function flatten concate net, which maybe nested.*/
      std::function<void(VerilogNetConcatExpr *,
                         std::vector<VerilogNetExpr *> &)>
          flatten_concat_net_expr =
              [&flatten_concat_net_expr](
                  VerilogNetConcatExpr *net_concat_expr,
                  std::vector<VerilogNetExpr *> &net_concat_vec) {
                auto &verilog_id_concat =
                    net_concat_expr->get_verilog_id_concat();
                for (auto &verilog_id_net_expr : verilog_id_concat) {
                  if (verilog_id_net_expr->isConcatExpr()) {
                    flatten_concat_net_expr(
                        dynamic_cast<VerilogNetConcatExpr *>(
                            verilog_id_net_expr.get()),
                        net_concat_vec);
                  } else {
                    net_concat_vec.push_back(verilog_id_net_expr.get());
                  }
                }
              };

      // create net
      for (auto &port_connection : port_connections) {
        LOG_FATAL_IF(!port_connection)
            << "The inst " << inst_name << " at line " << inst_stmt->get_line()
            << " port connection is null";
        auto *cell_port_id = port_connection->get_port_id();
        auto *net_expr = port_connection->get_net_expr();

        // create pin bus
        const char *cell_port_name = cell_port_id->getName();
        auto *library_port_bus =
            inst_cell->get_cell_port_or_port_bus(cell_port_name);
        std::unique_ptr<PinBus> pin_bus;
        if (library_port_bus->isLibertyPortBus()) {
          auto bus_size =
              dynamic_cast<LibertyPortBus *>(library_port_bus)->getBusSize();
          pin_bus = std::make_unique<PinBus>(cell_port_name, bus_size - 1, 0,
                                             bus_size);
        }

        if (!net_expr || net_expr->isIDExpr() || net_expr->isConstant()) {
          create_net_connection(cell_port_id, net_expr, std::nullopt,
                                pin_bus.get());
        } else {
          LOG_FATAL_IF(!pin_bus) << "pin bus is null.";

          auto *net_concat_expr =
              (dynamic_cast<VerilogNetConcatExpr *>(net_expr));
          std::vector<VerilogNetExpr *> verilog_id_concat_vec;
          flatten_concat_net_expr(net_concat_expr, verilog_id_concat_vec);

          for (int i = (verilog_id_concat_vec.size() - 1);
               auto *verilog_id_net_expr : verilog_id_concat_vec) {
            create_net_connection(cell_port_id, verilog_id_net_expr, i--,
                                  pin_bus.get());
          }
        }

        if (pin_bus) {
          inst.addPinBus(std::move(pin_bus));
        }
      }

      design_netlist.addInstance(std::move(inst));
    }
  }

  LOG_INFO << "link design " << top_cell_name << " end";
}

/**
 * @brief Link the design file to design netlist use rust parser.
 *
 * @param top_cell_name
 */
void Sta::linkDesignWithRustParser() {
  const char *top_cell_name = _rust_top_module->module_name;
  auto top_module_stmts = _rust_top_module->module_stmts;
  // auto port_list = _rust_top_module->port_list;

  LOG_INFO << "link design " << top_cell_name << " start";
  Netlist &design_netlist = _netlist;
  design_netlist.set_name(top_cell_name);

  /*The verilog decalre statement process lookup table.*/
  std::map<DclType, std::function<DesignObject *(DclType, const char *)>>
      dcl_process = {
          {DclType::KInput,
           [&design_netlist](DclType dcl_type, const char *dcl_name) {
             Port in_port(dcl_name, PortDir::kIn);
             auto &ret_val = design_netlist.addPort(std::move(in_port));
             return &ret_val;
           }},
          {DclType::KOutput,
           [&design_netlist](DclType dcl_type, const char *dcl_name) {
             Port out_port(dcl_name, PortDir::kOut);
             auto &ret_val = design_netlist.addPort(std::move(out_port));
             return &ret_val;
           }},
          {DclType::KInout,
           [&design_netlist](DclType dcl_type, const char *dcl_name) {
             Port out_port(dcl_name, PortDir::kInOut);
             auto &ret_val = design_netlist.addPort(std::move(out_port));
             return &ret_val;
           }},
          {DclType::KWire,
           [&design_netlist](DclType dcl_type, const char *dcl_name) {
             dcl_name = Str::trimmed(dcl_name);
             Net net(dcl_name);
             auto &ret_val = design_netlist.addNet(std::move(net));
             return &ret_val;
           }}};

  /*process the verilog declare statement.*/
  auto process_dcl_stmt = [&dcl_process,
                           &design_netlist](auto *rust_verilog_dcl) {
    auto dcl_type = rust_verilog_dcl->dcl_type;
    const auto *dcl_name = rust_verilog_dcl->dcl_name;
    auto dcl_range = rust_verilog_dcl->range;

    if (!dcl_range.has_value) {
      if (dcl_process.contains(dcl_type)) {
        dcl_process[dcl_type](dcl_type, dcl_name);
      } else {
        LOG_INFO << "not support the declaration " << dcl_name;
      }
    } else {
      auto bus_range = std::make_pair(dcl_range.start, dcl_range.end);
      for (int index = bus_range.second; index <= bus_range.first; index++) {
        // for port or wire bus, we split to one bye one port.
        const char *one_name = Str::printf("%s[%d]", dcl_name, index);

        if (dcl_process.contains(dcl_type)) {
          auto *design_obj = dcl_process[dcl_type](dcl_type, one_name);
          if (design_obj->isPort()) {
            auto *port = dynamic_cast<Port *>(design_obj);
            if (index == bus_range.second) {
              unsigned bus_size = bus_range.first + 1;
              PortBus port_bus(dcl_name, bus_range.first, bus_range.second,
                               bus_size, port->get_port_dir());
              port_bus.addPort(index, port);
              auto &ret_val = design_netlist.addPortBus(std::move(port_bus));
              port->set_port_bus(&ret_val);
            } else {
              auto *found_port_bus = design_netlist.findPortBus(dcl_name);
              found_port_bus->addPort(index, port);
              port->set_port_bus(found_port_bus);
            }
          }

        } else {
          LOG_INFO << "not support the declaration " << one_name;
        }
      }
    }
  };

  void *stmt;
  FOREACH_VEC_ELEM(&top_module_stmts, void, stmt) {
    if (rust_is_verilog_dcls_stmt(stmt)) {
      RustVerilogDcls *verilog_dcls_struct = rust_convert_verilog_dcls(stmt);
      auto verilog_dcls = verilog_dcls_struct->verilog_dcls;
      void *verilog_dcl = nullptr;
      FOREACH_VEC_ELEM(&verilog_dcls, void, verilog_dcl) {
        process_dcl_stmt(rust_convert_verilog_dcl(verilog_dcl));
      }
    } else if (rust_is_module_inst_stmt(stmt)) {
      RustVerilogInst *verilog_inst = rust_convert_verilog_inst(stmt);
      const char *inst_name = verilog_inst->inst_name;
      inst_name = Str::trimmed(inst_name);

      const char *liberty_cell_name = verilog_inst->cell_name;
      auto port_connections = verilog_inst->port_connections;

      auto *inst_cell = findLibertyCell(liberty_cell_name);

      if (!inst_cell) {
        LOG_INFO << "liberty cell " << liberty_cell_name << " is not exist.";
        continue;
      }

      Instance inst(inst_name, inst_cell);

      /*lambda function create net for connect instance pin*/
      auto create_net_connection = [verilog_inst, inst_cell, &inst,
                                    &design_netlist](auto *cell_port_id,
                                                     auto *net_expr,
                                                     std::optional<int> index,
                                                     PinBus *pin_bus) {
        const char *cell_port_name;
        if (rust_is_id(cell_port_id)) {
          cell_port_name = rust_convert_verilog_id(cell_port_id)->id;
        } else if (rust_is_bus_index_id(cell_port_id)) {
          cell_port_name = rust_convert_verilog_index_id(cell_port_id)->id;
        } else {
          cell_port_name = rust_convert_verilog_slice_id(cell_port_id)->id;
        }

        auto *library_port_or_port_bus =
            inst_cell->get_cell_port_or_port_bus(cell_port_name);
        LOG_INFO_IF(!library_port_or_port_bus)
            << cell_port_name << " port is not found.";
        if (!library_port_or_port_bus) {
          return;
        }

        auto add_pin_to_net = [&design_netlist](Pin *inst_pin,
                                                std::string &net_name) {
          if (net_name.empty()) {
            return;
          }

          Net *the_net = design_netlist.findNet(net_name.c_str());
          if (the_net) {
            the_net->addPinPort(inst_pin);
          } else {
            // DLOG_INFO << "create net " << net_name;
            auto &created_net = design_netlist.addNet(Net(net_name.c_str()));

            created_net.addPinPort(inst_pin);
            the_net = &created_net;
          }
          // The same name port is default connect to net.
          if (auto *design_port = design_netlist.findPort(net_name.c_str());
              design_port && !the_net->isNetPinPort(design_port)) {
            the_net->addPinPort(design_port);
          }
        };

        auto add_pin_to_inst = [&inst, &add_pin_to_net, pin_bus](
                                   auto *pin_name, auto *library_port,
                                   std::optional<int> pin_index) -> Pin * {
          auto *inst_pin = inst.addPin(pin_name, library_port);
          if (pin_bus) {
            pin_bus->addPin(pin_index.value(), inst_pin);
          }

          return inst_pin;
        };

        LibertyPort *library_port = nullptr;
        std::string pin_name;
        std::string net_name;

        if (net_expr) {
          if (rust_is_id_expr(net_expr)) {
            auto *net_id = const_cast<void *>(
                rust_convert_verilog_net_id_expr(net_expr)->verilog_id);
            LOG_FATAL_IF(!net_id) << "The port connection " << cell_port_name
                                  << " net id is not exist "
                                  << "at line " << verilog_inst->line_no;

            if (rust_is_id(net_id)) {
              net_name = rust_convert_verilog_id(net_id)->id;
            } else if (rust_is_bus_index_id(net_id)) {
              net_name = rust_convert_verilog_index_id(net_id)->id;
            } else {
              net_name = rust_convert_verilog_slice_id(net_id)->id;
            }
            // fix net name contain backslash
            net_name = Str::trimBackslash(net_name);
            net_name = Str::trimmed(net_name.c_str());
          } else if (rust_is_constant(net_expr)) {
            LOG_INFO_FIRST_N(5) << "for the constant net need TODO.";
          }
        }

        if (!library_port_or_port_bus->isLibertyPortBus()) {
          library_port = dynamic_cast<LibertyPort *>(library_port_or_port_bus);
          pin_name = cell_port_name;
          auto *inst_pin =
              add_pin_to_inst(pin_name.c_str(), library_port, std::nullopt);

          add_pin_to_net(inst_pin, net_name);

        } else {
          auto *library_port_bus =
              dynamic_cast<LibertyPortBus *>(library_port_or_port_bus);
          if (index) {
            library_port = (*library_port_bus)[index.value()];
            pin_name = Str::printf("%s[%d]", cell_port_name, index.value());
            auto *inst_pin =
                add_pin_to_inst(pin_name.c_str(), library_port, index.value());

            add_pin_to_net(inst_pin, net_name);

          } else {
            for (size_t i = 0; i < library_port_bus->getBusSize(); ++i) {
              library_port = (*library_port_bus)[i];
              pin_name = Str::printf("%s[%d]", cell_port_name, i);
              auto *inst_pin =
                  add_pin_to_inst(pin_name.c_str(), library_port, i);

              std::string net_index_name;
              if (rust_is_bus_slice_id(const_cast<void *>(
                      rust_convert_verilog_net_id_expr(net_expr)
                          ->verilog_id)) ||
                  rust_is_bus_slice_id(const_cast<void *>(
                      rust_convert_verilog_constant_expr(net_expr)
                          ->verilog_id))) {
                void *verilog_id = nullptr;
                if (rust_is_id_expr(net_expr)) {
                  verilog_id = const_cast<void *>(
                      rust_convert_verilog_net_id_expr(net_expr)->verilog_id);
                } else if (rust_is_constant(net_expr)) {
                  verilog_id = const_cast<void *>(
                      rust_convert_verilog_constant_expr(net_expr)->verilog_id);
                }
                auto *net_slice_id = rust_convert_verilog_slice_id(verilog_id);

                net_index_name = rust_get_index_name(
                    net_slice_id, i + net_slice_id->range_base);
              } else {
                net_index_name = Str::printf("%s[%d]", net_name.c_str(), i);
              }

              add_pin_to_net(inst_pin, net_index_name);
            }
          }
        }
      };

      /*lambda function flatten concate net, which maybe nested.*/
      std::function<void(RustVerilogNetConcatExpr *, std::vector<void *> &)>
          flatten_concat_net_expr =
              [&flatten_concat_net_expr](
                  RustVerilogNetConcatExpr *net_concat_expr,
                  std::vector<void *> &net_concat_vec) {
                auto verilog_id_concat = net_concat_expr->verilog_id_concat;

                void *verilog_id;
                FOREACH_VEC_ELEM(&verilog_id_concat, void, verilog_id) {
                  if (rust_is_concat_expr(verilog_id)) {
                    flatten_concat_net_expr(
                        rust_convert_verilog_net_concat_expr(verilog_id),
                        net_concat_vec);
                  } else {
                    net_concat_vec.push_back(verilog_id);
                  }
                }
              };

      // create net
      void *port_connection;
      FOREACH_VEC_ELEM(&port_connections, void, port_connection) {
        LOG_FATAL_IF(!port_connection)
            << "The inst " << inst_name << " at line " << verilog_inst->line_no
            << " port connection is null";
        RustVerilogPortRefPortConnect *rust_port_connection =
            rust_convert_verilog_port_ref_port_connect(port_connection);
        // *const c_void
        void *cell_port_id = const_cast<void *>(rust_port_connection->port_id);
        // *mut c_void
        void *net_expr = rust_port_connection->net_expr;

        // create pin bus
        const char *cell_port_name;
        if (rust_is_id(cell_port_id)) {
          cell_port_name = rust_convert_verilog_id(cell_port_id)->id;
        } else if (rust_is_bus_index_id(cell_port_id)) {
          cell_port_name = rust_convert_verilog_index_id(cell_port_id)->id;
        } else {
          cell_port_name = rust_convert_verilog_slice_id(cell_port_id)->id;
        }

        auto *library_port_bus =
            inst_cell->get_cell_port_or_port_bus(cell_port_name);
        std::unique_ptr<PinBus> pin_bus;
        if (library_port_bus->isLibertyPortBus()) {
          auto bus_size =
              dynamic_cast<LibertyPortBus *>(library_port_bus)->getBusSize();
          pin_bus = std::make_unique<PinBus>(cell_port_name, bus_size - 1, 0,
                                             bus_size);
        }

        if (!net_expr || rust_is_id_expr(net_expr) ||
            rust_is_constant(net_expr)) {
          create_net_connection(cell_port_id, net_expr, std::nullopt,
                                pin_bus.get());
        } else {
          LOG_FATAL_IF(!pin_bus) << "pin bus is null.";
          auto *net_concat_expr =
              rust_convert_verilog_net_concat_expr(net_expr);

          std::vector<void *> verilog_id_concat_vec;
          flatten_concat_net_expr(net_concat_expr, verilog_id_concat_vec);

          for (int i = (verilog_id_concat_vec.size() - 1);
               auto *verilog_id_net_expr : verilog_id_concat_vec) {
            create_net_connection(cell_port_id, verilog_id_net_expr, i--,
                                  pin_bus.get());
          }
        }

        if (pin_bus) {
          inst.addPinBus(std::move(pin_bus));
        }
      }

      design_netlist.addInstance(std::move(inst));
    }
  }

  LOG_INFO << "link design " << top_cell_name << " end";
}

/**
 * @brief reset constraint.
 */
void Sta::resetConstraint() { _constrains.reset(); }

/**
 * @brief Find the liberty cell from the lib.
 *
 * @param cell_name
 * @return LibertyCell*
 */
LibertyCell *Sta::findLibertyCell(const char *cell_name) {
  LibertyCell *found_cell = nullptr;
  for (auto &lib : _libs) {
    if (found_cell = lib->findCell(cell_name); found_cell) {
      break;
    }
  }
  return found_cell;
}

/**
 * @brief Find the data aocv object spec from the aocv.
 *
 * @param cell_name
 * @return std::optional<AocvObjectSpecSet *>
 */
std::optional<AocvObjectSpecSet *> Sta::findDataAocvObjectSpecSet(
    const char *object_name) {
  std::optional<AocvObjectSpecSet *> aocv_objectspec_set = std::nullopt;
  for (auto &aocv : _aocvs) {
    if (aocv_objectspec_set = aocv->findDataAocvObjectSpecSet(object_name);
        aocv_objectspec_set) {
      break;
    }
  }
  return aocv_objectspec_set;
}

/**
 * @brief Find the clock aocv object spec from the aocv.
 *
 * @param cell_name
 * @return std::optional<AocvObjectSpecSet *>
 */
std::optional<AocvObjectSpecSet *> Sta::findClockAocvObjectSpecSet(
    const char *object_name) {
  std::optional<AocvObjectSpecSet *> aocv_objectspec_set = std::nullopt;
  for (auto &aocv : _aocvs) {
    if (aocv_objectspec_set = aocv->findClockAocvObjectSpecSet(object_name);
        aocv_objectspec_set) {
      break;
    }
  }
  return aocv_objectspec_set;
}

/**
 * @brief Make the function equivalently liberty cell map.
 *
 * @param equiv_libs
 */
void Sta::makeEquivCells(std::vector<LibertyLibrary *> &equiv_libs) {
  if (_equiv_cells) {
    _equiv_cells.reset();
  }

  _equiv_cells = std::make_unique<LibertyClassifyCell>();
  _equiv_cells->classifyLibCell(equiv_libs);
}

/**
 * @brief Get the equivalently liberty cell.
 *
 * @param cell
 * @return Vector<LibertyCell *>*
 */
Vector<LibertyCell *> *Sta::equivCells(LibertyCell *cell) {
  if (_equiv_cells)
    return _equiv_cells->getClassOfCell(cell);
  else
    return nullptr;
}

/**
 * @brief Init all supported ista cmd.
 *
 */
void Sta::initSdcCmd() {
  auto cmd_create_clock = std::make_unique<CmdCreateClock>("create_clock");
  LOG_FATAL_IF(!cmd_create_clock);
  TclCmds::addTclCmd(std::move(cmd_create_clock));

  auto cmd_create_generated_clock =
      std::make_unique<CmdCreateGeneratedClock>("create_generated_clock");
  LOG_FATAL_IF(!cmd_create_generated_clock);
  TclCmds::addTclCmd(std::move(cmd_create_generated_clock));

  auto cmd_set_input_transition =
      std::make_unique<CmdSetInputTransition>("set_input_transition");
  LOG_FATAL_IF(!cmd_set_input_transition);
  TclCmds::addTclCmd(std::move(cmd_set_input_transition));

  auto cmd_set_driving_cell =
      std::make_unique<CmdSetDrivingCell>("set_driving_cell");
  LOG_FATAL_IF(!cmd_set_driving_cell);
  TclCmds::addTclCmd(std::move(cmd_set_driving_cell));

  auto cmd_set_load = std::make_unique<CmdSetLoad>("set_load");
  LOG_FATAL_IF(!cmd_set_load);
  TclCmds::addTclCmd(std::move(cmd_set_load));

  auto cmd_set_input_delay =
      std::make_unique<CmdSetInputDelay>("set_input_delay");
  LOG_FATAL_IF(!cmd_set_input_delay);
  TclCmds::addTclCmd(std::move(cmd_set_input_delay));

  auto cmd_set_output_delay =
      std::make_unique<CmdSetOutputDelay>("set_output_delay");
  LOG_FATAL_IF(!cmd_set_output_delay);
  TclCmds::addTclCmd(std::move(cmd_set_output_delay));

  auto cmd_set_max_fanout = std::make_unique<CmdSetMaxFanout>("set_max_fanout");
  LOG_FATAL_IF(!cmd_set_max_fanout);
  TclCmds::addTclCmd(std::move(cmd_set_max_fanout));

  auto cmd_set_max_transition =
      std::make_unique<CmdSetMaxTransition>("set_max_transition");
  LOG_FATAL_IF(!cmd_set_max_transition);
  TclCmds::addTclCmd(std::move(cmd_set_max_transition));

  auto cmd_set_max_capacitance =
      std::make_unique<CmdSetMaxCapacitance>("set_max_capacitance");
  LOG_FATAL_IF(!cmd_set_max_capacitance);
  TclCmds::addTclCmd(std::move(cmd_set_max_capacitance));

  auto cmd_current_design =
      std::make_unique<CmdCurrentDesign>("current_design");
  LOG_FATAL_IF(!cmd_current_design);
  TclCmds::addTclCmd(std::move(cmd_current_design));

  auto get_clocks = std::make_unique<CmdGetClocks>("get_clocks");
  LOG_FATAL_IF(!get_clocks);
  TclCmds::addTclCmd(std::move(get_clocks));

  auto get_pins = std::make_unique<CmdGetPins>("get_pins");
  LOG_FATAL_IF(!get_pins);
  TclCmds::addTclCmd(std::move(get_pins));

  auto get_ports = std::make_unique<CmdGetPorts>("get_ports");
  LOG_FATAL_IF(!get_ports);
  TclCmds::addTclCmd(std::move(get_ports));

  auto all_clocks = std::make_unique<CmdAllClocks>("all_clocks");
  LOG_FATAL_IF(!all_clocks);
  TclCmds::addTclCmd(std::move(all_clocks));

  auto all_inputs = std::make_unique<CmdAllInputs>("all_inputs");
  LOG_FATAL_IF(!all_inputs);
  TclCmds::addTclCmd(std::move(all_inputs));

  auto all_outputs = std::make_unique<CmdAllOutputs>("all_outputs");
  LOG_FATAL_IF(!all_outputs);
  TclCmds::addTclCmd(std::move(all_outputs));

  auto set_propagated_clock =
      std::make_unique<CmdSetPropagatedClock>("set_propagated_clock");
  LOG_FATAL_IF(!set_propagated_clock);
  TclCmds::addTclCmd(std::move(set_propagated_clock));

  auto set_clock_groups =
      std::make_unique<CmdSetClockGroups>("set_clock_groups");
  LOG_FATAL_IF(!set_clock_groups);
  TclCmds::addTclCmd(std::move(set_clock_groups));

  auto set_multicycle_path =
      std::make_unique<CmdSetMulticyclePath>("set_multicycle_path");
  LOG_FATAL_IF(!set_multicycle_path);
  TclCmds::addTclCmd(std::move(set_multicycle_path));

  auto set_timing_derate =
      std::make_unique<CmdSetTimingDerate>("set_timing_derate");
  LOG_FATAL_IF(!set_timing_derate);
  TclCmds::addTclCmd(std::move(set_timing_derate));

  auto set_clock_uncertainty =
      std::make_unique<CmdSetClockUncertainty>("set_clock_uncertainty");
  LOG_FATAL_IF(!set_clock_uncertainty);
  TclCmds::addTclCmd(std::move(set_clock_uncertainty));

  auto set_units = std::make_unique<CmdSetUnits>("set_units");
  LOG_FATAL_IF(!set_units);
  TclCmds::addTclCmd(std::move(set_units));
}

/**
 * @brief Get the sta clock accord the clock name.
 *
 * @param clock_name
 * @return StaClock*
 */
StaClock *Sta::findClock(const char *clock_name) {
  for (auto &clock : _clocks) {
    if (Str::equal(clock->get_clock_name(), clock_name)) {
      return clock.get();
    }
  }
  return nullptr;
}

/**
 * @brief get the fastest clock.
 *
 * @return StaClock*
 */
StaClock *Sta::getFastestClock() {
  StaClock *fastest_clock = nullptr;
  std::ranges::for_each(_clocks, [&fastest_clock](auto &clock) {
    if (!fastest_clock) {
      fastest_clock = clock.get();
    } else {
      if (fastest_clock->get_period() > clock->get_period()) {
        fastest_clock = clock.get();
      }
    }
  });
  return fastest_clock;
}

/**
 * @brief set all clock latency, such as all to zero.
 *
 * @param latency
 */
void Sta::setIdealClockNetworkLatency(double latency) {
  for (auto &clock : _clocks) {
    clock->set_ideal_clock_network_latency(NS_TO_PS(latency));
  }
}

/**
 * @brief set some clock latency.
 *
 * @param clock_name
 * @param latency
 */
void Sta::setIdealClockNetworkLatency(const char *clock_name, double latency) {
  auto *sta_clock = findClock(clock_name);
  if (sta_clock) {
    sta_clock->set_ideal_clock_network_latency(latency);
  }
}

/**
 * @brief Add io delay constrain.
 *
 * @param port_vertex
 * @param io_delay_constrain
 */
void Sta::addIODelayConstrain(StaVertex *port_vertex,
                              SdcSetIODelay *io_delay_constrain) {
  _io_delays.insert(port_vertex, io_delay_constrain);
}

/**
 * @brief Get the io delay constrain of port vertex.
 *
 * @param port_vertex
 * @return std::list<SdcSetIODelay*>
 */
std::list<SdcSetIODelay *> Sta::getIODelayConstrain(StaVertex *port_vertex) {
  return _io_delays.values(port_vertex);
}

/**
 * @brief Find the vertex.
 *
 * @param pin_name
 * @return StaVertex*
 */
StaVertex *Sta::findVertex(const char *pin_name) {
  auto objs = _netlist.findObj(pin_name, false, false);
  if (!objs.empty()) {
    auto *obj = objs.front();
    auto the_vertex = _graph.findVertex(obj);
    return the_vertex ? *the_vertex : nullptr;
  }
  return nullptr;
}

/**
 * @brief Get the vertex slew limit.
 *
 * @param vertex
 * @param mode
 * @param trans_type
 * @return double
 */
std::optional<double> Sta::getVertexSlewLimit(StaVertex *the_vertex,
                                              AnalysisMode mode,
                                              TransType trans_type) {
  std::optional<double> limit;
  std::optional<double> slew_limit;
  if (auto *obj = the_vertex->get_design_obj(); obj->isPin()) {
    auto *port = dynamic_cast<Pin *>(obj)->get_cell_port();
    slew_limit = port->get_port_slew_limit(mode);
    if (!slew_limit) {
      slew_limit =
          port->get_ower_cell()->get_owner_lib()->get_default_max_transition();
    }
  }

  auto global_slew_limit =
      (trans_type == TransType::kRise) ? getMaxRiseSlew() : getMaxFallSlew();

  auto vertex_slew_limit = (trans_type == TransType::kRise)
                               ? the_vertex->getMaxRiseSlew()
                               : the_vertex->getMaxFallSlew();

  if (vertex_slew_limit) {
    limit = vertex_slew_limit;
  } else if (global_slew_limit) {
    limit = global_slew_limit;
  } else if (slew_limit) {
    limit = slew_limit;
  }

  return limit;
}

/**
 * @brief Get the vertex slew slack.
 *
 * @param the_vertex
 * @param mode
 * @param trans_type
 * @return std::optional<double>
 */
std::optional<double> Sta::getVertexSlewSlack(StaVertex *the_vertex,
                                              AnalysisMode mode,
                                              TransType trans_type) {
  std::optional<double> slack;

  auto slew = the_vertex->getSlewNs(mode, trans_type);
  auto limit = getVertexSlewLimit(the_vertex, mode, trans_type);

  if (limit && slew) {
    slack = *limit - *slew;
  }

  return slack;
}

/**
 * @brief Get the vertex cap limit.
 *
 * @param the_vertex
 * @param mode
 * @param trans_type
 * @return std::optional<double>
 */
std::optional<double> Sta::getVertexCapacitanceLimit(StaVertex *the_vertex,
                                                     AnalysisMode mode,
                                                     TransType trans_type) {
  std::optional<double> limit;
  std::optional<double> cap_limit;
  if (auto *obj = the_vertex->get_design_obj(); obj->isPin()) {
    auto *port = dynamic_cast<Pin *>(obj)->get_cell_port();
    cap_limit = port->get_port_cap_limit(mode);
  }

  auto global_cap_limit =
      (trans_type == TransType::kRise) ? getMaxRiseCap() : getMaxFallCap();

  auto vertex_cap_limit = (trans_type == TransType::kRise)
                              ? the_vertex->getMaxRiseCap()
                              : the_vertex->getMaxFallCap();
  if (vertex_cap_limit) {
    limit = vertex_cap_limit;
  } else if (global_cap_limit) {
    limit = global_cap_limit;
  } else if (cap_limit) {
    limit = cap_limit;
  }

  return limit;
}

/**
 * @brief Get the driver vertex capacitance.
 *
 * @param the_vertex
 * @param mode
 * @param trans_type
 * @return double
 */
double Sta::getVertexCapacitance(StaVertex *the_vertex, AnalysisMode mode,
                                 TransType trans_type) {
  double capacitance = 0.0;

  auto get_cap = [mode, trans_type](auto *obj) {
    double capacitance = 0.0;
    if (obj->isPin()) {
      auto *port = dynamic_cast<Pin *>(obj)->get_cell_port();
      auto port_cap = port->get_port_cap(mode, trans_type);

      if (!port_cap) {
        capacitance = port->get_port_cap();
      } else {
        capacitance = *port_cap;
      }
    } else if (obj->isPort()) {
      double port_cap = dynamic_cast<Port *>(obj)->cap(mode, trans_type);
      capacitance = port_cap;
    }
    return capacitance;
  };
  auto *obj = the_vertex->get_design_obj();
  if (obj->isPin() && obj->isOutput()) {
    auto *pin = dynamic_cast<Pin *>(obj);
    if (auto *net = pin->get_net(); net) {
      auto loads = net->getLoads();
      for (auto *load_pin : loads) {
        capacitance += get_cap(load_pin);
      }
    }

  } else {
    capacitance = get_cap(obj);
  }
  return capacitance;
}

/**
 * @brief GEt the vertex cap slack.
 *
 * @param the_vertex
 * @param mode
 * @param trans_type
 * @return std::optional<double>
 */
std::optional<double> Sta::getVertexCapacitanceSlack(StaVertex *the_vertex,
                                                     AnalysisMode mode,
                                                     TransType trans_type) {
  std::optional<double> slack;
  // only consider driver cap.
  double capacitance = getVertexCapacitance(the_vertex, mode, trans_type);
  auto limit = getVertexCapacitanceLimit(the_vertex, mode, trans_type);

  if (limit) {
    slack = *limit - capacitance;
  }

  return slack;
}

/**
 * @brief Get the driver fanout limit.
 *
 * @param the_vertex
 * @param mode
 * @param trans_type
 * @return std::optional<double>
 */
std::optional<double> Sta::getDriverVertexFanoutLimit(StaVertex *the_vertex,
                                                      AnalysisMode mode) {
  std::optional<double> limit;
  std::optional<double> fanout_limit;
  if (auto *obj = the_vertex->get_design_obj();
      obj->isPin() && obj->isOutput()) {
    auto *port = dynamic_cast<Pin *>(obj)->get_cell_port();

    fanout_limit =
        port->get_ower_cell()->get_owner_lib()->get_default_max_fanout();
  }

  auto global_fanout_limit = getMaxFanout();
  auto vertex_fanout_limit = the_vertex->getMaxFanout();

  if (vertex_fanout_limit) {
    limit = vertex_fanout_limit;
  } else if (global_fanout_limit) {
    limit = global_fanout_limit;
  } else if (fanout_limit) {
    limit = fanout_limit;
  }

  return limit;
}

/**
 * @brief Get the driver vertex fanout limit.
 *
 * @param the_vertex
 * @param mode
 * @return std::optional<double>
 */
std::optional<double> Sta::getDriverVertexFanoutSlack(StaVertex *the_vertex,
                                                      AnalysisMode mode) {
  std::optional<double> slack;
  auto *obj = the_vertex->get_design_obj();
  if ((obj->isPin() && obj->isOutput()) || (obj->isPort() && obj->isInput())) {
    auto fanout = the_vertex->get_src_arcs().size();
    auto limit = getDriverVertexFanoutLimit(the_vertex, mode);

    if (limit) {
      slack = *limit - static_cast<double>(fanout);
    }
  }
  return slack;
}

/**
 * @brief build the graph data.
 *
 * @return unsigned
 */
unsigned Sta::buildGraph() {
  StaGraph &the_graph = get_graph();
  Vector<std::function<unsigned(StaGraph *)>> funcs = {StaBuildGraph()};
  for (auto &func : funcs) {
    the_graph.exec(func);
  }

  return 1;
}

/**
 * @brief Insert the seq path data.
 *
 */
unsigned Sta::insertPathData(StaClock *capture_clock, StaVertex *end_vertex,
                             StaSeqPathData *seq_data) {
  auto p = _clock_groups.find(capture_clock);
  if (p == _clock_groups.end()) {
    auto seq_path_group = std::make_unique<StaSeqPathGroup>(capture_clock);
    seq_path_group->insertPathData(end_vertex, seq_data);
    _clock_groups[capture_clock] = std::move(seq_path_group);
  } else {
    p->second->insertPathData(end_vertex, seq_data);
  }

  return 1;
}

unsigned Sta::insertPathData(StaVertex *end_vertex,
                             StaClockGatePathData *clock_gate_data) {
  if (!_clock_gate_group) {
    const char *clock_group = "**clock_gating_default**";
    _clock_gate_group = std::make_unique<StaClockGatePathGroup>(clock_group);
  }
  _clock_gate_group->insertPathData(end_vertex, clock_gate_data);
  return 1;
}

/**
 * @brief set the report froms tos build tag.
 *
 * @param prop_froms
 */
void Sta::setReportSpec(std::vector<std::string> &&prop_froms,
                        std::vector<std::string> &&prop_tos) {
  StaReportSpec report_spec;
  report_spec.set_prop_froms(std::move(prop_froms));
  report_spec.set_prop_tos(std::move(prop_tos));
  set_report_spec(std::move(report_spec));
}

/**
 * @brief set the report froms through to build tag.
 *
 * @param prop_froms
 */
void Sta::setReportSpec(std::vector<std::string> &&prop_froms,
                        std::vector<StaReportSpec::ReportList> &&prop_throughs,
                        std::vector<std::string> &&prop_tos) {
  StaReportSpec report_spec;
  report_spec.set_prop_froms(std::move(prop_froms));
  report_spec.set_prop_throughs(std::move(prop_throughs));
  report_spec.set_prop_tos(std::move(prop_tos));

  set_report_spec(std::move(report_spec));
}

/**
 * @brief Report path in text file.
 *
 * @param rpt_file_name The report text file name.
 * @return unsigned 1 if success, 0 else fail.
 */
unsigned Sta::reportPath(const char *rpt_file_name, bool is_derate /*=true*/) {
  auto report_path =
      [this](StaReportPathSummary &report_path_func) -> unsigned {
    unsigned is_ok = 1;

    auto path_group = get_path_group();  // specify path group.

    for (auto &&[capture_clock, seq_path_group] : _clock_groups) {
      if (!path_group ||
          path_group.value() == capture_clock->get_clock_name()) {
        is_ok = report_path_func(seq_path_group.get());
        if (!is_ok) {
          break;
        }
      }
    }

    if (_clock_gate_group) {
      is_ok = report_path_func(_clock_gate_group.get());
    }

    return is_ok;
  };

  auto report_path_of_mode = [&report_path, this, rpt_file_name,
                              is_derate](AnalysisMode mode) -> unsigned {
    unsigned is_ok = 1;
    if ((get_analysis_mode() == mode) ||
        (get_analysis_mode() == AnalysisMode::kMaxMin)) {
      unsigned n_worst = get_n_worst_path_per_clock();

      StaReportPathSummary report_path_summary(rpt_file_name, mode, n_worst);
      report_path_summary.set_significant_digits(get_significant_digits());

      StaReportPathDetail report_path_detail(rpt_file_name, mode, n_worst,
                                             is_derate);
      report_path_detail.set_significant_digits(get_significant_digits());

      StaReportClockTNS report_path_TNS(rpt_file_name, mode, 1);
      report_path_TNS.set_significant_digits(get_significant_digits());

      std::vector<StaReportPathSummary *> report_funcs{
          &report_path_summary, &report_path_detail, &report_path_TNS};

      // StaReportPathDump report_path_dump(rpt_file_name, mode, n_worst);
      StaReportPathYaml report_path_dump(rpt_file_name, mode, n_worst);

      if (c_print_delay_yaml) {
        report_funcs.emplace_back(&report_path_dump);
      }

      for (auto *report_fun : report_funcs) {
        is_ok = report_path(*report_fun);
      }
    }

    return is_ok;
  };

  unsigned is_ok = report_path_of_mode(AnalysisMode::kMax);
  is_ok &= report_path_of_mode(AnalysisMode::kMin);

  if (!is_ok) {
    return is_ok;
  }

  LOG_INFO << "\n" << _report_tbl_summary->c_str();
  LOG_INFO << "\n" << _report_tbl_TNS->c_str();

  auto close_file = [](std::FILE *fp) { std::fclose(fp); };

  std::unique_ptr<std::FILE, decltype(close_file)> f(
      std::fopen(rpt_file_name, "w"), close_file);

  std::fprintf(f.get(), "Generate the report at %s, GitVersion: %s.\n",
               Time::getNowWallTime(), GIT_VERSION);
  std::fprintf(f.get(), "%s", _report_tbl_summary->c_str());  // WNS
  // report_TNS;
  std::fprintf(f.get(), "%s", _report_tbl_TNS->c_str());

  for (auto &report_tbl_detail : _report_tbl_details) {
    std::fprintf(f.get(), "%s", report_tbl_detail->c_str());
  }

  return 1;
}

/**
 * @brief report trans slack.
 *
 * @param rpt_file_name
 * @return unsigned
 */
unsigned Sta::reportTrans(const char *rpt_file_name) {
  unsigned n_worst = get_n_worst_path_per_clock();
  StaReportTrans report_trans(rpt_file_name, AnalysisMode::kMax, n_worst);
  report_trans(this);

  return 1;
}

/**
 * @brief report cap slack.
 *
 * @param rpt_file_name
 * @return unsigned
 */
unsigned Sta::reportCap(const char *rpt_file_name, bool is_clock_cap) {
  unsigned n_worst = get_n_worst_path_per_clock();
  StaReportCap report_cap(rpt_file_name, AnalysisMode::kMax, n_worst,
                          is_clock_cap);
  report_cap(this);

  return 1;
}

/**
 * @brief report fanout slack.
 *
 * @param rpt_file_name
 * @return unsigned
 */
unsigned Sta::reportFanout(const char *rpt_file_name) {
  unsigned n_worst = get_n_worst_path_per_clock();
  StaReportFanout report_fanout(rpt_file_name, AnalysisMode::kMax, n_worst);
  report_fanout(this);

  return 1;
}

/**
 * @brief report the path skew.
 *
 * @param rpt_file_name
 * @param analysis_mode
 * @return unsigned
 */
unsigned Sta::reportSkew(const char *rpt_file_name,
                         AnalysisMode analysis_mode) {
  unsigned n_worst = get_n_worst_path_per_clock();

  StaReportSkewSummary report_skew_summary(rpt_file_name, analysis_mode,
                                           n_worst);
  report_skew_summary(this);

  StaReportSkewDetail report_skew_detail(rpt_file_name, analysis_mode, n_worst);
  report_skew_detail(this);

  auto close_file = [](std::FILE *fp) { std::fclose(fp); };

  std::unique_ptr<std::FILE, decltype(close_file)> f(
      std::fopen(rpt_file_name, "w"), close_file);

  std::fprintf(f.get(), "Generate the report at %s, GitVersion: %s.\n",
               Time::getNowWallTime(), GIT_VERSION);

  for (auto &report_tbl_skew : report_skew_summary.get_report_path_skews()) {
    std::fprintf(f.get(), "Clock: %s\n", report_tbl_skew->get_tbl_name());
    std::fprintf(f.get(), "%s", report_tbl_skew->c_str());
  }

  for (auto &report_tbl_skew : report_skew_detail.get_report_path_skews()) {
    std::fprintf(f.get(), "%s", report_tbl_skew->c_str());
  }

  return 1;
}

/**
 * @brief report the specify path.
 *
 * @param rpt_file_name
 * @param analysis_mode
 * @param from_pin
 * @param through_pin
 * @param to_pin
 * @return unsigned
 */
unsigned Sta::reportFromThroughTo(const char *rpt_file_name,
                                  AnalysisMode analysis_mode,
                                  const char *from_pin, const char *through_pin,
                                  const char *to_pin) {
  StaReportSpecifyPath report_specify_path(rpt_file_name, analysis_mode,
                                           from_pin, through_pin, to_pin);
  report_specify_path(this);

  auto close_file = [](std::FILE *fp) { std::fclose(fp); };

  std::unique_ptr<std::FILE, decltype(close_file)> f(
      std::fopen(rpt_file_name, "w"), close_file);

  std::fprintf(f.get(), "Generate the report at %s, GitVersion: %s.\n",
               Time::getNowWallTime(), GIT_VERSION);

  for (auto &report_tbl_detail : _report_tbl_details) {
    std::fprintf(f.get(), "%s", report_tbl_detail->c_str());
  }

  return 1;
}

/**
 * @brief Get the end vertex seq data.
 *
 * @param delay_data
 * @return StaSeqPathData*
 */
std::vector<StaSeqPathData *> Sta::getSeqData(StaVertex *vertex,
                                              StaData *delay_data) {
  std::vector<StaSeqPathData *> seq_data_vec;
  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end = seq_path_group->findPathEndData(vertex);
    if (path_end) {
      StaPathData *path_data =
          path_end->findPathData(dynamic_cast<StaPathDelayData *>(delay_data));
      if (path_data) {
        auto *seq_data = dynamic_cast<StaSeqPathData *>(path_data);
        seq_data_vec.emplace_back(seq_data);
      }
    }
  }

  return seq_data_vec;
}

/**
 * @brief Get WNS.
 *
 * @param clock_name
 * @param mode
 * @param trans_type
 * @return double
 */
double Sta::getWNS(const char *clock_name, AnalysisMode mode) {
  double WNS = 0;

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    if (Str::equal(clk->get_clock_name(), clock_name)) {
      auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
        int left_slack = left->getSlack();
        int right_slack = right->getSlack();
        return left_slack > right_slack;
      };

      std::priority_queue<StaPathData *, std::vector<StaPathData *>,
                          decltype(cmp)>
          seq_data_queue(cmp);

      StaPathEnd *path_end;
      StaPathData *path_data;
      FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end)
      FOREACH_PATH_END_DATA(path_end, mode, path_data) {
        seq_data_queue.push(path_data);
      }
      auto *worst_seq_data = seq_data_queue.top();
      WNS = FS_TO_NS(worst_seq_data->getSlack());
      break;
    }
  }
  return WNS;
}

/**
 * @brief Get TNS.
 *
 * @param clock_name
 * @param mode
 * @param trans_type
 * @return double TNS.
 */
double Sta::getTNS(const char *clock_name, AnalysisMode mode) {
  auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
    int left_slack = left->getSlack();
    int right_slack = right->getSlack();
    return left_slack > right_slack;
  };

  std::priority_queue<StaPathData *, std::vector<StaPathData *>, decltype(cmp)>
      seq_data_queue(cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    if (Str::equal(clk->get_clock_name(), clock_name)) {
      StaPathEnd *path_end;
      StaPathData *path_data;
      FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end)
      FOREACH_PATH_END_DATA(path_end, mode, path_data) {
        seq_data_queue.push(path_data);
      }
    }
  }

  double TNS = 0;
  while (!seq_data_queue.empty()) {
    auto *seq_path_data = dynamic_cast<StaSeqPathData *>(seq_data_queue.top());

    if (seq_path_data->getSlack() < 0) {
      TNS += FS_TO_NS(seq_path_data->getSlack());

    } else {
      break;
    }
    seq_data_queue.pop();
  }

  return TNS;
}

/**
 * @brief get local skew.
 *
 * @param clock_name
 * @param mode
 * @param trans_type
 * @return double local_skew.
 */
double Sta::getLocalSkew(const char *clock_name, AnalysisMode mode,
                         TransType trans_type) {
  auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
    int left_skew = left->getSkew();
    int right_skew = right->getSkew();
    return left_skew > right_skew;
  };

  std::priority_queue<StaPathData *, std::vector<StaPathData *>, decltype(cmp)>
      seq_data_queue(cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    if (Str::equal(clk->get_clock_name(), clock_name)) {
      StaPathEnd *path_end;
      StaPathData *path_data;
      FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end)
      FOREACH_PATH_END_DATA(path_end, mode, path_data) {
        seq_data_queue.push(path_data);
      }
    }
  }

  double local_skew = 0;
  while (!seq_data_queue.empty()) {
    auto *seq_path_data = dynamic_cast<StaSeqPathData *>(seq_data_queue.top());
    local_skew += FS_TO_NS(seq_path_data->getSkew());

    seq_data_queue.pop();
  }

  return local_skew;
}

/**
 * @brief get global skew.
 *
 * @param mode
 * @param trans_type
 * @return double global skew.
 */
double Sta::getGlobalSkew(AnalysisMode mode, TransType trans_type) {
  auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
    int left_skew = left->getSkew();
    int right_skew = right->getSkew();
    return left_skew > right_skew;
  };

  std::priority_queue<StaPathData *, std::vector<StaPathData *>, decltype(cmp)>
      seq_data_queue(cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end)
    FOREACH_PATH_END_DATA(path_end, mode, path_data) {
      seq_data_queue.push(path_data);
    }
  }

  double global_skew = 0;
  while (!seq_data_queue.empty()) {
    auto *seq_path_data = dynamic_cast<StaSeqPathData *>(seq_data_queue.top());
    global_skew += FS_TO_NS(seq_path_data->getSkew());

    seq_data_queue.pop();
  }

  return global_skew;
}

/**
 * @brief get max skew of the flip-flop's output pin.
 *
 * @param mode
 * @param trans_type
 * @return std::map<StaVertex *, int>
 */
std::map<StaVertex *, int> Sta::getFFMaxSkew(AnalysisMode mode,
                                             TransType trans_type) {
  std::multimap<StaVertex *, StaSeqPathData *> vertex2pathdata;
  std::map<StaVertex *, int> vertex2maxskew;

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end)
    FOREACH_PATH_END_DATA(path_end, mode, path_data) {
      auto *seq_path_data = dynamic_cast<StaSeqPathData *>(path_data);
      auto path_delay_data = seq_path_data->getPathDelayData();
      StaPathDelayData *prior_path_delay_data = path_delay_data.top();
      StaVertex *driver_vertex = prior_path_delay_data->get_own_vertex();
      vertex2pathdata.insert(std::pair<StaVertex *, StaSeqPathData *>(
          driver_vertex, seq_path_data));
    }
  }

  std::multimap<StaVertex *, StaSeqPathData *>::iterator iter;
  for (iter = vertex2pathdata.begin(); iter != vertex2pathdata.end();) {
    auto cur_vertex = iter->first;
    int max_skew = 0;

    int num = vertex2pathdata.count(cur_vertex);

    for (int i = 0; i < num; i++) {
      int skew = iter->second->getSkew();
      if (max_skew < skew) {
        max_skew = skew;
      }
      iter++;
    }

    vertex2maxskew[cur_vertex] = max_skew;
  }

  return vertex2maxskew;
}

/**
 * @brief get total skew of the flip-flop's output pin.
 *
 * @param mode
 * @param trans_type
 * @return std::map<StaVertex *, int>
 */
std::map<StaVertex *, int> Sta::getFFTotalSkew(AnalysisMode mode,
                                               TransType trans_type) {
  std::multimap<StaVertex *, StaSeqPathData *> vertex2pathdata;
  std::map<StaVertex *, int> vertex2totalskew;

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end)
    FOREACH_PATH_END_DATA(path_end, mode, path_data) {
      auto *seq_path_data = dynamic_cast<StaSeqPathData *>(path_data);
      auto path_delay_data = seq_path_data->getPathDelayData();
      StaPathDelayData *prior_path_delay_data = path_delay_data.top();
      StaVertex *driver_vertex = prior_path_delay_data->get_own_vertex();
      vertex2pathdata.insert(std::pair<StaVertex *, StaSeqPathData *>(
          driver_vertex, seq_path_data));
    }
  }

  std::multimap<StaVertex *, StaSeqPathData *>::iterator iter;
  for (iter = vertex2pathdata.begin(); iter != vertex2pathdata.end();) {
    auto cur_vertex = iter->first;
    int total_skew = 0;

    int num = vertex2pathdata.count(cur_vertex);

    for (int i = 0; i < num; i++) {
      int skew = iter->second->getSkew();
      total_skew += skew;
      iter++;
    }

    vertex2totalskew[cur_vertex] = total_skew;
  }

  return vertex2totalskew;
}

/**
 * @brief get the related sinks of the skew.
 *
 * @param mode
 * @param trans_type
 * @return std::multimap<std::string,std::string>
 */
std::multimap<std::string, std::string> Sta::getSkewRelatedSink(
    AnalysisMode mode, TransType trans_type) {
  std::multimap<std::string, std::string> driver2ends;

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end)
    FOREACH_PATH_END_DATA(path_end, mode, path_data) {
      auto *seq_path_data = dynamic_cast<StaSeqPathData *>(path_data);
      auto path_delay_data = seq_path_data->getPathDelayData();
      StaPathDelayData *prior_path_delay_data = path_delay_data.top();
      StaVertex *driver_vertex = prior_path_delay_data->get_own_vertex();
      StaVertex *end_vertex = path_end->get_end_vertex();
      driver2ends.insert(std::pair<std::string, std::string>(
          driver_vertex->getName(), end_vertex->getName()));
    }
  }

  return driver2ends;
}

/**
 * @brief Get the end vertex path data.
 *
 * @param vertex
 * @param mode
 * @param trans_type
 * @return StaSeqPathData*
 */
StaSeqPathData *Sta::getWorstSeqData(std::optional<StaVertex *> vertex,
                                     AnalysisMode mode, TransType trans_type) {
  auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
    int left_slack = left->getSlack();
    int right_slack = right->getSlack();
    return left_slack > right_slack;
  };

  std::priority_queue<StaPathData *, std::vector<StaPathData *>, decltype(cmp)>
      seq_data_queue(cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end) {
      if (!vertex || path_end->get_end_vertex() == *vertex) {
        FOREACH_PATH_END_DATA(path_end, mode, path_data) {
          seq_data_queue.push(path_data);
        }
      }
    }
  }

  StaSeqPathData *seq_path_data = nullptr;
  while (!seq_data_queue.empty()) {
    seq_path_data = dynamic_cast<StaSeqPathData *>(seq_data_queue.top());

    if (seq_path_data->get_delay_data()->get_trans_type() == trans_type) {
      break;
    }
    seq_data_queue.pop();
  }

  return seq_path_data;
}

/**
 * @brief Get the worst slack path.
 *
 * @param mode
 * @param trans_type
 * @return StaSeqPathData*
 */
StaSeqPathData *Sta::getWorstSeqData(AnalysisMode mode, TransType trans_type) {
  return getWorstSeqData(std::nullopt, mode, trans_type);
}

/**
 * @brief obtain the start_end_slack tuples of the top n voilated timing
 * path(slack_n<slack_(n-1)<...<slack_1).
 *
 * @param top_n
 * @param mode
 * @param trans_type
 * @return std::vector<std::tuple<std::string, std::string, double>>
 */
std::vector<std::tuple<std::string, std::string, double>>
Sta::getStartEndSlackPairsOfTopNPaths(int top_n, AnalysisMode mode,
                                      TransType trans_type) {
  auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
    int left_slack = left->getSlack();
    int right_slack = right->getSlack();
    return left_slack > right_slack;
  };

  std::priority_queue<StaPathData *, std::vector<StaPathData *>, decltype(cmp)>
      seq_data_queue(cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end) {
      FOREACH_PATH_END_DATA(path_end, mode, path_data) {
        seq_data_queue.push(path_data);
      }
    }
  }

  StaSeqPathData *seq_path_data = nullptr;
  std::vector<std::tuple<std::string, std::string, double>> start_end_slacks;
  while (!seq_data_queue.empty() && top_n > 0) {
    seq_path_data = dynamic_cast<StaSeqPathData *>(seq_data_queue.top());

    if (seq_path_data->get_delay_data()->get_trans_type() == trans_type) {
      auto start_pin_name =
          seq_path_data->getPathDelayData().top()->get_own_vertex()->getName();
      auto end_pin_name =
          seq_path_data->get_delay_data()->get_own_vertex()->getName();
      double slack = seq_path_data->getSlackNs();
      if (slack >= 0) {
        break;
      }
      start_end_slacks.push_back(
          std::make_tuple(start_pin_name, end_pin_name, slack));

      --top_n;
    }
    seq_data_queue.pop();
  }

  return start_end_slacks;
}

/**
 * @brief obtain the start_end_slack tuples of the top percentage voilated
 * timing path(slack_n<slack_(n-1)<...<slack_1).
 *
 * @param top_percentage
 * @param mode
 * @param trans_type
 * @return std::vector<std::tuple<std::string, std::string, double>>
 */
std::vector<std::tuple<std::string, std::string, double>>
Sta::getStartEndSlackPairsOfTopNPercentPaths(double top_percentage,
                                             AnalysisMode mode,
                                             TransType trans_type) {
  auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
    int left_slack = left->getSlack();
    int right_slack = right->getSlack();
    return left_slack > right_slack;
  };

  std::priority_queue<StaPathData *, std::vector<StaPathData *>, decltype(cmp)>
      seq_data_queue(cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end) {
      FOREACH_PATH_END_DATA(path_end, mode, path_data) {
        seq_data_queue.push(path_data);
      }
    }
  }

  StaSeqPathData *seq_path_data = nullptr;
  std::vector<std::tuple<std::string, std::string, double>> start_end_slacks;
  int top_n = seq_data_queue.size() * top_percentage;
  while (!seq_data_queue.empty() && top_n > 0) {
    seq_path_data = dynamic_cast<StaSeqPathData *>(seq_data_queue.top());

    if (seq_path_data->get_delay_data()->get_trans_type() == trans_type) {
      auto start_pin_name =
          seq_path_data->getPathDelayData().top()->get_own_vertex()->getName();
      auto end_pin_name =
          seq_path_data->get_delay_data()->get_own_vertex()->getName();
      double slack = seq_path_data->getSlackNs();
      if (slack >= 0) {
        break;
      }
      start_end_slacks.push_back(
          std::make_tuple(start_pin_name, end_pin_name, slack));
      --top_n;
    }
    seq_data_queue.pop();
  }

  return start_end_slacks;
}

/**
 * @brief Get the violated StaSeqPathDatas of the two specified sinks that form
 * the timing path.
 *
 * @param vertex1 (one sink)
 * @param vertex2 (one sink)
 * @param mode
 * @return the violated StaSeqPathDatas.
 */
std::priority_queue<StaSeqPathData *, std::vector<StaSeqPathData *>,
                    decltype(seq_data_cmp)>
Sta::getViolatedSeqPathsBetweenTwoSinks(StaVertex *vertex1, StaVertex *vertex2,
                                        AnalysisMode mode) {
  // auto cmp = [](StaSeqPathData *left, StaSeqPathData *right) -> bool {
  //   int left_slack = left->getSlack();
  //   int right_slack = right->getSlack();
  //   return left_slack > right_slack;
  // };

  std::priority_queue<StaSeqPathData *, std::vector<StaSeqPathData *>,
                      decltype(seq_data_cmp)>
      seq_data_queue(seq_data_cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end) {
      if (!vertex1 || path_end->get_end_vertex() == vertex1) {
        FOREACH_PATH_END_DATA(path_end, mode, path_data) {
          auto *seq_path_data = dynamic_cast<StaSeqPathData *>(path_data);

          if (!vertex2 ||
              seq_path_data->getPathDelayData().top()->get_own_vertex() ==
                  vertex2) {
            if (seq_path_data->getSlack() < 0) {
              seq_data_queue.push(seq_path_data);
            }
          }
        }
      }
    }
  }

  return seq_data_queue;
}

/**
 * @brief Get the slack of the two specified sinks that form the timing path.
 *
 * @param vertex1 (one sink)
 * @param vertex2 (one sink)
 * @param mode
 * @return double worst slack
 */
std::optional<double> Sta::getWorstSlackBetweenTwoSinks(StaVertex *vertex1,
                                                        StaVertex *vertex2,
                                                        AnalysisMode mode) {
  auto cmp = [](StaPathData *left, StaPathData *right) -> bool {
    int left_slack = left->getSlack();
    int right_slack = right->getSlack();
    return left_slack > right_slack;
  };

  std::priority_queue<StaPathData *, std::vector<StaPathData *>, decltype(cmp)>
      seq_data_queue(cmp);

  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end) {
      if (!vertex1 || path_end->get_end_vertex() == vertex1) {
        FOREACH_PATH_END_DATA(path_end, mode, path_data) {
          auto *seq_path_data = dynamic_cast<StaSeqPathData *>(path_data);

          if (!vertex2 ||
              seq_path_data->getPathDelayData().top()->get_own_vertex() ==
                  vertex2) {
            seq_data_queue.push(path_data);
          }
        }
      }
    }
  }

  StaSeqPathData *seq_path_data = nullptr;
  std::optional<double> worst_slack;
  while (!seq_data_queue.empty()) {
    seq_path_data = dynamic_cast<StaSeqPathData *>(seq_data_queue.top());
    worst_slack = seq_path_data->getSlackNs();
    break;
  }

  return worst_slack;
}

/**
 * @brief Get the slack of all two sinks that form the timing path.
 *
 * @param mode
 * @return std::map<std::pair<StaVertex *, StaVertex *>, double>
 */
std::map<std::pair<StaVertex *, StaVertex *>, double>
Sta::getWorstSlackBetweenTwoSinks(AnalysisMode mode) {
  std::map<std::pair<StaVertex *, StaVertex *>, double> twosinks2worstslack;
  for (const auto &[clk, seq_path_group] : _clock_groups) {
    StaPathEnd *path_end;
    StaPathData *path_data;
    FOREACH_PATH_GROUP_END(seq_path_group.get(), path_end) {
      FOREACH_PATH_END_DATA(path_end, mode, path_data) {
        auto *seq_path_data = dynamic_cast<StaSeqPathData *>(path_data);
        StaVertex *start_vertex =
            seq_path_data->getPathDelayData().top()->get_own_vertex();

        if (!start_vertex->is_clock()) {
          continue;
        }

        auto *end_vertex = seq_path_data->get_delay_data()->get_own_vertex();
        if (end_vertex->is_port()) {
          continue;
        }

        auto *end_clock_vertex =
            seq_path_data->get_capture_clock_data()->get_own_vertex();

        if (start_vertex == end_clock_vertex) {
          continue;
        }

        // make launch and capture clock vertex pair.
        std::pair<StaVertex *, StaVertex *> sink_pair =
            std::make_pair(start_vertex, end_clock_vertex);

        double slack = seq_path_data->getSlackNs();

        // store worst slack.
        if (!twosinks2worstslack.contains(sink_pair) ||
            (slack < twosinks2worstslack[sink_pair])) {
          twosinks2worstslack[sink_pair] = slack;
        }
      }
    }
  }

  return twosinks2worstslack;
}

/**
 * @brief Get the worst slack of the end vertex.
 *
 * @param end_vertex
 * @return int
 */
int Sta::getWorstSlack(StaVertex *end_vertex, AnalysisMode mode,
                       TransType trans_type) {
  auto *the_worst_seq_path_data = getWorstSeqData(end_vertex, mode, trans_type);
  return the_worst_seq_path_data->getSlack();
}

/**
 * @brief write verilog.
 *
 * @param verilog_file_name
 * @param sort
 * @param include_pwr_gnd_pins
 * @param remove_cells
 * @param netlist
 */
void Sta::writeVerilog(const char *verilog_file_name,
                       std::set<std::string> &exclude_cell_names) {
  NetlistWriter writer(verilog_file_name, exclude_cell_names, _netlist);
  writer.writeModule();
}

/**
 * @brief reset graph data.
 *
 * @return unsigned
 */
unsigned Sta::resetGraphData() {
  StaGraph &the_graph = get_graph();
  the_graph.initGraph();
  the_graph.resetVertexData();
  the_graph.resetArcData();
  return 1;
}

/**
 * @brief reset path data.
 *
 * @return unsigned
 */
unsigned Sta::resetPathData() {
  reset_clock_groups();
  resetReportTbl();
  return 1;
}

/**
 * @brief update the timing data.
 *
 * @return unsigned
 */
unsigned Sta::updateTiming() {
  LOG_INFO << "update timing start";

  resetSdcConstrain();
  resetGraphData();
  resetPathData();

  StaGraph &the_graph = get_graph();

  Vector<std::function<unsigned(StaGraph *)>> funcs = {
      StaApplySdc(StaApplySdc::PropType::kApplySdcPreProp),
      StaConstPropagation(),
      StaClockPropagation(StaClockPropagation::PropType::kIdealClockProp),
      StaCombLoopCheck(), StaSlewPropagation(), StaDelayPropagation(),
      StaClockPropagation(StaClockPropagation::PropType::kNormalClockProp),
      StaApplySdc(StaApplySdc::PropType::kApplySdcPostNormalClockProp),
      StaClockPropagation(
          StaClockPropagation::PropType::kUpdateGeneratedClockProp),
      StaApplySdc(StaApplySdc::PropType::kApplySdcPostClockProp),
      StaLevelization(), StaBuildPropTag(StaPropagationTag::TagType::kProp),
      StaDataPropagation(StaDataPropagation::PropType::kFwdProp),
      // StaCrossTalkPropagation(),
      StaDataPropagation(StaDataPropagation::PropType::kIncrFwdProp),
      StaAnalyze(), StaApplySdc(StaApplySdc::PropType::kApplySdcPostProp),
      StaDataPropagation(StaDataPropagation::PropType::kBwdProp)};

  for (auto &func : funcs) {
    the_graph.exec(func);
  }

  LOG_INFO << "update timing end";
  return 1;
}

/**
 * @brief update the clock timing data for finding the start pins or the end
 * pins.
 *
 * @return unsigned
 */
unsigned Sta::updateClockTiming() {
  LOG_INFO << "update timing start";

  resetSdcConstrain();
  resetGraphData();
  resetPathData();

  StaGraph &the_graph = get_graph();

  Vector<std::function<unsigned(StaGraph *)>> funcs = {
      StaApplySdc(StaApplySdc::PropType::kApplySdcPreProp),
      StaConstPropagation(),
      StaClockPropagation(StaClockPropagation::PropType::kIdealClockProp),
      StaCombLoopCheck(),
      StaSlewPropagation(),
      StaDelayPropagation(),
      StaClockPropagation(StaClockPropagation::PropType::kNormalClockProp)};

  for (auto &func : funcs) {
    the_graph.exec(func);
  }

  LOG_INFO << "update timing end";
  return 1;
}

/**
 * @brief Find the start/end pins accordingt to the given end/start pin of the
 * timing path.
 *
 * @param the_vertex
 * @param is_find_end
 * @return std::set<std::string>
 */
std::set<std::string> Sta::findStartOrEnd(StaVertex *the_vertex,
                                          bool is_find_end) {
  std::set<std::string> pin_names;

  if (is_find_end) {
    StaFindEnd find_end;
    if (the_vertex->is_start() && the_vertex->is_clock()) {
      the_vertex->exec(find_end);
    } else {
      LOG_FATAL << "Not the correct start pin of the timing path";
    }

    auto &end_vertexes = the_vertex->get_fanout_end_vertexes();
    for (auto &end_vertex : end_vertexes) {
      std::string end_pin_name = end_vertex->getName();
      pin_names.insert(end_pin_name);
    }
  } else {
    StaFindStart find_start;
    if (the_vertex->is_end()) {
      the_vertex->exec(find_start);
    } else {
      LOG_FATAL << "Not the correct end pin of the timing path";
    }

    auto &start_vertexes = the_vertex->get_fanin_start_vertexes();
    for (auto &start_vertex : start_vertexes) {
      std::string start_pin_name = start_vertex->getName();
      pin_names.insert(start_pin_name);
    }
  }

  return pin_names;
}

/**
 * @brief generate the timing report.
 *
 * @return unsigned
 */
unsigned Sta::reportTiming(std::set<std::string> &&exclude_cell_names /*= {}*/,
                           bool is_derate /*=true*/,
                           bool is_clock_cap /*=false*/) {
  const char *design_work_space = get_design_work_space();
  std::string now_time = Time::getNowWallTime();
  std::string tmp = Str::replace(now_time, ":", "_");
  std::string copy_design_work_space =
      Str::printf("%s_%s", design_work_space, tmp.c_str());

  LOG_INFO << "start write sta report.";
  LOG_INFO << "output sta report path: " << design_work_space;
  if (std::filesystem::exists(design_work_space)) {
    std::filesystem::create_directories(copy_design_work_space);
  }
  std::filesystem::create_directories(design_work_space);

  // std::string specify_path_file_name =
  //     Str::printf("%s/%s.spec", design_work_space,
  //     get_design_name().c_str());
  // reportFromThroughTo(specify_path_file_name.c_str(), AnalysisMode::kMax,
  //                     nullptr, nullptr,
  //                     "u0_soc_top/u0_nic400_bus/u_cd_c0/u_ib_slave_5_ib_s/"
  //                     "u_aw_slave_port_chan_slice/u_rev_regd_slice/_1021_:D");

  resetReportTbl();

  auto copy_file = [this, &tmp, &copy_design_work_space](
                       const std::string &file_name,
                       const std::string &file_type) {
    std::string copy_file_name =
        Str::printf("%s/%s_%s%s", copy_design_work_space.c_str(),
                    get_design_name().c_str(), tmp.c_str(), file_type.c_str());
    if (std::filesystem::exists(file_name)) {
      std::filesystem::copy_file(
          file_name, copy_file_name,
          std::filesystem::copy_options::overwrite_existing);
    }
  };

  std::string rpt_file_name =
      Str::printf("%s/%s.rpt", design_work_space, get_design_name().c_str());
  copy_file(rpt_file_name, ".rpt");
  reportPath(rpt_file_name.c_str(), is_derate);

  std::string trans_rpt_file_name =
      Str::printf("%s/%s.trans", design_work_space, get_design_name().c_str());
  copy_file(trans_rpt_file_name, ".trans");
  reportTrans(trans_rpt_file_name.c_str());

  std::string cap_rpt_file_name =
      Str::printf("%s/%s.cap", design_work_space, get_design_name().c_str());
  copy_file(cap_rpt_file_name, ".cap");
  reportCap(cap_rpt_file_name.c_str(), is_clock_cap);

  std::string fanout_rpt_file_name =
      Str::printf("%s/%s.fanout", design_work_space, get_design_name().c_str());
  copy_file(fanout_rpt_file_name, ".fanout");
  reportFanout(fanout_rpt_file_name.c_str());

  std::string setup_skew_rpt_file_name = Str::printf(
      "%s/%s_setup.skew", design_work_space, get_design_name().c_str());
  copy_file(setup_skew_rpt_file_name, "_setup.skew");
  reportSkew(setup_skew_rpt_file_name.c_str(), AnalysisMode::kMax);

  std::string hold_skew_rpt_file_name = Str::printf(
      "%s/%s_hold.skew", design_work_space, get_design_name().c_str());
  copy_file(hold_skew_rpt_file_name, "_hold.skew");
  reportSkew(hold_skew_rpt_file_name.c_str(), AnalysisMode::kMin);

  std::string verilog_file_name =
      Str::printf("%s/%s.v", design_work_space, get_design_name().c_str());
  copy_file(verilog_file_name, ".v");
  writeVerilog(verilog_file_name.c_str(), exclude_cell_names);

  LOG_INFO << "The timing engine run success.";

  return 1;
}

/**
 * @brief dump vertex data in yaml format.
 *
 * @param vertex_names
 */
void Sta::dumpVertexData(std::vector<std::string> vertex_names) {
  const char *design_work_space = get_design_work_space();

  unsigned index = 0;
  for (auto &vertex_name : vertex_names) {
    StaDumpYaml dump_data;
    auto *the_vertex = findVertex(vertex_name.c_str());
    the_vertex->exec(dump_data);
    const char *file_name =
        Str::printf("%s/vertex_%d.txt", design_work_space, index);
    dump_data.printText(file_name);
    ++index;
  }
}

/**
 * @brief dump netlist data in json and txt format.
 *
 */
void Sta::dumpNetlistData() {
  const char *design_work_space = get_design_work_space();

  Sta *ista = Sta::getOrCreateSta();
  auto &the_graph = ista->get_graph();
  Netlist *the_netlist = ista->get_netlist();
  Net *the_net;

  nlohmann::json net_info_json = nlohmann::json::array();
  std::set<std::string> cell_types;

  FOREACH_NET(the_netlist, the_net) {
    // initialize the json container
    nlohmann::json net_info_obj = nlohmann::json::object();
    nlohmann::json net_load_list = nlohmann::json::array();

    auto net_name = the_net->get_name();

    // driver info
    auto *driver_pin = the_net->getDriver();
    auto *driver_cell_type = "";
    if (driver_pin->isPort()) {
      driver_cell_type = "[port]";
    } else {
      driver_cell_type =
          driver_pin->get_own_instance()->get_inst_cell()->get_cell_name();
    }
    cell_types.emplace(driver_cell_type);

    auto driver_cell_level = (*the_graph.findVertex(driver_pin))->get_level();
    auto driver_cell_fanout = the_net->getLoads().size();

    // write net_name and driver info to json container
    net_info_obj["net_name"] = net_name;
    net_info_obj["driver_cell_fanout"] = driver_cell_fanout;
    net_info_obj["driver_cell_type"] = driver_cell_type;
    net_info_obj["driver_cell_level"] = driver_cell_level;

    // load info
    std::vector<DesignObject *> load_pins = the_net->getLoads();
    auto index = 1;
    for (auto *load_pin : load_pins) {
      // initialize the load_info json container
      nlohmann::json net_load_info = nlohmann::json::object();

      auto load_vertex = the_graph.findVertex(load_pin);
      LOG_FATAL_IF(!load_vertex);

      const char *load_cell_type = "";
      if (load_pin->isPort()) {
        load_cell_type = "[port]";
      } else {
        load_cell_type =
            load_pin->get_own_instance()->get_inst_cell()->get_cell_name();
      }
      cell_types.emplace(load_cell_type);

      // write the load info to container with index
      net_load_info["index"] = index++;
      net_load_info["load_cell_type"] = load_cell_type;

      // insert the net load/cell info the the net load list before next dealing
      // next load
      net_load_list.emplace_back(net_load_info);
    }

    // write load info to json container
    net_info_obj["net_load_list"] = net_load_list;

    // write the json container to net_info_json before looping next net
    net_info_json.emplace_back(net_info_obj);
  }

  // export to json file before exiting
  std::ofstream outputJson(std::string(design_work_space) +
                           std::string("/net_list_info.json"));

  if (!outputJson.is_open()) {
    std::cerr << "Error: Failed to open file for writing." << std::endl;
  }

  outputJson << std::setw(4) << net_info_json << std::endl;
  LOG_INFO << "net_list_info.json written to "
           << std::string(design_work_space) +
                  std::string("/net_list_info.json")
           << std::endl;
  outputJson.close();

  std::ofstream outputTxt(std::string(design_work_space) +
                          std::string("/cell_types.txt"));
  if (!outputTxt.is_open()) {
    std::cerr << "Error: Failed to open file for writing." << std::endl;
  }

  std::copy(cell_types.begin(), cell_types.end(),
            std::ostream_iterator<std::string>(outputTxt, "\n"));
  LOG_INFO << "cell_types.txt written to "
           << std::string(design_work_space) + std::string("/cell_types.txt")
           << std::endl;
  outputJson.close();
}

/**
 * @brief Build clock tree for GUI.
 *
 */
void Sta::buildClockTrees() {
  StaBuildClockTree build_clock_tree;
  for (auto &clock : _clocks) {
    build_clock_tree(clock.get());
  }

  _clock_trees = std::move(build_clock_tree.takeClockTrees());
}

/**
 * @brief get the instance worst slack.
 *
 * @param analysis_mode
 * @param the_inst
 * @return std::optional<double>
 */
std::optional<double> Sta::getInstWorstSlack(AnalysisMode analysis_mode,
                                             Instance *the_inst) {
  Pin *the_pin;
  std::optional<double> the_worst_inst_slack;
  FOREACH_INSTANCE_PIN(the_inst, the_pin) {
    auto *the_vertex = findVertex(the_pin);
    if (!the_vertex) {
      continue;
    }
    auto the_worst_pin_slack = the_vertex->getWorstSlackNs(analysis_mode);
    if (the_worst_pin_slack) {
      if (!the_worst_inst_slack ||
          (*the_worst_inst_slack > *the_worst_pin_slack)) {
        the_worst_inst_slack = *the_worst_pin_slack;
      }
    }
  }

  // LOG_FATAL_IF(the_worst_inst_slack)
  //     << "inst " << the_inst->get_name() << "the worst slack "
  //     << *the_worst_inst_slack;
  return the_worst_inst_slack;
}

/**
 * @brief get total negative slack of all instance pins.
 *
 * @param analysis_mode
 * @param the_inst
 * @return std::optional<double>
 */
std::optional<double> Sta::getInstTotalNegativeSlack(AnalysisMode analysis_mode,
                                                     Instance *the_inst) {
  Pin *the_pin;
  std::optional<double> the_total_negative_inst_slack;
  FOREACH_INSTANCE_PIN(the_inst, the_pin) {
    auto *the_vertex = findVertex(the_pin);
    if (!the_vertex) {
      continue;
    }
    auto the_total_negative_slack = the_vertex->getTNSNs(analysis_mode);
    if (the_total_negative_slack) {
      if (!the_total_negative_inst_slack) {
        the_total_negative_inst_slack = *the_total_negative_slack;
      } else {
        *the_total_negative_inst_slack += *the_total_negative_slack;
      }
    }
  }

  // LOG_FATAL_IF(the_total_negative_inst_slack)
  //     << "inst " << the_inst->get_name() << "the worst slack "
  //     << *the_total_negative_inst_slack;
  return the_total_negative_inst_slack;
}

/**
 * @brief get the instance worst transition.
 *
 * @param analysis_mode
 * @param the_inst
 * @return std::optional<double>
 */
std::optional<double> Sta::getInstTransition(AnalysisMode analysis_mode,
                                             Instance *the_inst) {
  Pin *the_pin;
  std::optional<double> the_worst_inst_slew;
  FOREACH_INSTANCE_PIN(the_inst, the_pin) {
    auto *the_vertex = findVertex(the_pin);
    if (!the_vertex) {
      continue;
    }
    auto the_worst_pin_slew = the_vertex->getWorstSlewNs(analysis_mode);
    if (the_worst_pin_slew) {
      if (!the_worst_inst_slew) {
        the_worst_inst_slew = *the_worst_pin_slew;
      } else {
        if ((analysis_mode == AnalysisMode::kMax) &&
            (*the_worst_inst_slew < *the_worst_pin_slew)) {
          the_worst_inst_slew = *the_worst_pin_slew;
        } else if ((analysis_mode == AnalysisMode::kMin) &&
                   (*the_worst_inst_slew > *the_worst_pin_slew)) {
          the_worst_inst_slew = *the_worst_pin_slew;
        }
      }
    }
  }

  return the_worst_inst_slew;
}

/**
 * @brief display timing map of inst worst slack.
 *
 * @param analysis_mode
 * @return auto
 */
std::map<Instance::Coordinate, double> Sta::displayTimingMap(
    AnalysisMode analysis_mode) {
  std::map<Instance::Coordinate, double> loc_to_inst_slack;
  Instance *the_inst;
  FOREACH_INSTANCE(&_netlist, the_inst) {
    auto the_inst_worst_slack = getInstWorstSlack(analysis_mode, the_inst);
    if (the_inst_worst_slack) {
      auto inst_coordinate = the_inst->get_coordinate();
      if (!inst_coordinate) {
        LOG_INFO << "inst " << the_inst->get_name() << " has no coordinate.";
        continue;
      }

      loc_to_inst_slack[*inst_coordinate] = *the_inst_worst_slack;
    }
  }

  return loc_to_inst_slack;
}

/**
 * @brief display timing tns map.
 *
 * @param analysis_mode
 * @return std::map<Instance::Coordinate, double>
 */
std::map<Instance::Coordinate, double> Sta::displayTimingTNSMap(
    AnalysisMode analysis_mode) {
  std::map<Instance::Coordinate, double> loc_to_inst_tns;
  Instance *the_inst;
  FOREACH_INSTANCE(&_netlist, the_inst) {
    auto the_inst_tns = getInstTotalNegativeSlack(analysis_mode, the_inst);
    if (the_inst_tns) {
      auto inst_coordinate = the_inst->get_coordinate();
      if (!inst_coordinate) {
        LOG_INFO << "inst " << the_inst->get_name() << " has no coordinate.";
        continue;
      }

      loc_to_inst_tns[*inst_coordinate] = *the_inst_tns;
    }
  }

  return loc_to_inst_tns;
}

/**
 * @brief get the inst transition map.
 *
 * @param analysis_mode
 * @return std::map<Instance::Coordinate, double>
 */
std::map<Instance::Coordinate, double> Sta::displayTransitionMap(
    AnalysisMode analysis_mode) {
  std::map<Instance::Coordinate, double> loc_to_inst_transition;
  Instance *the_inst;
  FOREACH_INSTANCE(&_netlist, the_inst) {
    auto the_inst_worst_transition = getInstTransition(analysis_mode, the_inst);
    if (the_inst_worst_transition) {
      auto inst_coordinate = the_inst->get_coordinate();
      if (!inst_coordinate) {
        LOG_INFO << "inst " << the_inst->get_name() << " has no coordinate.";
        continue;
      }

      loc_to_inst_transition[*inst_coordinate] = *the_inst_worst_transition;
    }
  }

  return loc_to_inst_transition;
}

double Sta::convertTimeUnit(const double src_value) {
  TimeUnit current_time_unit = getTimeUnit();
  if (current_time_unit == TimeUnit::kNS) {
    return src_value;
  } else if (current_time_unit == TimeUnit::kFS) {
    return FS_TO_NS(src_value);
  } else if (current_time_unit == TimeUnit::kPS) {
    return PS_TO_NS(src_value);
  }
  return -1;
}

double Sta::convertCapUnit(const double src_value) {
  CapacitiveUnit current_cap_unit = getCapUnit();
  if (current_cap_unit == CapacitiveUnit::kPF) {
    return src_value;
  } else if (current_cap_unit == CapacitiveUnit::kFF) {
    return FF_TO_PF(src_value);
  } else if (current_cap_unit == CapacitiveUnit::kF) {
    return F_TO_PF(src_value);
  }
  return -1;
}
}  // namespace ista
