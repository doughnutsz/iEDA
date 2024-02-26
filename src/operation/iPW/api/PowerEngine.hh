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
 * @file PowerEngine.hh
 * @author simin tao (taosm@pcl.ac.cn)
 * @brief The power engine for provide power and timing analysis api.
 * @version 0.1
 * @date 2024-02-26
 *
 */
#pragma once

#include "Power.hh"
#include "api/TimingEngine.hh"
#include "Type.hh"

namespace ipower {

/**
 * @brief The top class for power(include timing) engine.
 *
 */
class PowerEngine {
 public:
  static PowerEngine *getOrCreatePowerEngine();
  static void destroyPowerEngine();

 private:
  PowerEngine();
  ~PowerEngine();

  Power *_power;
  ista::TimingEngine *_timing_engine;

  // Singleton timing engine.
  static PowerEngine *_power_engine;

  FORBIDDEN_COPY(PowerEngine);
};

}  // namespace ipower