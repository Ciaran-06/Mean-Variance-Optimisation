#include <iostream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <numeric>
#include "tiingo.h"

static int _ = []
{
    std::cerr << ">> tiingo.cpp loaded <<\n";
    return 0;
}();

std::string build_api_url(const std::string &ticker,
                          const std::string &start_date,
                          const std::string &end_date)
{
    return "https://api.tiingo.com/tiingo/daily/" + ticker +
           "/prices?startDate=" + start_date +
           "&endDate=" + end_date +
           "&format=json"; // compact JSON array
}

std::map<std::string, std::map<std::string, double>>
fetch_time_series(const std::vector<std::string> &tickers,
                  const std::string &api_key,
                  const std::string &start_date,
                  const std::string &end_date)
{
    std::map<std::string, std::map<std::string, double>> all_series;

    for (const auto &ticker : tickers)
    {
        std::string url = build_api_url(ticker, start_date, end_date);

        /* ---- HTTP GET with key in header ---- */
        auto r = cpr::Get(
            cpr::Url{url},
            cpr::Header{{"Content-Type", "application/json"},
                        {"Authorization", "Token " + api_key}});

        std::cerr << "Api key is " << api_key;
        std::cerr << "Status " << r.status_code
          << " for " << ticker << "\n"
          << r.text << "\n";


        if (r.status_code != 200)
        {
            std::cerr << "Tiingo request failed (" << r.status_code
                      << ") for " << ticker << '\n';
            continue; // skip this ticker
        }

        /* ---- Parse JSON array ---- */
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(r.text);
        }
        catch (const std::exception &e)
        {
            std::cerr << "JSON parse error for " << ticker << ": "
                      << e.what() << '\n';
            continue;
        }

        if (!j.is_array() || j.empty())
        {
            std::cerr << "No price data returned for " << ticker << '\n';
            continue;
        }

        std::map<std::string, double> series;
        for (const auto &bar : j)
        {
            try
            {
                std::string date = bar.at("date").get<std::string>().substr(0, 10);
                double adj_close = bar.at("adjClose").get<double>();
                series[date] = adj_close;
            }
            catch (...)
            {
                std::cerr << "Skipping malformed bar for " << ticker << '\n';
            }
        }

        all_series[ticker] = std::move(series);

        /* ---- polite pacing: free tier allows 500 requests/day (~1 req/sec) ---- */
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    return all_series;
}

std::map<std::string, std::vector<double>> calculate_daily_returns(
    const std::map<std::string, std::map<std::string, double>> &priceTable)
{
    std::map<std::string, std::vector<double>> dailyReturns;

    for (const auto &ticker : priceTable)
    {
        const std::string &tickerSymbol = ticker.first;
        const auto &prices = ticker.second;

        std::vector<double> returns;

        auto it = prices.begin();
        double previousPrice = it->second;

        for (++it; it != prices.end(); ++it)
        {
            const double &currentPrice = it->second;
            // Calculate the daily return: (Current Price - Previous Price) / Previous Price
            double dailyReturn = (currentPrice - previousPrice) / previousPrice;
            returns.push_back(dailyReturn);
            previousPrice = currentPrice; // Update the previous price for the next iteration
        }

        // Store the daily returns for the ticker
        dailyReturns[tickerSymbol] = returns;
    }

    return dailyReturns;
}

double calculate_covariance(const std::vector<double> &r1,
                            const std::vector<double> &r2,
                            CovarMode mode)
{
    const std::size_t n = r1.size();
    if (n != r2.size() || n < 2)
        throw std::invalid_argument("covariance: vectors must have same length ≥2");

    const double mean1 = std::accumulate(r1.begin(), r1.end(), 0.0) / n;
    const double mean2 = std::accumulate(r2.begin(), r2.end(), 0.0) / n;

    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        sum += (r1[i] - mean1) * (r2[i] - mean2);

    const double denom = (mode == CovarMode::Sample ? n - 1 : n);
    return sum / denom; // 0.00030 (sample)  or 0.00024 (population)
}

double calculate_variance(const std::vector<double> &r,
                          CovarMode mode)
{
    const std::size_t n = r.size();
    if (n < 2)
        throw std::invalid_argument("variance needs at least two observations");

    const double mean = std::accumulate(r.begin(), r.end(), 0.0) / n;

    double sum = 0.0;
    for (double x : r)
        sum += (x - mean) * (x - mean);

    const double denom = (mode == CovarMode::Sample ? n - 1 : n);
    return sum / denom;
}

std::map<std::string, std::map<std::string, double>>
calculate_covariance_matrix(
    const std::map<std::string, std::vector<double>> &dailyReturns,
    CovarMode mode)
{
    std::map<std::string, std::map<std::string, double>> C;

    for (const auto &[tickerA, retA] : dailyReturns)
    {
        for (const auto &[tickerB, retB] : dailyReturns)
        {
            double v = (tickerA == tickerB)
                           ? calculate_variance(retA, mode)
                           : calculate_covariance(retA, retB, mode);

            C[tickerA][tickerB] = v;
            C[tickerB][tickerA] = v; // symmetry
        }
    }
    return C;
}
