#pragma once

#include "wirelength_db.h"

namespace ieval {

class WirelengthEval
{
 public:
  WirelengthEval();
  ~WirelengthEval();

  int32_t evalTotalHPWL(PointSets point_sets);
  int32_t evalTotalFLUTE(PointSets point_sets);
  int32_t evalTotalHTree(PointSets point_sets);
  int32_t evalTotalVTree(PointSets point_sets);

  int32_t evalNetHPWL(PointSet point_set);
  int32_t evalNetFLUTE(PointSet point_set);
  int32_t evalNetHTree(PointSet point_set);
  int32_t evalNetVTree(PointSet point_set);

  int32_t evalPathHPWL(PointSet point_set, PointPair point_pair);
  int32_t evalPathFLUTE(PointSet runEGRpoint_set, PointPair point_pair);
  int32_t evalPathHTree(PointSet point_set, PointPair point_pair);
  int32_t evalPathVTree(PointSet point_set, PointPair point_pair);

  float evalTotalEGRWL(std::string guide_path);
  float evalNetEGRWL(std::string guide_path, std::string net_name);
  float evalPathEGRWL(std::string guide_path, std::string net_name, std::string load_name);
};

}  // namespace ieval