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
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WArANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#ifndef IMP_SHAPECURVE_H
#define IMP_SHAPECURVE_H
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "Geometry.hh"
namespace imp {
constexpr double MIN_AR = 1 / 3;
constexpr double MAX_AR = 3.;
// template <typename T>
// class ShapeCurve
// {
//  public:
//   ShapeCurve(T min_area, double min_ar, double max_ar, double inital_ar, double x_inflate, double y_inflate)
//       : _min_area(min_area), _min_ar(min_ar), _max_ar(max_ar), _ar(inital_ar), _x_inflate(x_inflate), _y_inflate(y_inflate)
//   {
//   }
//   ~ShapeCurve() = default;
//   geo::box<T> minimal_box() const
//   {
//     double min_area = static_cast<double>(_min_area);
//     double height = std::ceil(std::sqrt(min_area * _ar));
//     double width = std::ceil(min_area / height);

//     geo::box<double> fbox = geo::scale(geo::make_box(0., 0., width, height), 1. + _x_inflate, 1. + _y_inflate);

//     return geo::make_box(0, 0, static_cast<T>(geo::width(fbox)), static_cast<T>(geo::height(fbox)));
//   }

//  private:
//   double _ar;  // h/w
//   double _min_ar;
//   double _max_ar;
//   double _x_inflate;
//   double _y_inflate;

//   T _min_area;
// };
// template <template <typename> typename Shape, typename T>
// ShapeCurve<T> make_shapecurve(const std::vector<Shape<T>>& shape, double x_inflate = 0., double y_inflate = 0., double inital_ar = 1.,
//                               double min_ar = MIN_AR, double max_ar = MAX_AR)
// {
//   T min_area{0};
//   // T max_width{0};
//   // T max_height{0};
//   for (auto&& i : shape) {
//     min_area += area(i);
//     // max_width = std::max(max_width, width(i));
//     // max_height = std::max(max_height, height(i));
//   }
//   return ShapeCurve<T>(min_area, min_ar, max_ar, inital_ar, x_inflate, y_inflate);
// }

// template <typename T>
// ShapeCurve<T> MergeShapeCurve(const std::vector<ShapeCurve<T>>& shapes)
// {
//   return ShapeCurve<T>();
// }

template <typename T>
class ShapeCurve
{
 public:
  ShapeCurve(double min_ar, double max_ar) : _min_ar(min_ar), _max_ar(max_ar), _width(0), _height(0), _area(0) {}
  ShapeCurve(const ShapeCurve& other) = default;
  ~ShapeCurve() = default;

  // getter
  T get_width() const { return _width; }
  T get_height() const { return _height; }
  T get_area() const { return _area; }
  double get_min_ar() const { return _min_ar; }
  double get_max_ar() const { return _max_ar; }
  double get_ar() const { return calAr(_width, _height); }
  const std::vector<std::pair<T, T>>& get_width_list() const { return _width_list; }
  const std::vector<std::pair<T, T>>& get_height_list() const { return _height_list; }

  void setShapesDiscrete(const std::vector<std::pair<T, T>>& discrete_shapes, bool force_flag)
  {
    // discrete_shapes is in ascending order by area
    // force_flag is used by macros, if force_flag == true, not use arr clip
    _width_list.clear();
    _height_list.clear();
    if (discrete_shapes.empty()) {
      std::cerr << "Error: setting empty shaping curve" << std::endl;
      exit(1);
    }
    // not sorted..
    for (auto& shape : discrete_shapes) {
      // discard odd shapes
      if (force_flag || checkShapeAr(shape.first, shape.second)) {
        _width_list.emplace_back(shape.first, shape.first);
        _height_list.emplace_back(shape.second, shape.second);
      }
    }
    _width = discrete_shapes[0].first;
    _height = discrete_shapes[0].second;
    _area = discrete_shapes[0].first * discrete_shapes[0].second;
  }

