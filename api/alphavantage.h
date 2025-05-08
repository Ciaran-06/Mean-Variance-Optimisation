#ifndef ALPHAVANTAGE_H
#define ALPHAVANTAGE_H

#include <string>

std::string build_api_url(const std::string& symbol,
                          const std::string& api_key,
                          const std::string& output_size = "compact");

#endif
