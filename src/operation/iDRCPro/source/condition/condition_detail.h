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
#pragma once

#include <vector>

#include "condition_sequence.h"
#include "drc_basic_point.h"

namespace idrc {

class CheckItem;

class ConditionDetail
{
 public:
  ConditionDetail() {}
  virtual ~ConditionDetail() {}

  // virtual CheckItem* createCheckItem() = 0;  // TODO: param

  virtual bool apply(std::vector<std::pair<ConditionSequence::SequenceType, std::vector<DrcBasicPoint*>>>& check_region) = 0;

  // virtual bool apply(CheckItem* item) = 0;

 private:
};

}  // namespace idrc