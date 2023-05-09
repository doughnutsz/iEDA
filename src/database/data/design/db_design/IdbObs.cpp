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
 * @project		iDB
 * @file		IdbObs.h
 * @date		25/05/2021
 * @version		0.1
* @description


        Describe Obstrction information,.
 *
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "IdbObs.h"

using namespace std;
namespace idb {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IdbObsLayer::IdbObsLayer()
{
  _layer_shape = new IdbLayerShape();
}

IdbObsLayer::~IdbObsLayer()
{
  if (_layer_shape) {
    delete _layer_shape;
    _layer_shape = nullptr;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IdbObs::IdbObs()
{
}

IdbObs::~IdbObs()
{
}

IdbObsLayer* IdbObs::add_obs_layer(IdbObsLayer* obs_layer)
{
  IdbObsLayer* pLayer = obs_layer;
  if (pLayer == nullptr) {
    pLayer = new IdbObsLayer();
  }
  _obs_layer_list.emplace_back(pLayer);

  return pLayer;
}

}  // namespace idb
