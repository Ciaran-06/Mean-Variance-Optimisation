#include <iostream>
#include<cpr/cpr.h>
#include<nlohmann/json.hpp>
#include <stdexcept>
#include <numeric>
#include "alphavantage.h"

std::string build_api_url(const std::string& symbol,
                          const std::string& api_key,
                          const std::string& output_size) {
    std::string base_url = "https://www.alphavantage.co/query?";
    std::string function = "function=TIME_SERIES_DAILY_ADJUSTED";
    std::string symbol_param = "&symbol=" + symbol;
    std::string apikey_param = "&apikey=" + api_key;
    std::string output_param = "&outputsize=" + output_size;

    return base_url + function + symbol_param + output_param + apikey_param;
}

std::map<std::string, double> fetch_time_series(const std::string& ticker,
                                               const std::string& api_key,
                                               const std::string& output_size) {
    // Build the API URL
    auto url = build_api_url(ticker, api_key, output_size);

    // Send the GET request
    auto r = cpr::Get(cpr::Url{url});

    // Handle request failure (invalid API key, network issues, etc.)
    if (r.status_code != 200) {
        throw std::runtime_error("API request failed for " + ticker + " with status code: " + std::to_string(r.status_code));
    }

    // Try parsing the JSON response
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(r.text);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse JSON response: " + std::string(e.what()));
    }

    // Check if "Time Series (Daily)" exists in the response
    if (j.find("Time Series (Daily)") == j.end()) {
        throw std::runtime_error("Missing 'Time Series (Daily)' field in response for ticker: " + ticker);
    }

    // Access the time series data
    auto ts = j["Time Series (Daily)"];
    std::map<std::string, double> series;

    // Parse the time series data
    for (auto it = ts.begin(); it != ts.end(); ++it) {
        const std::string& date = it.key();
        const auto& day = it.value();

        try {
            // Attempt to parse the price, if it's non-numeric (e.g., "N/A"), throw an error
            double price = std::stod(day.at("5. adjusted close").get<std::string>());
            series[date] = price;
        } catch (const std::invalid_argument& e) {
            // Handle case where the price is not a valid number (e.g., "N/A")
            std::cerr << "Skipping date " << date << " due to invalid price data: " << e.what() << std::endl;
        } catch (const std::out_of_range& e) {
            // Handle case where "5. adjusted close" is missing
            std::cerr << "Skipping date " << date << " due to missing '5. adjusted close': " << e.what() << std::endl;
        }
    }

    // Return the parsed time series data
    return series;
}

std::map<std::string, std::vector<double>> calculate_daily_returns(
    const std::map<std::string, std::map<std::string, double>>& priceTable
) {
    std::map<std::string, std::vector<double>> dailyReturns;

    for(const auto& ticker : priceTable) {
        const std::string& tickerSymbol = ticker.first;
        const auto& prices = ticker.second;

        std::vector<double> returns;

        auto it = prices.begin();
        double previousPrice = it->second;

        for (++it; it != prices.end(); ++it) {
            const double& currentPrice = it->second;
            // Calculate the daily return: (Current Price - Previous Price) / Previous Price
            double dailyReturn = (currentPrice - previousPrice) / previousPrice;
            returns.push_back(dailyReturn);
            previousPrice = currentPrice;  // Update the previous price for the next iteration
        }

        // Store the daily returns for the ticker
        dailyReturns[tickerSymbol] = returns;
    }

    return dailyReturns;
    }

double calculate_covariance(const std::vector<double>& r1,
                            const std::vector<double>& r2,
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
    return sum / denom;                // 0.00030 (sample)  or 0.00024 (population)
}

double calculate_variance(const std::vector<double>& r,
                          CovarMode mode)
{
    const std::size_t n = r.size();
    if (n < 2)
        throw std::invalid_argument("variance needs at least two observations");

    const double mean = std::accumulate(r.begin(), r.end(), 0.0) / n;

    double sum = 0.0;
    for (double x : r) sum += (x - mean)*(x - mean);

    const double denom = (mode == CovarMode::Sample ? n - 1 : n);
    return sum / denom;
}

std::map<std::string, std::map<std::string,double>>
calculate_covariance_matrix(
        const std::map<std::string, std::vector<double>>& dailyReturns,
        CovarMode mode)
{
    std::map<std::string, std::map<std::string,double>> C;

    for (const auto& [tickerA, retA] : dailyReturns)
    {
        for (const auto& [tickerB, retB] : dailyReturns)
        {
            double v = (tickerA == tickerB)
                     ? calculate_variance  (retA,        mode)
                     : calculate_covariance(retA, retB,  mode);

            C[tickerA][tickerB] = v;
            C[tickerB][tickerA] = v;   // symmetry
        }
    }
    return C;
}




std::map<std::string, std::map<std::string, double>> fetch_time_series(
    const std::vector<std::string>& tickers,
    const std::string& api_key,
    const std::string& output_size) {
    
    std::map<std::string, std::map<std::string, double>> priceTable;

    for (const auto& ticker : tickers) {
        std::string url = build_api_url(ticker, api_key, output_size);
        std::cout << "Fetching data for: " << ticker << std::endl;

        auto r = cpr::Get(cpr::Url{url});
        
        if (r.status_code != 200) {
            std::cerr << "Failed to fetch data for " << ticker << " (Status Code: " << r.status_code << ")" << std::endl;
            continue; // Skip to the next ticker if the request failed
        }

        try {
            auto json_response = nlohmann::json::parse(r.text);
            auto ts = json_response.at("Time Series (Daily)");

            std::map<std::string, double> ticker_prices;
            
            for (auto& entry : ts.items()) {
                const std::string& date = entry.key();
                double price = std::stod(entry.value().at("5. adjusted close").get<std::string>());
                ticker_prices[date] = price;
            }
            
            priceTable[ticker] = ticker_prices;  // Store the prices for the ticker

        } catch (const std::exception& e) {
            std::cerr << "Error parsing response for " << ticker << ": " << e.what() << std::endl;
        }
    }
    return priceTable;
}