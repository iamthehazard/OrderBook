#include "include/json.hpp"
#include "lib.cpp"
#include <fstream>

using json = nlohmann::json;

std::vector<std::string> symbols;
std::unordered_map<std::string, Instrument> instruments;

std::string toCsvLine(L1Datum L1d) {
    {
        using namespace std;
        return to_string(L1d.exchTime) + ","
            + L1d.symbol + ","
            + to_string(L1d.price[0]) + ","
            + to_string(L1d.volume[0]) + ","
            + to_string(L1d.price[1]) + ","
            + to_string(L1d.volume[1]);
    }
}

std::ofstream l1Stream("l1.out");
void L1UpdateHandler(L1Datum L1d) {
    l1Stream << toCsvLine(L1d) << "\n";
}

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
        instruments[sym].setCallback(&L1UpdateHandler);
    }

    l1Stream << "recv_time,symbol,bid_price,bid_size,ask_price,ask_size\n";

    std::string type, data;
    while (std::cin >> type >> data) { //change to entire file
        //std::cin >> type >> data;
        auto j = json::parse(data);

        std::string symbol = j["symbol"].template get<std::string>();
        auto instrument = &instruments[symbol];

        if (type == "NewOrder:") {
            instrument->addOrder({
                j["orderId"].template get<int>(),
                j["exchTime"].template get<timestamp>(),
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
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
