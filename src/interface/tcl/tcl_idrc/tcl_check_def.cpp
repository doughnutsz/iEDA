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
#include "DRC.h"
#include "idrc_api.h"
#include "tcl_drc.h"
#include "tcl_util.h"

namespace tcl {

// public

TclDrcCheckDef::TclDrcCheckDef(const char* cmd_name) : TclCmd(cmd_name)
{
  //   addOptionForJSON();
  addOptionForTCL();
}

unsigned TclDrcCheckDef::exec()
{
  if (!check()) {
    return 0;
  }
  //   std::map<std::string, std::any> config_map;

  //   bool pass = false;
  //   //   pass = !pass ? initConfigMapByJSON(config_map) : pass;
  //   pass = !pass ? initConfigMapByTCL(config_map) : pass;
  //   if (!pass) {
  //     return 0;
  //   }
  //   DrcInst.initDesign(config_map);
  //   std::cout << "init DRC check module ......" << std::endl;
  //   DrcInst.initCheckModule();
  //   std::cout << "run DRC check module ......" << std::endl;
  //   DrcInst.run();
  //   std::cout << "report check result ......" << std::endl;
  //   DrcInst.report();
  idrc::DrcApi drc_api;
  drc_api.init();
  auto violations = drc_api.checkDef();
  return 1;
}

// private

// void TclDrcCheckDef::addOptionForJSON()
// {
//   TclUtil::addOption(this, "-config", ValueType::kString);
// }

void TclDrcCheckDef::addOptionForTCL()
{
  TclUtil::addOption(this, "-def_path", ValueType::kString);
}

// bool TclDrcCheckDef::initConfigMapByJSON(std::map<std::string, std::any>& config_map)
// {
//   TclOption* config_file_path_option = getOptionOrArg("-config");
//   if (!config_file_path_option->is_set_val()) {
//     return false;
//   }

//   std::ifstream& config_file = TclUtil::getInputFileStream(config_file_path_option->getStringVal());
//   nlohmann::json json;
//   // read a JSON file
//   config_file >> json;
//   json = json["PM"];

//   TclUtil::updateConfigMap(json, config_map, "-log_verbose", ValueType::kInt);
//   TclUtil::updateConfigMap(json, config_map, "-ban_pa_generate_layer_list", ValueType::kStringList);
//   TclUtil::updateConfigMap(json, config_map, "-only_via_access_layer_list", ValueType::kStringList);
//   TclUtil::updateConfigMap(json, config_map, "-temp_directory_path", ValueType::kString);

//   TclUtil::closeFileStream(config_file);

//   return true;
// }

bool TclDrcCheckDef::initConfigMapByTCL(std::map<std::string, std::any>& config_map)
{
  std::any config_value = TclUtil::getValue(this, "-def_path", ValueType::kString);
  if (config_value.has_value()) {
    config_map.insert(std::make_pair("-def_path", config_value));
  }

  return true;
}

}  // namespace tcl