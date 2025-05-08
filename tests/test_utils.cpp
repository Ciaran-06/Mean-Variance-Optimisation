#include <catch2/catch_test_macros.hpp>
#include "alphavantage.h"

TEST_CASE("build_api_url returns correct URL") {
    std::string symbol = "AAPL";
    std::string api_key = "demo_key";
    std::string output_size = "compact";

    std::string url = build_api_url(symbol, api_key, output_size);

    SECTION("URL contains base") {
        REQUIRE(url.find("https://www.alphavantage.co/query?") == 0);
    }

    SECTION("URL includes symbol") {
        REQUIRE(url.find("symbol=AAPL") != std::string::npos);
    }

    SECTION("URL includes output size") {
        REQUIRE(url.find("outputsize=compact") != std::string::npos);
    }

    SECTION("URL includes API key") {
        REQUIRE(url.find("apikey=demo_key") != std::string::npos);
    }
}