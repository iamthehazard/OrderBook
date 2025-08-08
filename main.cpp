#include "include/json.hpp"
#include "lib.cpp"

using json = nlohmann::json;

//example lines (so I don't have to keep opening events.in)
//NewOrder: {"exchTime":1725412500115000,"orderId":1591,"price":113.26,"qty":100,"recvTime":1725413100093350,"side":"S","symbol":"E"}
//OrderCanceled: {"exchTime":1725412516673000,"orderId":36941,"recvTime":1725413100093350,"symbol":"E"}
//Trade: {"exchTime":1725413100000000,"price":118.7,"qty":50,"recvTime":1725413100693106,"symbol":"F","tradeId":"36581","tradeTime":1725413100000000}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":0,"orderId":78849,"recvTime":1725413100693106,"symbol":"F"}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":10,"orderId":45517,"recvTime":1725413100693106,"symbol":"F"}

int main() {
    std::ios::sync_with_stdio(false);

    std::freopen("../events.in", "r", stdin);
    //std::pair<std::string, std::string> data[10005]; //temp
    //std::string type, data;
    //std::cin >> type >> data;

    clock_t t = clock();

    for (int i = 0; i < 26; i++) {
        instruments[{static_cast<char>('A' + i)}] = Instrument({static_cast<char>('A' + i)});
    }

    for (int i = 0; i < 100000; i++) {
        std::string type, data;
        std::cin >> type >> data;
        auto j = json::parse(data);

        std::string symbol = j["symbol"].template get<std::string>();
        auto instrument = &instruments[symbol];

        if (type == "NewOrder:") {
            instrument->addOrder({
                j["orderId"].template get<int>(),
                j["exchTime"].template get<long long>(),
                (int)lround(j["price"].template get<double>() * PRICE_FACTOR), //test
                j["qty"].template get<int>(),
                side[j["side"].template get<std::string>()],
                j["symbol"].template get<std::string>()
            });
        }
        if (type == "OrderCanceled:") {
            instrument->removeOrder(j["orderId"].template get<int>());
        }
        if (type == "OrderExecuted:") {
            instrument->executeOrder(j["orderId"].template get<int>(), j["execQty"].template get<int>());
        }
        if (type == "Trade:") {
            //don't have to do anything yet
        }
    }

    std::cout << (double) instruments["B"].getBestOffer(side["B"]) / PRICE_FACTOR << " " << (double) instruments["B"].getBestOffer(side["S"]) / PRICE_FACTOR << "\n";
    std::cout << instruments["J"].getLevelByIndex(0, side["B"]).price << " " << instruments["J"].getLevelByIndex(0, side["B"]).volume << " " << instruments["J"].getLevelByIndex(0, side["B"]).count << "\n";
    //std::cout << instruments["J"].getLevelByPrice(1160650, side["B"]) << "\n";
    //std::cout << instruments["J"].getOrderById(2893934) << "\n";
    std::cout << ((float) clock() - t)/CLOCKS_PER_SEC << "\n";
}
