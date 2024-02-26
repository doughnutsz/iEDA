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

#include "geometry_boost.h"

#include <boost/geometry.hpp>

namespace ieda_solver {

GeometryBoost::GeometryBoost()
{
}

GeometryBoost::~GeometryBoost()
{
}

void GeometryBoost::addRect(int llx, int lly, int urx, int ury)
{
  GtlRect rect(llx, lly, urx, ury);

  _polyset += rect;
}

/**
 * get points from polygon list
 * @param
 * std::pair<int, int> : define point x, y
 * std::vector<std::pair<int, int>> : define point list
 * std::vector<std::vector<std::pair<int, int>>> : define polygon list
 */
// std::vector<std::vector<std::pair<int, int>>> GeometryBoost::get_polygons_points()
// {
//   std::vector<std::vector<std::pair<int, int>>> polygons_points;

//   auto& polygon_list = get_polygon_list();
//   polygons_points.reserve(polygon_list.size());

//   for (auto& polygon : polygon_list) {
//     std::vector<std::pair<int, int>> pt_list;
//     for (auto& pt : polygon) {
//       pt_list.push_back(std::make_pair(pt.x(), pt.y()));
//     }
//     polygons_points.emplace_back(pt_list);
//   }

//   return polygons_points;
// }
/**
 * get boost points from polygon list
 * @param
 * uint64_t : total point number
 * GtlPoint : boost point
 * std::vector<GtlPoint> : point list for one polygon
 * std::vector<std::vector<GtlPoint>> : define all point list in the polygon list
 */
// std::pair<uint64_t, std::vector<std::vector<GtlPoint>>> GeometryBoost::get_boost_polygons_points()
// {
//   auto& polygon_list = get_polygon_list();

//   std::vector<std::vector<GtlPoint>> polygons_points;
//   polygons_points.reserve(polygon_list.size());
//   for (auto& polygon : polygon_list) {
//     polygons_points.emplace_back(polygon.coords_);
//   }

//   uint64_t number = _polyset.size();
//   /// return total number & point list number for all polygons
//   return std::make_pair(number, polygons_points);
// }

void GeometryBoost::addGeometry(EngineGeometry* geometry)
{
  auto* boost_geometry = dynamic_cast<GeometryBoost*>(geometry);
  if (boost_geometry == nullptr) {
    return;
  }

  boost_geometry->get_polyset().clean();
  _polyset += boost_geometry->get_polyset();
}

std::vector<GeometryPolygon>& GeometryBoost::getPolygons()
{
  if (_polygon_list.size() <= 0) {
    _polyset.get(_polygon_list);
  }
  return _polygon_list;
}

std::vector<GeometryRect>& GeometryBoost::getOverlap()
{
  if (_overlap_list.size() <= 0) {
    GtlPolygon90Set set(_polyset);
    set.self_intersect();
    set.get_rectangles(_overlap_list);
  }
  return _overlap_list;
}

}  // namespace ieda_solver