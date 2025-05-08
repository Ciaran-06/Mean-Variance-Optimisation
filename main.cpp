#include <iostream>
#include <stdlib.h>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <vector>
#include <string>

#include <dotenv.h>
#include "api/alphavantage.h"

int main()
{

    dotenv::init(); // Load environment variables from the .env file

    // Retrieve the API key from the environment
    std::string api_key = std::getenv("API_KEY");

    if (api_key.empty())
    {
        std::cerr << "API_KEY not found in the environment!" << std::endl;
        return 1; // Exit if the API key is missing
    }

    std::vector<std::string> tickers = {"AAPL", "MSFT", "JPM"};
    std::string output_size = "compact"; // Choose "full" or "compact"

    // Call the function from the Alphavantage API library
    auto priceData = fetch_time_series(tickers, api_key, output_size);

    // Print the fetched data for verification
    for (const auto &ticker : priceData)
    {
        std::cout << "Data for ticker: " << ticker.first << std::endl;
        for (const auto &entry : ticker.second)
        {
            std::cout << "  Date: " << entry.first << ", Price: " << entry.second << std::endl;
        }
    }
    return 0;
}