  void setShapes(float soft_shapes_area, const std::vector<std::pair<T, T>>& discrete_shapes = std::vector<std::pair<T, T>>())
  {
    // discrete_shapes is in ascending order by area
    // discrete_shapes does not concern soft_shape_area

    T min_width, max_width;
    _width_list.clear();
    _height_list.clear();

    // no discrete shapes

    if (discrete_shapes.empty()) {
      if (soft_shapes_area <= 0) {
        std::cerr << "setting wrong shapes with no discreate shapes and soft_shape_area!" << std::endl;
        exit(1);
      } else {  // pure soft shape, calculate min_width && max_width based on ar
        _area = soft_shapes_area;
        min_width = calWidthByAr(_area, _min_ar);
        max_width = calWidthByAr(_area, _max_ar);
        _width_list.emplace_back(min_width, max_width);
        _height_list.emplace_back(_area / min_width, _area / max_width);
        // set default shape;
        _width = _width_list[0].first;
        _height = _height_list[0].first;
        return;
      }
    }
    // mixed shapes
    // use the largest area of discrete_shapes
    auto iter = std::max_element(
        discrete_shapes.begin(), discrete_shapes.end(),
        [](const std::pair<T, T>& p1, const std::pair<T, T>& p2) -> bool { return p1.first * p1.second < p2.first * p2.second; });
    _area = iter->first * iter->second + soft_shapes_area;  // discrete-shape-area + soft-shape-area

    std::vector<std::pair<T, T>> temp_width_list, temp_height_list;
    for (auto& shape : discrete_shapes) {
      min_width = shape.first;
      max_width = _area / shape.second;
      temp_width_list.emplace_back(min_width, max_width);
    }

    // sort width list asending order
    temp_height_list = temp_width_list;
    temp_width_list.clear();
    std::sort(temp_height_list.begin(), temp_height_list.end(),
              [](const std::pair<T, T>& p1, const std::pair<T, T>& p2) -> bool { return p1.first < p2.first; });
    for (auto& interval : temp_height_list) {
      if (temp_width_list.empty() || interval.first > temp_width_list.back().second) {
        temp_width_list.push_back(interval);
      } else if (interval.second > temp_width_list.back().second) {
        temp_width_list.back().second = interval.second;
      }
    }

    temp_height_list.clear();
    for (auto& shape : temp_width_list) {
      temp_height_list.emplace_back(_area / shape.first, _area / shape.second);
    }

    // clip shape curves to discard odd shapes
    T new_height, new_width;
    for (size_t i = 0; i < temp_width_list.size(); ++i) {
      // 5 possible conditions
      if (calAr(temp_width_list[i].first, temp_height_list[i].first) < _min_ar) {
        if (calAr(temp_width_list[i].second, temp_height_list[i].second) <= _min_ar) {
          continue;
        } else {  // clip width-interval by _min_ar
          new_width = calWidthByAr(_area, _min_ar);
          new_height = _area / new_width;
          _width_list.emplace_back(new_width, temp_width_list[i].second);
          _height_list.emplace_back(new_height, temp_height_list[i].second);
          // clip width-interval by _min_ar
          if (calAr(temp_width_list[i].second, temp_height_list[i].second) > _max_ar) {
            auto new_width = calWidthByAr(_area, _max_ar);
            auto new_height = _area / new_width;
            _width_list.back().second = new_width;
            _height_list.back().second = new_height;
            break;  // no valid shapes left
          }
          continue;
        }
      } else if (calAr(temp_width_list[i].second, temp_height_list[i].second) > _max_ar) {
        if (calAr(temp_width_list[i].first, temp_height_list[i].first) >= _max_ar) {
          break;  // no valid shapes left
        } else {
          auto new_width = calWidthByAr(_area, _max_ar);
          auto new_height = _area / new_width;
          _width_list.emplace_back(temp_width_list[i].first, new_width);
          _height_list.emplace_back(temp_height_list[i].first, new_height);
          break;
        }
      } else {  // needn't clip
        _width_list.emplace_back(temp_width_list[i].first, temp_width_list[i].second);
        _height_list.emplace_back(temp_height_list[i].first, temp_height_list[i].second);
      }
    }

    // set default shape
    _width = _width_list[0].first;
    _height = _height_list[0].first;
  }

 private:
  double _min_ar;  // w / h
  double _max_ar;  // w / h
  T _width;
  T _height;
  T _area;
  std::vector<std::pair<T, T>> _width_list;
  std::vector<std::pair<T, T>> _height_list;

  // double _x_inflate;
  // double _y_inflate;
  bool checkShapeAr(const T& width, const T& height) const
  {
    double ar = calAr(width, height);
    return ar >= _min_ar && ar <= _max_ar;
  }
  double calAr(const T& width, const T& height) const { return double(width) / height; }
  T calWidthByAr(T area, double Ar) { return std::sqrt(area * Ar); }
};

}  // namespace imp

#endif