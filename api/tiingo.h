// alphavantage.h
#ifndef ALPHAVANTAGE_H
#define ALPHAVANTAGE_H

#include <string>
#include <vector>
#include <map>

// ── optional denominator selector ───────────────
enum class CovarMode
{
    Sample,
    Population
};

// ------------ data‑fetch helpers ---------------
std::string build_api_url(const std::string &ticker,
                          const std::string &start_date,
                          const std::string &end_date);

std::map<std::string, std::map<std::string, double>>
fetch_time_series(const std::vector<std::string> &tickers,
                  const std::string &api_key,
                  const std::string &start_date,
                  const std::string &end_date);

// ------------ statistics helpers ---------------
double calculate_variance(const std::vector<double> &returns,
                          CovarMode mode = CovarMode::Population);

double calculate_covariance(const std::vector<double> &r1,
                            const std::vector<double> &r2,
                            CovarMode mode = CovarMode::Population);

std::map<std::string, std::map<std::string, double>>
calculate_covariance_matrix(
    const std::map<std::string, std::vector<double>> &dailyReturns,
    CovarMode mode = CovarMode::Population);

std::map<std::string, std::vector<double>>
calculate_daily_returns(const std::map<std::string,
                                       std::map<std::string, double>> &priceTable);

#endif
