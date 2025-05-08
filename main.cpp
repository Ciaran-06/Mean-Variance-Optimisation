#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    // Create a JSON object
    nlohmann::json data;
    data["ticker"] = "AAPL";
    data["price"] = 189.73;
    data["volume"] = 1200000;

    // Print it pretty
    std::cout << data.dump(4) << std::endl;

    // Access individual elements
    std::string ticker = data["ticker"];
    double price = data["price"];
    int volume = data["volume"];

    std::cout << "\nTicker: " << ticker << ", Price: " << price << ", Volume: " << volume << std::endl;

    return 0;
}
