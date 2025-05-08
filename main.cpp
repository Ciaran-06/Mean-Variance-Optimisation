#include <iostream>
#include <stdlib.h>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <vector>
#include <string>

#include "dotenv.h"
#include "api/alphavantage.h"

int main()
{
    std::map<std::string, std::map<std::string, double>> priceTable;
    std::vector<std::string> tickers = {"AAPL", "MSFT", "JPM"};
    return 0;
}
