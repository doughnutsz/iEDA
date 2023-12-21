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

#include "Logger.hpp"

namespace irt {

enum class Stage
{
  kNone,
  kDetailedRouter,
  kGlobalRouter,
  kPinAccessor,
  kResourceAllocator,
  kTrackAssigner
};

struct GetStageName
{
  std::string operator()(const Stage& stage) const
  {
    std::string stage_name;
    switch (stage) {
      case Stage::kNone:
        stage_name = "none";
        break;
      case Stage::kDetailedRouter:
        stage_name = "detailed_router";
        break;
      case Stage::kGlobalRouter:
        stage_name = "global_router";
        break;
      case Stage::kPinAccessor:
        stage_name = "pin_accessor";
        break;
      case Stage::kResourceAllocator:
        stage_name = "resource_allocator";
        break;
      case Stage::kTrackAssigner:
        stage_name = "track_assigner";
        break;
      default:
        LOG_INST.error(Loc::current(), "Unrecognized type!");
        break;
    }
    return stage_name;
  }
};

}  // namespace irt
