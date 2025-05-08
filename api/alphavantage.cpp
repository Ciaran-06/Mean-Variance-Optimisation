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
