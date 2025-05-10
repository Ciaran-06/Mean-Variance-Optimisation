#include <iostream>
#include <stdlib.h>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <vector>
#include <string>

#include <dotenv.h>
#include "api/tiingo.h"

int main()
{
    dotenv::init("../.env");   // loads .env -> calls setenv for each entry
    std::string api_key = std::getenv("API_KEY");    


    std::vector<std::string> tickers = {"AAPL", "MSFT", "JPM"};
    std::string start_date = "2023-01-01";
    std::string end_date   = "2023-12-31";
    std::cerr << "main(): key = [" << api_key << "]\n";

    auto priceData = fetch_time_series(tickers, api_key,
                                       start_date, end_date);

    for (const auto& [ticker, series] : priceData) {
        std::cout << "\nTicker: " << ticker << '\n';
        for (const auto& [date, price] : series)
            std::cout << "  " << date << "  " << price << '\n';
    }
    return 0;
}
