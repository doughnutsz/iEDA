/*
 * @FilePath: density_app.cpp
 * @Author: Yihang Qiu (qiuyihang23@mails.ucas.ac.cn)
 * @Date: 2024-08-24 15:37:27
 * @Description:
 */

#include <iostream>

#include "density_api.h"

void test_density_map();
void test_density_report();

int main()
{
  test_density_map();
  test_density_report();
  return 0;
}

void test_density_map()
{
  ieval::DensityAPI density_api;

  ieval::DensityRegion region;
  region.lx = 0;
  region.ly = 0;
  region.ux = 10;
  region.uy = 7;

  ieval::DensityCells cells;
  ieval::DensityCell cell1;
  cell1.type = "macro";
  cell1.lx = 1;
  cell1.ly = 1;
  cell1.width = 4;
  cell1.height = 6;
  ieval::DensityCell cell2;
  cell2.type = "stdcell";
  cell2.lx = 1;
  cell2.ly = 3;
  cell2.width = 2;
  cell2.height = 2;
  cells.push_back(cell1);
  cells.push_back(cell2);

  int32_t grid_size = 2;

  ieval::DensityPins pins;
  ieval::DensityPin pin1;
  pin1.type = "macro";
  pin1.lx = 1;
  pin1.ly = 1;
  ieval::DensityPin pin2;
  pin2.type = "stdcell";
  pin2.lx = 1;
  pin2.ly = 7;
  pins.push_back(pin1);
  pins.push_back(pin2);

  ieval::DensityNets nets;
  ieval::DensityNet net1;
  net1.lx = 0;
  net1.ly = 0;
  net1.ux = 1;
  net1.uy = 1;
  ieval::DensityNet net2;
  net2.lx = 1;
  net2.ly = 1;
  net2.ux = 5;
  net2.uy = 7;
  nets.push_back(net1);
  nets.push_back(net2);

  ieval::CellMapSummary cell_map_summary = density_api.cellDensityMap(cells, region, grid_size);
  std::cout << "Macro density: " << cell_map_summary.macro_density << std::endl;
  std::cout << "StdCell density: " << cell_map_summary.stdcell_density << std::endl;
  std::cout << "AllCell density: " << cell_map_summary.allcell_density << std::endl;

  ieval::PinMapSummary pin_map_summary = density_api.pinDensityMap(pins, region, grid_size);
  std::cout << "Macro pin density: " << pin_map_summary.macro_pin_density << std::endl;
  std::cout << "StdCell pin density: " << pin_map_summary.stdcell_pin_density << std::endl;
  std::cout << "AllCell pin density: " << pin_map_summary.allcell_pin_density << std::endl;

  ieval::NetMapSummary net_map_summary = density_api.netDensityMap(nets, region, grid_size);
  std::cout << "Local net density: " << net_map_summary.local_net_density << std::endl;
  std::cout << "Global net density: " << net_map_summary.global_net_density << std::endl;
  std::cout << "All net density: " << net_map_summary.allnet_density << std::endl;
}

void test_density_report()
{
}
