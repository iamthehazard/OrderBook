#include "include/json.hpp"
#include <iostream>
#include <list>
#include <unordered_map>

using json = nlohmann::json;

//example lines (so I don't have to keep opening events.in)
//NewOrder: {"exchTime":1725412500115000,"orderId":1591,"price":113.26,"qty":100,"recvTime":1725413100093350,"side":"S","symbol":"E"}
//OrderCanceled: {"exchTime":1725412516673000,"orderId":36941,"recvTime":1725413100093350,"symbol":"E"}
//Trade: {"exchTime":1725413100000000,"price":118.7,"qty":50,"recvTime":1725413100693106,"symbol":"F","tradeId":"36581","tradeTime":1725413100000000}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":0,"orderId":78849,"recvTime":1725413100693106,"symbol":"F"}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":10,"orderId":45517,"recvTime":1725413100693106,"symbol":"F"}

/*void readLine() {
    std::string type, data;
    std::cin >> type >> data;
    if (type == "NewOrder:") {

    }
    if (type == "OrderCanceled:") {

    }
    if (type == "OrderExecuted:") {

    }
    if (type == "Trade:") {

    }
}*/

const int PRICE_FACTOR = 10000; //should be divisible by 2000 and a power of 10

std::map<std::string, bool> side = {
    {"S", 1},
    {"B", 0}
};

struct Order {
    int id;
    long long exchTime;
    int price; //price * 1000, always int
    int qty;
    bool isAsk;
    std::string symbol;
    std::list<Order>::iterator it;
};

struct PriceLevel {
    int price;
    int volume = 0;
    int count = 0; //# of orders
    std::list<Order> orders;
};

std::ostream& operator<<(std::ostream& stream, const Order& o) {
    stream << "{exchTime:" << o.exchTime << ",id:" << o.id << ",price:" << (double) o.price / PRICE_FACTOR << ",qty:" << o.qty << ",side:" << (o.isAsk ? "S" : "B") << ",symbol:" << o.symbol << "}";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const std::list<Order>& orders) {
    if (orders.empty()) {
        stream << "[]";
    } else {
        stream << "[\n";
        for (const auto& o : orders) stream << "\t" << o << "\n";
        stream << "]";
    }
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const PriceLevel pl) {
    stream << "{price:" << (double) pl.price / PRICE_FACTOR << ",volume:" << pl.volume << ",count:" << pl.count << ",orders:" << pl.orders << "}";
}

class Instrument final {
    public:
        Instrument() {}

        Instrument(std::string sym) {
            symbol = sym;
        }

        void addOrder(Order order) {
            auto pl = getLevelPointer(order.price, order.isAsk);
            pl->price = order.price;
            pl->orders.push_back(order);
            std::prev(pl->orders.end())->it = std::prev(pl->orders.end());
            order.it = std::prev(pl->orders.end());
            pl->volume += order.qty;
            pl->count++;

            ordersById[order.id] = order;
        }

        void removeOrder(Order order) {
            auto pl = getLevelPointer(order.price, order.isAsk);

            pl->orders.erase(order.it);
            if (pl->orders.empty()) {
                if (order.isAsk) asks.erase(order.price);
                else bids.erase(order.price);
            }
            pl->volume -= order.qty;
            pl->count--;

            ordersById.erase(order.id);
        }

        void removeOrder(int id) {
            removeOrder(getOrderById(id));
        }

        void executeOrder(Order order, int execQty) {
            if (order.qty == execQty) removeOrder(order);
            else {
                order.it->qty -= execQty;
                ordersById[order.id].qty -= execQty;
                getLevelPointer(order.price, order.isAsk)->volume -= execQty;
            }
        }

        void executeOrder(int id, int qty) {
            executeOrder(getOrderById(id), qty);
        }

        Order getOrderById(int id) {
            auto it = ordersById.find(id);
            if (it == ordersById.end()) throw std::invalid_argument("No order with id " + std::to_string(id));
            return it->second;
        }

        PriceLevel getLevelByIndex(std::size_t index, bool isAsk) {
            if (isAsk) {
                if (asks.size() > index) {
                    auto it = asks.begin();
                    std::advance(it, index);
                    return it->second;
                }
                throw std::invalid_argument{"No ask level at index " + std::to_string(index)};
            } else {
                if (bids.size() > index) {
                    auto it = bids.begin();
                    std::advance(it, index);
                    return it->second;
                }
                throw std::invalid_argument{"No bid level at index " + std::to_string(index)};
            }
        }

        PriceLevel getLevelByPrice(int price, bool isAsk) {
            if (isAsk) {
                auto it = asks.find(price);
                if (it == asks.end()) throw std::invalid_argument{"No ask level at " + std::to_string((double) price / PRICE_FACTOR)};
                return it->second;
            } else {
                auto it = bids.find(price);
                if (it == bids.end()) throw std::invalid_argument{"No bid level at " + std::to_string((double) price / PRICE_FACTOR)};
                return it->second;
            }
        }

        int getBestOffer(bool isAsk) {
            return getLevelByIndex(0, isAsk).price;
        }

        int getMidPrice() {
            return (getBestOffer(0) + getBestOffer(1)) / 2;
        }
    private:
        std::string symbol;
        std::unordered_map<int, Order> ordersById;
        std::map<int, PriceLevel> asks;
        std::map<int, PriceLevel, std::greater<int>> bids;

        PriceLevel* getLevelPointer(int price, bool isAsk) {
            if (isAsk) {
                return &asks[price];
            } else {
                return &bids[price];
            }
        }
};

std::unordered_map<std::string, Instrument> instruments;

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

    for (int i = 0; i < 241124; i++) {
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
    std::cout << instruments["J"].getLevelByPrice(1160650, side["B"]) << "\n";
    std::cout << instruments["J"].getOrderById(2893934) << "\n";
    std::cout << ((float) clock() - t)/CLOCKS_PER_SEC << "\n";
}

//some library to read json
//order books can be priority queues with some additional info stored
//or maybe some map from price to a map of order id -> data
//map (or vector) from ids to order data
//<100ms latency
//maps have to be ordered differently by side
//store price level data as linkedlist or map

