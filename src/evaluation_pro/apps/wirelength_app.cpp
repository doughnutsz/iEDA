/*
 * @FilePath: wirelength_app.cpp
 * @Author: Yihang Qiu (qiuyihang23@mails.ucas.ac.cn)
 * @Date: 2024-08-24 15:37:27
 * @Description:
 */

#include <iostream>

#include "wirelength_api.h"

void test_total_wirelength();
void test_net_wirelength();
void test_path_wirelength();
void test_egr_wirelength(std::string guide_path);

int main()
{
  test_total_wirelength();
  test_net_wirelength();
  test_path_wirelength();
  test_egr_wirelength("/home/yhqiu/benchmark/AiEDA/application/benchmark/28nm/gcd/output/iEDA/data/rt/initial_router/route.guide");
  return 0;
}

void test_total_wirelength()
{
  ieval::WirelengthAPI wirelength_api;

  std::vector<std::pair<int32_t, int32_t>> point_set;
  std::pair<int32_t, int32_t> point1(0, 0);
  std::pair<int32_t, int32_t> point2(3, 6);
  std::pair<int32_t, int32_t> point3(4, 4);
  std::pair<int32_t, int32_t> point4(6, 3);

  point_set.push_back(point1);
  point_set.push_back(point2);
  point_set.push_back(point3);
  point_set.push_back(point4);

  std::vector<std::vector<std::pair<int32_t, int32_t>>> point_sets;
  point_sets.push_back(point_set);

  ieval::TotalWLSummary total_wl = wirelength_api.totalWL(point_sets);

  std::cout << "Total HPWL: " << total_wl.HPWL << std::endl;
  std::cout << "Total FLUTE: " << total_wl.FLUTE << std::endl;
  std::cout << "Total HTree: " << total_wl.HTree << std::endl;
  std::cout << "Total VTree: " << total_wl.VTree << std::endl;
}

void test_net_wirelength()
{
  ieval::WirelengthAPI wirelength_api;

  std::vector<std::pair<int32_t, int32_t>> point_set;
  std::pair<int32_t, int32_t> point1(0, 0);
  std::pair<int32_t, int32_t> point2(3, 6);
  std::pair<int32_t, int32_t> point3(4, 4);
  std::pair<int32_t, int32_t> point4(6, 3);

  point_set.push_back(point1);
  point_set.push_back(point2);
  point_set.push_back(point3);
  point_set.push_back(point4);

  ieval::NetWLSummary net_wl = wirelength_api.netWL(point_set);

  std::cout << "Net HPWL: " << net_wl.HPWL << std::endl;
  std::cout << "Net FLUTE: " << net_wl.FLUTE << std::endl;
  std::cout << "Net HTree: " << net_wl.HTree << std::endl;
  std::cout << "Net VTree: " << net_wl.VTree << std::endl;
}

void test_path_wirelength()
{
  ieval::WirelengthAPI wirelength_api;

  std::vector<std::pair<int32_t, int32_t>> point_set;
  std::pair<int32_t, int32_t> point1(0, 0);
  std::pair<int32_t, int32_t> point2(3, 6);
  std::pair<int32_t, int32_t> point3(4, 4);
  std::pair<int32_t, int32_t> point4(6, 3);

  point_set.push_back(point1);
  point_set.push_back(point2);
  point_set.push_back(point3);
  point_set.push_back(point4);

  std::pair<int32_t, int32_t> point_pair1(4, 4);
  std::pair<int32_t, int32_t> point_pair2(6, 3);

  ieval::PathWLSummary path_wl = wirelength_api.pathWL(point_set, {point_pair1, point_pair2});

  std::cout << "Path HPWL: " << path_wl.HPWL << std::endl;
  std::cout << "Path FLUTE: " << path_wl.FLUTE << std::endl;
  std::cout << "Path HTree: " << path_wl.HTree << std::endl;
  std::cout << "Path VTree: " << path_wl.VTree << std::endl;
}

void test_egr_wirelength(std::string guide_path)
{
  ieval::WirelengthAPI wirelength_api;

  float total_egr_wl = wirelength_api.totalEGRWL(guide_path);
  std::cout << "Total EGR WL: " << total_egr_wl << std::endl;

  float net_egr_wl = wirelength_api.netEGRWL(guide_path, "clk");
  std::cout << "Net EGR WL: " << net_egr_wl << std::endl;

  float path_egr_wl = wirelength_api.pathEGRWL(guide_path, "clk", "clk_0_buf:I");
  std::cout << "Path EGR WL: " << path_egr_wl << std::endl;
}
