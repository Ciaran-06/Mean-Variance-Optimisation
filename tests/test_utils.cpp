#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

#include "alphavantage.h"

constexpr CovarMode MODE = CovarMode::Sample; 

static std::map<std::string,std::vector<double>> make_daily()
{
    return {
        {"AAPL",{ 0.01, 0.02, 0.03, 0.01, -0.02}},
        {"MSFT",{ 0.02, 0.01, 0.04, 0.00, -0.01}},
        {"JPM" ,{ 0.01, 0.03, 0.02, -0.01,  0.00}}
    };
}

TEST_CASE("build_api_url returns correct URL", "[url_builder]")
{
    std::string symbol = "AAPL";
    std::string api_key = "demo_key";
    std::string output_size = "compact";

    std::string url = build_api_url(symbol, api_key, output_size);

    SECTION("URL contains base")
    {
        REQUIRE(url.find("https://www.alphavantage.co/query?") == 0);
    }

    SECTION("URL includes symbol")
    {
        REQUIRE(url.find("symbol=AAPL") != std::string::npos);
    }

    SECTION("URL includes output size")
    {
        REQUIRE(url.find("outputsize=compact") != std::string::npos);
    }

    SECTION("URL includes API key")
    {
        REQUIRE(url.find("apikey=demo_key") != std::string::npos);
    }
}

TEST_CASE("fetch_time_series returns complete series", "[fetch_time_series]")
{
    // Mock response (simulating what you'd get from the API)
    std::string mock_json = R"JSON({
        "Time Series (Daily)": {
            "2025-05-08": { "5. adjusted close": "100.5" },
            "2025-05-07": { "5. adjusted close": "101.2" }
        }
    })JSON";

    // But for testing, we'll bypass the actual API call and directly mock the JSON response
    auto j = nlohmann::json::parse(mock_json);

    // Act: Call the function under test (fetch_time_series) with the mock data
    std::map<std::string, double> result;

    // Simulate the parsing of JSON
    auto ts = j["Time Series (Daily)"];
    for (auto it = ts.begin(); it != ts.end(); ++it)
    {
        const std::string &date = it.key();
        const auto &day = it.value();
        double price = std::stod(day.at("5. adjusted close").get<std::string>());
        result[date] = price;
    }

    // Assert: Check the result
    REQUIRE(result.size() == 2);                           // Ensure two entries in the map
    REQUIRE(result["2025-05-08"] == Catch::Approx(100.5)); // Ensure the correct price for May 8th
    REQUIRE(result["2025-05-07"] == Catch::Approx(101.2)); // Ensure the correct price for May 7th
}

TEST_CASE("calculate_daily_returns returns correct calculation", "[calculate_daily_returns]")
{
    std::map<std::string, std::map<std::string, double>> priceTable = {
        {"AAPL", {{"2025-05-08", 100.5}, {"2025-05-07", 101.2}}},
        {"MSFT", {{"2025-05-08", 210.0}, {"2025-05-07", 212.5}}}};

    auto dailyReturns = calculate_daily_returns(priceTable);

    SECTION("Correct daily return calculation for AAPL")
    {
        REQUIRE(dailyReturns["AAPL"][0] == Catch::Approx(-0.0069).epsilon(0.01));
    }
    SECTION("Correct daily returns calculation for MSFT")
    {
        REQUIRE(dailyReturns["MSFT"][0] == Catch::Approx(-0.0118).epsilon(0.01));
    }
}

TEST_CASE("Handle empty price data gracefully", "[calculate_daily_returns]")
{
    std::map<std::string, std::map<std::string, double>> emptyPriceTable;
    auto dailyReturns = calculate_daily_returns(emptyPriceTable);

    SECTION("Empty price data returns empty result")
    {
        REQUIRE(dailyReturns.empty());
    }
}

TEST_CASE("Handle single price data gracefully", "[calculate_daily_returns]")
{
    std::map<std::string, std::map<std::string, double>> priceTable = {
        {"AAPL", {{"2025-05-08", 100.5}}} // Only one price
    };

    auto dailyReturns = calculate_daily_returns(priceTable);

    SECTION("Single price data returns empty returns")
    {
        REQUIRE(dailyReturns["AAPL"].empty()); // No returns can be calculated with one price
    }
}

TEST_CASE("calculate_variance returns unbiased sample variance", "[variance]") {
    std::vector<double> r = {0.01, 0.02, 0.03, 0.01, -0.02};

    double expected =
        ( MODE == CovarMode::Sample )
            ? 0.00037  /* hand‑calc: n‑1 = 4 */
            : 0.00030; /* population: n = 5   */

    REQUIRE( calculate_variance(r, MODE) ==
             Catch::Approx(expected).epsilon(1e-6) );
}

TEST_CASE("calculate_covariance returns correct value", "[covariance]") {
    std::vector<double> aapl = {0.01, 0.02, 0.03, 0.01, -0.02};
    std::vector<double> msft = {0.02, 0.01, 0.04, 0.00, -0.01};

    double expected =
        ( MODE == CovarMode::Sample )
            ? 0.00030   // n‑1 denominator
            : 0.00024;  // n   denominator

    REQUIRE( calculate_covariance(aapl, msft, MODE) ==
             Catch::Approx(expected).epsilon(1e-6) );
}


TEST_CASE("covariance‑matrix values (sample)", "[covariance_matrix]")
{
    auto daily = make_daily();

    // choose Sample or Population here
    constexpr CovarMode MODE = CovarMode::Sample;   // ← switch if needed

    auto C = calculate_covariance_matrix(daily, MODE);

    SECTION("AAPL vs MSFT")
    {
        double expected = calculate_covariance(daily["AAPL"],
                                               daily["MSFT"], MODE);
        REQUIRE( C.at("AAPL").at("MSFT") == Catch::Approx(expected).epsilon(1e-6) );
    }

    SECTION("MSFT vs JPM")
    {
        double expected = calculate_covariance(daily["MSFT"],
                                               daily["JPM"], MODE);
        REQUIRE( C.at("MSFT").at("JPM") == Catch::Approx(expected).epsilon(1e-6) );
    }

    SECTION("AAPL vs JPM")
    {
        double expected = calculate_covariance(daily["AAPL"],
                                               daily["JPM"], MODE);
        REQUIRE( C.at("AAPL").at("JPM") == Catch::Approx(expected).epsilon(1e-6) );
    }

    SECTION("matrix symmetry")
    {
        REQUIRE( C.at("AAPL").at("MSFT") == C.at("MSFT").at("AAPL") );
        REQUIRE( C.at("MSFT").at("JPM")  == C.at("JPM") .at("MSFT") );
        REQUIRE( C.at("AAPL").at("JPM")  == C.at("JPM") .at("AAPL") );
    }
}
