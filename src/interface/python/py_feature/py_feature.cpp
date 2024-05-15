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
#include "py_feature.h"

#include "feature_manager.h"
#include "idm.h"

namespace python_interface {
bool feature_layout(const std::string& path)
{
  return featureInst->save_layout(path);
}

bool feature_instances(const std::string& path)
{
  return featureInst->save_instances(path);
}

bool feature_nets(const std::string& path)
{
  return featureInst->save_nets(path);
}

bool feature_summary(const std::string& path, const std::string& step)
{
  return featureInst->save_reportSummary(path, step);
}

bool feature_summary_map(const std::string& path, const int& bin_cnt_x, const int& bin_cnt_y)
{
  return featureInst->save_reportSummary_map(path, bin_cnt_x, bin_cnt_y);
}

}  // namespace python_interface