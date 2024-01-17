/**
 * @file Annealer.hh
 * @author Fuxing Huang (fxxhuang@gmail.com)
 * @brief
 * @version 0.1
 * @date 2023-07-11
 *
 * @copyright Copyright (c) 2023 PCNL
 *
 */
#ifndef IMP_ANNEALER_H
#define IMP_ANNEALER_H
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include "Logger.hpp"
namespace imp {
struct SAOption
{
  int max_iters = 500;
  int num_operates = 60;
  double cool_rate = 0.92;
  double init_pro = 0.95;
  double start_temperature = 1000;
};

class SASolution
{
 public:
  SASolution() = default;
  virtual void operate() = 0;
  virtual void rollback() = 0;
  virtual void update() = 0;
  virtual double evaluate() = 0;
};
class SimulateAnneal
{
 public:
  static bool solve(SASolution& solution, const SAOption& opt);

 private:
  SimulateAnneal() = delete;
  ~SimulateAnneal() = delete;
};
template <typename T>
std::vector<T> SASolve(T& solution, std::function<double(const T&)>& evaluate, std::function<void(T&)>& action, int max_iters,
                       int num_actions, double cool_rate, double temperature)
{
  std::vector<T> historys;
  double inital_temp = temperature;
  double cur_cost = evaluate(solution);
  double last_cost = 0.;
  double temp_cost{0.}, delta_cost{0.}, random{0.};
  std::random_device r;
  std::mt19937 e1(r());
  std::uniform_real_distribution<double> real_rand(0., 1.);
  // fast sa
  T solution_t = solution;
  for (int iter = 0; iter < max_iters && temperature >= 0.01; ++iter) {
    last_cost = cur_cost;
    for (int times = 0; times < num_actions; ++times) {
      action(solution_t);
      temp_cost = evaluate(solution_t);
      delta_cost = temp_cost - cur_cost;
      random = real_rand(e1);
      if (exp(-delta_cost * inital_temp / temperature) > random) {
        solution = solution_t;
        cur_cost = temp_cost;
      } else {
        solution_t = solution;
      }
    }
    INFO("iter: ", iter, " temperature: ", temperature, " cost: ", cur_cost, " dis: ", cur_cost - last_cost);
    temperature *= cool_rate;
    if ((cur_cost - last_cost) != 0)
      historys.push_back(solution);
  }

  return historys;
}
}  // namespace imp

#endif