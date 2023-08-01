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

#include "Config.hpp"
#include "DRBoxId.hpp"
#include "DRCRect.hpp"
#include "DRModel.hpp"
#include "DRNet.hpp"
#include "DRNode.hpp"
#include "DataManager.hpp"
#include "Database.hpp"
#include "Net.hpp"
#include "RTU.hpp"

namespace irt {

#define DR_INST (irt::DetailedRouter::getInst())

class DetailedRouter
{
 public:
  static void initInst();
  static DetailedRouter& getInst();
  static void destroyInst();
  // function
  void route(std::vector<Net>& net_list);

 private:
  // self
  static DetailedRouter* _dr_instance;

  DetailedRouter() = default;
  DetailedRouter(const DetailedRouter& other) = delete;
  DetailedRouter(DetailedRouter&& other) = delete;
  ~DetailedRouter() = default;
  DetailedRouter& operator=(const DetailedRouter& other) = delete;
  DetailedRouter& operator=(DetailedRouter&& other) = delete;
  // function
  void routeNetList(std::vector<Net>& net_list);

#if 1  // init
  DRModel init(std::vector<Net>& net_list);
  DRModel initDRModel(std::vector<Net>& net_list);
  std::vector<DRNet> convertToDRNetList(std::vector<Net>& net_list);
  DRNet convertToDRNet(Net& net);
  void buildDRModel(DRModel& dr_model);
  void buildSchedule(DRModel& dr_model);
  void updateNetFixedRectMap(DRModel& dr_model);
  void addRectToEnv(DRModel& dr_model, DRSourceType dr_source_type, DRBoxId dr_box_id, DRCRect drc_rect);
  void updateNetPanelResultMap(DRModel& dr_model);
  void updateNetEnclosureMap(DRModel& dr_model);
  void buildBoxScaleAxis(DRModel& dr_model);
  void buildDRTaskList(DRModel& dr_model);
  void buildDRTask(DRModel& dr_model, DRNet& dr_net);
  std::map<TNode<RTNode>*, DRTask> makeDRNodeTaskMap(DRModel& dr_model, DRNet& dr_net);
  DRGroup makeDRGroup(DRBox& dr_box, DRPin& dr_pin);
  DRGroup makeDRGroup(DRBox& dr_box, TNode<RTNode>* ta_node_node);
  void buildBoundingBox(DRBox& dr_box, DRTask& dr_task);
  void buildDRBoxMap(DRModel& dr_model);
  void initLayerNodeMap(DRBox& dr_box);
  void buildNeighborMap(DRBox& dr_box);
  void checkDRBox(DRBox& dr_box);
  void saveDRBox(DRBox& dr_box);
#endif

#if 1  // iterative
  void iterative(DRModel& dr_model);
  void resetDRModel(DRModel& dr_model);
  void routeDRModel(DRModel& dr_model);
  void iterativeDRBox(DRModel& dr_model, DRBoxId& dr_box_id);
  void sortDRBox(DRBox& dr_box);
  bool sortByMultiLevel(DRTask& task1, DRTask& task2);
  SortStatus sortByRoutingVolumeASC(DRTask& task1, DRTask& task2);
  SortStatus sortByPinNumDESC(DRTask& task1, DRTask& task2);
  void resetDRBox(DRBox& dr_box);
  void routeDRBox(DRBox& dr_box);
  void routeDRTask(DRBox& dr_box, DRTask& dr_task);
  void initSingleTask(DRBox& dr_box, DRTask& dr_task);
  bool isConnectedAllEnd(DRBox& dr_box);
  void routeByStrategy(DRBox& dr_box, DRRouteStrategy dr_route_strategy);
  void routeSinglePath(DRBox& dr_box);
  void initPathHead(DRBox& dr_box);
  bool searchEnded(DRBox& dr_box);
  void expandSearching(DRBox& dr_box);
  bool passChecking(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  std::vector<Segment<LayerCoord>> getRoutingSegmentListByPathHead(DRBox& dr_box);
  bool replaceParentNode(DRBox& dr_box, DRNode* parent_node, DRNode* child_node);
  void resetPathHead(DRBox& dr_box);
  bool isRoutingFailed(DRBox& dr_box);
  void resetSinglePath(DRBox& dr_box);
  void updatePathResult(DRBox& dr_box);
  void updateDirectionSet(DRBox& dr_box);
  void resetStartAndEnd(DRBox& dr_box);
  void updateTaskResult(DRBox& dr_box, DRTask& dr_task);
  void resetSingleTask(DRBox& dr_box);
  void pushToOpenList(DRBox& dr_box, DRNode* curr_node);
  DRNode* popFromOpenList(DRBox& dr_box);
  double getKnowCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  double getJointCost(DRBox& dr_box, DRNode* curr_node, Orientation orientation);
  double getKnowWireCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  double getKnowCornerCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  double getKnowViaCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  double getEstimateCostToEnd(DRBox& dr_box, DRNode* curr_node);
  double getEstimateCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  double getEstimateWireCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  double getEstimateCornerCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  double getEstimateViaCost(DRBox& dr_box, DRNode* start_node, DRNode* end_node);
  void processDRBox(DRBox& dr_box);
  void buildRoutingResult(DRTask& dr_task);
  void countDRBox(DRBox& dr_box);
  void reportDRBox(DRBox& dr_box);
  void updateDRBox(DRModel& dr_model, DRBox& dr_box);
  bool stopDRBox(DRBox& dr_box);
  void countDRModel(DRModel& dr_model);
  void reportDRModel(DRModel& dr_model);
  bool stopDRModel(DRModel& dr_model);
#endif

#if 1  // update
  void update(DRModel& dr_model);
#endif

#if 0  // plot dr_box
void plotDRBox(DRBox& dr_box, irt_int curr_task_idx);
#endif
};

}  // namespace irt
