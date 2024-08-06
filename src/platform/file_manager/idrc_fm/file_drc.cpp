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
 * @project		iplf
 * @file		file_cts.h
 * @date		25/05/2021
 * @version		0.1
 * @description


        Process file.
 *
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "file_drc.h"

#include <cstring>
#include <iomanip>
#include <iostream>

#include "IdbLayer.h"
#include "idm.h"
#include "idrc_io/idrc_io.h"
#include "json_parser.h"

using namespace std;

namespace iplf {

bool FileDrcManager::readFile()
{
  auto path = get_data_path();
  if (path.substr(path.length() - 4) == "json" || path.substr(path.length() - 7) == "json.gz") {
    return readJson();
  } else {
    return FileManager::readFile();
  }
}

void FileDrcManager::wrapDrcStruct(idrc::DrcViolation* spot, DrcDetailResult& detail_result)
{
  memset(&detail_result, 0, sizeof(DrcDetailResult));
  detail_result.violation_type = (int) spot->get_violation_type();
  std::memcpy(detail_result.layer_name, spot->get_layer()->get_name().c_str(), spot->get_layer()->get_name().length());
  detail_result.layer_id = spot->get_layer()->get_id();
  detail_result.net_id = spot->get_net_ids().size() > 0 ? *spot->get_net_ids().begin() : -1;

  if (!spot->is_rect()) {
    std::cout << "idrc : violation type is not rectangle!" << std::endl;
    return;
  }

  auto* spot_rect = static_cast<idrc::DrcViolationRect*>(spot);
  detail_result.min_x = spot_rect->get_llx();
  detail_result.min_y = spot_rect->get_lly();
  detail_result.max_x = spot_rect->get_urx();
  detail_result.max_y = spot_rect->get_ury();
}

idrc::DrcViolation* FileDrcManager::parseDrcStruct(DrcDetailResult& detail_result)
{
  auto* idb_layer = dmInst->get_idb_layout()->get_layers()->find_routing_layer(detail_result.layer_id);
  auto* violation = new idrc::DrcViolationRect(idb_layer, (idrc::ViolationEnumType) detail_result.violation_type, detail_result.min_x,
                                               detail_result.min_y, detail_result.max_x, detail_result.max_y);
  violation->set_net_ids({detail_result.net_id});
  return violation;
}

bool FileDrcManager::parseFileData()
{
  //   uint64_t size = get_data_size();
  //   if (size == 0) {
  //     return false;
  //   }

  /// parse cts data header
  DrcFileHeader data_header;
  get_fstream().read((char*) &data_header, sizeof(DrcFileHeader));
  get_fstream().seekp(sizeof(DrcFileHeader), ios::cur);

  auto& detail_rule_map = drcInst->get_detail_drc();
  detail_rule_map.clear();

  char* data_buf = new char[max_size];
  std::memset(data_buf, 0, max_size);

  for (int i = 0; i < data_header.module_num; ++i) {
    /// parse header
    DrcResultHeader drc_header;
    get_fstream().read((char*) &drc_header, sizeof(DrcResultHeader));
    get_fstream().seekp(sizeof(DrcResultHeader), ios::cur);

    vector<idrc::DrcViolation*> spot_list;
    spot_list.reserve(drc_header.drc_num);

    std::memset(data_buf, 0, max_size);
    char* buf_ref = data_buf;
    uint64_t total_num = 0;

    while (total_num < drc_header.drc_num) {
      /// calculate spot number read from file
      int read_num = drc_header.drc_num - total_num >= max_num ? max_num : drc_header.drc_num - total_num;

      get_fstream().read(data_buf, sizeof(DrcDetailResult) * read_num);
      get_fstream().seekp(sizeof(DrcDetailResult) * read_num, ios::cur);

      for (int j = 0; j < read_num; j++) {
        /// parse single unit
        DrcDetailResult detail_result;
        std::memcpy(&detail_result, buf_ref, sizeof(DrcDetailResult));
        auto* spot = parseDrcStruct(detail_result);

        /// add to spot list
        spot_list.push_back(spot);
        buf_ref += sizeof(DrcDetailResult);

        total_num++;
      }

      std::memset(data_buf, 0, max_size);
      buf_ref = data_buf;
    }

    detail_rule_map.insert(std::make_pair(std::string(drc_header.rule_name), spot_list));
  }

  delete[] data_buf;
  data_buf = nullptr;

  return true;
}

int32_t FileDrcManager::getBufferSize()
{
  return drcInst->get_buffer_size();
}

bool FileDrcManager::saveFileData()
{
  if (saveJson() == true) {
    return true;
  }
  //   int size = getBufferSize();
  //   assert(size != 0);
  auto& detail_rule_map = drcInst->get_detail_drc();
  /// save cts data header
  DrcFileHeader file_header;
  file_header.module_num = detail_rule_map.size();

  get_fstream().write((char*) &file_header, sizeof(DrcFileHeader));
  get_fstream().seekp(sizeof(DrcFileHeader), ios::cur);

  char* data_buf = new char[max_size];
  std::memset(data_buf, 0, max_size);

  for (auto [rule_name, drc_list] : detail_rule_map) {
    /// save drc header
    DrcResultHeader drc_header;
    std::memset(&drc_header, 0, sizeof(DrcResultHeader));
    std::memcpy(drc_header.rule_name, rule_name.c_str(), rule_name.length());
    drc_header.drc_num = drc_list.size();
    // std::memcpy(buf_ref, &drc_header, sizeof(DrcResultHeader));
    // buf_ref += sizeof(DrcResultHeader);
    // mem_size += sizeof(DrcResultHeader);
    get_fstream().write((char*) &drc_header, sizeof(DrcResultHeader));
    get_fstream().seekp(sizeof(DrcResultHeader), ios::cur);

    char* buf_ref = data_buf;
    int index = 0;
    uint64_t total_num = 0;
    for (auto drc_spot : drc_list) {
      /// wrap drc file struct
      DrcDetailResult detail_result;
      wrapDrcStruct(drc_spot, detail_result);
      /// save drc data
      std::memcpy(buf_ref, &detail_result, sizeof(DrcDetailResult));
      buf_ref += sizeof(DrcDetailResult);
      index++;
      total_num++;

      if (index == max_num || total_num >= drc_list.size()) {
        /// write file
        get_fstream().write(data_buf, sizeof(DrcDetailResult) * index);
        get_fstream().seekp(sizeof(DrcDetailResult) * index, ios::cur);

        /// reset
        std::memset(data_buf, 0, max_size);
        buf_ref = data_buf;
        index = 0;
      }
    }

    buf_ref = nullptr;
  }

  delete[] data_buf;
  data_buf = nullptr;

  return true;
}

bool FileDrcManager::saveJson()
{
  auto get_layer_dict = [](std::string layer, std::map<std::string, json>& layer_dict) -> json& {
    if (layer_dict.find(layer) == layer_dict.end()) {
      json json_layer;
      json_layer["number"] = 0;
      json json_list = json::array();
      json_layer["list"] = json_list;
      layer_dict[layer] = json_layer;
    }

    return layer_dict[layer];
  };
  auto path = get_data_path();
  std::string tail_str = path.substr(path.length() - 4);
  if (tail_str != "json") {
    return false;
  }

  json drc_json;
  drc_json["file_path"] = path;
  drc_json["drc"]["number"] = 0;
  int total = 0;  /// drc total number
  auto dub = dmInst->get_idb_design()->get_units()->get_micron_dbu();

  json json_distribution = json::array();
  auto& detail_rule_map = drcInst->get_detail_drc();

  for (auto [rule_name, drc_list] : detail_rule_map) {
    json json_rule;
    json_rule[rule_name]["number"] = drc_list.size();
    total += drc_list.size();

    std::map<std::string, json> layer_dict;
    for (auto drc_spot : drc_list) {
      DrcDetailResult detail_result;
      wrapDrcStruct(drc_spot, detail_result);

      auto& json_layer = get_layer_dict(detail_result.layer_name, layer_dict);

      json json_drc;
      json_drc["llx"] = detail_result.min_x;
      json_drc["lly"] = detail_result.min_y;
      json_drc["urx"] = detail_result.max_x;
      json_drc["ury"] = detail_result.max_y;
      json_layer["list"].push_back(json_drc);
      int number = json_layer["number"];
      json_layer["number"] = number + 1;
    }

    for (auto& [layer, node] : layer_dict) {
      json_rule[rule_name]["layers"][layer] = node;
    }

    json_distribution.push_back(json_rule);
  }

  drc_json["drc"]["number"] = total;
  drc_json["drc"]["distribution"] = json_distribution;

  std::ofstream file_stream(path);
  file_stream << std::setw(4) << drc_json;

  file_stream.close();

  std::cout << std::endl << "Save feature json success, path = " << path << std::endl;
  return true;
}

bool FileDrcManager::readJson()
{
  auto path = get_data_path();
  //   std::ifstream& config_stream = ieda::getInputFileStream(path);

  nlohmann::json json;

  ieda::initJson(path, json);
  //   config_stream >> json;

  auto& detail_rule_map = drcInst->get_detail_drc();
  detail_rule_map.clear();

  /// total number
  auto total_drc = ieda::getJsonData(json, {"drc", "number"});
  auto json_distribution = ieda::getJsonData(json, {"drc", "distribution"});

  /// drc distribution
  for (auto& json_drc_type : json_distribution.items()) {
    /// drc type
    auto drc_type = json_drc_type.key();
    // /// check metal short pnly
    // if (drc_type != "Metal Short") {
    //   continue;
    // } else {
    //   /// change to "Minimum Area" in order to control drc views
    //   drc_type = "Minimum Area";
    // }

    idrc::ViolationEnumType enum_type = idrc::GetViolationType()(drc_type);

    /// drc number for all layers
    auto number = json_drc_type.value()["number"];
    vector<idrc::DrcViolation*> spot_list;
    spot_list.reserve(number);

    /// drc in each layer
    auto& json_layers = json_drc_type.value()["layers"];

    for (auto& json_layer : json_layers.items()) {
      /// layer name
      auto layer = json_layer.key();
      /// layer drc number
      auto number = json_layer.value()["number"];

      /// drc list
      auto& json_drc_list = json_layer.value()["list"];

      for (auto& json_drc : json_drc_list.items()) {
        std::set<int> net_ids;
        auto json_nets = json_drc.value()["net"];
        for (auto& json_net : json_nets.items()) {
          std::string net_name = json_net.value();
          std::cout << net_name << std::endl;
          auto net = dmInst->get_idb_design()->get_net_list()->find_net(net_name);
          if (net != nullptr) {
            net_ids.insert(net->get_id());
          }
        }

        std::set<int> inst_ids;
        auto json_insts = json_drc.value()["inst"];
        for (auto& json_inst : json_insts.items()) {
          std::string inst_name = json_inst.value();
          auto inst = dmInst->get_idb_design()->get_instance_list()->find_instance(inst_name);
          if (inst != nullptr) {
            inst_ids.insert(inst->get_id());
          }
        }

        auto* idb_layer = dmInst->get_idb_layout()->get_layers()->find_layer(layer);

        auto llx = dmInst->get_idb_design()->transUnitDB(json_drc.value()["llx"]);
        auto lly = dmInst->get_idb_design()->transUnitDB(json_drc.value()["lly"]);
        auto urx = dmInst->get_idb_design()->transUnitDB(json_drc.value()["urx"]);
        auto ury = dmInst->get_idb_design()->transUnitDB(json_drc.value()["ury"]);
        auto* violation = new idrc::DrcViolationRect(idb_layer, enum_type, llx, lly, urx, ury);
        violation->set_net_ids(net_ids);
        violation->set_inst_ids(inst_ids);

        /// add to spot list
        spot_list.push_back(violation);
      }
    }

    detail_rule_map.insert(std::make_pair(drc_type, spot_list));
  }

  return true;
}

}  // namespace iplf
