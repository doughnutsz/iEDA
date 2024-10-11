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
 * @file CmdSetMinDelay.cc
 * @author simin tao (taosm@pcl.ac.cn)
 * @brief The sdc set_min_delay cmd implemention.
 * @version 0.1
 * @date 2024-09-13
 */

#include "Cmd.hh"
#include "netlist/DesignObject.hh"

namespace ista {
CmdSetMinDelay::CmdSetMinDelay(const char* cmd_name) : TclCmd(cmd_name) {
  auto* from_option = new TclStringListOption("-from", 0, {});
  addOption(from_option);

  auto* to_option = new TclStringListOption("-to", 0, {});
  addOption(to_option);

  auto* through_option = new TclStringListListOption("-through", 0);
  addOption(through_option);

  auto* rise_option = new TclSwitchOption("-rise");
  addOption(rise_option);

  auto* fall_option = new TclSwitchOption("-fall");
  addOption(fall_option);

  auto* delay_value_arg = new TclDoubleOption("delay_value", 1, 0.0);
  addOption(delay_value_arg);
}

unsigned CmdSetMinDelay::check() { return 1; }

unsigned CmdSetMinDelay::exec() { return 1; }
}  // namespace ista