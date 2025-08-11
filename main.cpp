#include "include/json.hpp"
#include "lib.cpp"

using json = nlohmann::json;

std::vector<std::string> symbols;
std::unordered_map<std::string, Instrument> instruments;

int main() {
    std::ios::sync_with_stdio(false);

    std::freopen("events.in", "r", stdin);
    //std::pair<std::string, std::string> data[10005]; //temp
    //std::string type, data;
    //std::cin >> type >> data;

    auto start = std::chrono::steady_clock::now();

    //in practice we would probably fetch symbol names somewhere in advance
    //alternatively you could read market data and detect when a new symbol appears, initializing a new symbol and instrument when this happens
    std::string alph = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < 26; i++) {
        symbols.push_back(alph.substr(i, 1));
    }

    for (auto sym : symbols) {
        instruments[sym] = Instrument(sym);
    }

    for (int i = 0; i < 100000; i++) { //change to entire file
        std::string type, data;
        std::cin >> type >> data;
        auto j = json::parse(data);

        std::string symbol = j["symbol"].template get<std::string>();
        auto instrument = &instruments[symbol];

        if (type == "NewOrder:") {
            instrument->addOrder({
                j["orderId"].template get<int>(),
                j["exchTime"].template get<long long>(),
                (int)lround(j["price"].template get<double>() * PRICE_FACTOR),
                j["qty"].template get<int>(),
                sideMap.at(j["side"].template get<std::string>()),
                j["symbol"].template get<std::string>()
            });
        } else if (type == "OrderCanceled:") {
            instrument->removeOrder(j["orderId"].template get<int>());
        } else if (type == "OrderExecuted:") {
            instrument->executeOrder(j["orderId"].template get<int>(), j["execQty"].template get<int>());
        } else if (type == "Trade:") {
            //don't have to do anything yet
        } else {
            std::cerr << "Invalid type for message " << type << " " << data << "\n";
        }
    }

    //just for sanity check
    //std::cout << (double) instruments["B"].getBestOffer(B) / PRICE_FACTOR << " " << (double) instruments["B"].getBestOffer(S) / PRICE_FACTOR << "\n";
    //std::cout << instruments["J"].getLevelByIndex(0, B).price << " " << instruments["J"].getLevelByIndex(0, S).volume << " " << instruments["J"].getLevelByIndex(0, S).count << "\n";
    //std::cout << instruments["J"].getLevelByPrice(1160650, side["B"]) << "\n";
    //std::cout << instruments["J"].getOrderById(2893934) << "\n";
    
    auto end = std::chrono::steady_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms \n";
    //ROUGH BENCHMARKS:
    //note that reading 100k lines takes ~3500ms
    //reading 100k lines AND getting components takes ~3800ms
    //so actual order book operations only take ~200ms
}
