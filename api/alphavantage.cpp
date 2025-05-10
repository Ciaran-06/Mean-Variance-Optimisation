#include <iostream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <numeric>
#include "alphavantage.h"

static int _ = []
{
    std::cerr << ">> alphavantage.cpp loaded <<\n";
    return 0;
}();

std::string build_api_url(const std::string &symbol,
                          const std::string &api_key,
                          const std::string &output_size)
{
    std::string base_url = "https://www.alphavantage.co/query?";
    std::string function = "function=TIME_SERIES_DAILY";
    std::string symbol_param = "&symbol=" + symbol;
    std::string apikey_param = "&apikey=" + api_key;
    std::string output_param = "&outputsize=" + output_size;

    return base_url + function + symbol_param + output_param + apikey_param;
}
std::map<std::string, std::map<std::string, double>> fetch_time_series(
    const std::vector<std::string> &tickers,
    const std::string &api_key,
    const std::string &output_size)
{
    std::map<std::string, std::map<std::string, double>> all_series;

    for (const auto &ticker : tickers)
    {
        std::string url = build_api_url(ticker, api_key, output_size);
        auto r = cpr::Get(cpr::Url{url});
        std::cerr << r.text;

        if (r.status_code != 200)
        {
            std::cerr << "API request failed for " << ticker << "\n";
            continue;
        }

        try
        {
            auto j = nlohmann::json::parse(r.text);

            if (j.find("Time Series (Daily)") == j.end())
            {
                std::cerr << "Missing Time Series data for " << ticker << "\n";
                continue;
            }

            std::map<std::string, double> series;
            auto ts = j["Time Series (Daily)"];
            for (auto it = ts.begin(); it != ts.end(); ++it)
            {
                const std::string &date = it.key();
                try
                {
                    double price = std::stod(it.value().at("5. adjusted close").get<std::string>());
                    series[date] = price;
                }
                catch (...)
                {
                    std::cerr << "Skipping invalid entry for " << ticker << " on " << date << "\n";
                }
            }

            all_series[ticker] = series;
        }
        catch (const std::exception &e)
        {
            std::cerr << "JSON parse error for " << ticker << ": " << e.what() << "\n";
        }
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
