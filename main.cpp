#include "include/json.hpp"
#include <iostream>
#include <list>
#include <sstream>
#include <unordered_map>
#include <ext/pb_ds/assoc_container.hpp>

using json = nlohmann::json;

//example lines (so I don't have to keep opening events.in)
//NewOrder: {"exchTime":1725412500115000,"orderId":1591,"price":113.26,"qty":100,"recvTime":1725413100093350,"side":"S","symbol":"E"}
//OrderCanceled: {"exchTime":1725412516673000,"orderId":36941,"recvTime":1725413100093350,"symbol":"E"}
//Trade: {"exchTime":1725413100000000,"price":118.7,"qty":50,"recvTime":1725413100693106,"symbol":"F","tradeId":"36581","tradeTime":1725413100000000}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":0,"orderId":78849,"recvTime":1725413100693106,"symbol":"F"}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":10,"orderId":45517,"recvTime":1725413100693106,"symbol":"F"}

const int PRICE_FACTOR = 10000; //should be divisible by 2000

enum Side {
    B = 0,
    S = 1
};

const std::map<std::string, Side> sideMap= {
    {"B", B},
    {"S", S}
};

std::string to_string(Side s) {
    return s ? "buy" : "sell";
}

struct Order {
    int id;
    long long exchTime;
    int price; //price * 1000, always int
    int qty;
    Side side;
    std::string symbol;
};
using Orders = std::list<Order>;
using OrderRef = Orders::iterator;

struct PriceLevel {
    int price;
    int volume = 0;
    int count = 0; //# of orders
    Orders orders;
};

std::string to_string(const Order& o) {
    std::ostringstream os;
    os << "{exchTime:" << o.exchTime << ",id:" << o.id << ",price:" << (double) o.price / PRICE_FACTOR << ",qty:" << o.qty << ",side:" << to_string(o.side) << ",symbol:" << o.symbol << "}";
    return os.str();
}

std::ostream& operator<<(std::ostream& stream, const Order& o) {
    stream << to_string(o);
    return stream;
}

std::string to_string(const Orders& orders) {
    std::ostringstream os;
    if (orders.empty()) {
        os << "[]";
    } else {
        os << "[\n";
        for (const auto& o : orders) os << "\t" << o << "\n";
        os << "]";
    }
    return os.str();
}


std::ostream& operator<<(std::ostream& stream, const Orders& orders) {
    stream << to_string(orders);
    return stream;
}

std::string to_string(const PriceLevel pl) {
    std::ostringstream os;
    os << "{price:" << (double) pl.price / PRICE_FACTOR << ",volume:" << pl.volume << ",count:" << pl.count << ",orders:" << pl.orders << "}";
    return os.str();
}

std::ostream& operator<<(std::ostream& stream, const PriceLevel pl) {
    stream << to_string(pl);
    return stream;
}

template<typename T>
struct sideBookComp {
    sideBookComp(Side dir) : do_greater(dir) {}
    bool operator() (const T& x, const T& y) const {
        return do_greater == B ? x > y : x < y;
    }
    bool do_greater;
};

class sideBook final {
    //not going to make these private; we will be returning references to them anyways (only for internal instrument use)
    public:
        Side side;
        std::map<int, PriceLevel, sideBookComp<int>> priceLevels;

        sideBook(Side s) : side(s), priceLevels(side) {}
};

class Instrument final {
    public:
        Instrument() = default;

        explicit Instrument(std::string const& sym) {
            symbol = sym;
        }

        void addOrder(Order const& order) {
            auto pl = getLevelPointer(order.price, order.side);
            pl->price = order.price;
            ordersById[order.id] = pl->orders.insert(pl->orders.end(), order);
            pl->volume += order.qty;
            pl->count++;
        }

        void removeOrder(Orders::iterator it) {
            auto const& order = *it;
            auto pl = getLevelPointer(order.price, order.side);

            if (pl->orders.size() == 1) {
                bookSides[order.side].priceLevels.erase(order.price);
                return;
            }
            pl->volume -= order.qty;
            pl->count--;
            pl->orders.erase(it);

            ordersById.erase(order.id);
        }
        
        void removeOrder(int id) {
            auto it = getOrderPtr(id);
            removeOrder(it);
        }

        void executeOrder(Orders::iterator it, int execQty) {
            auto const& order = *it;
            if (order.qty < execQty) throw std::invalid_argument{"execQty " + std::to_string(execQty) + " greater than order qty " + std::to_string(order.qty)};
            if (order.qty == execQty) removeOrder(order.id);
            else {
                it->qty -= execQty;
                getLevelPointer(order.price, order.side)->volume -= execQty;
            }
        }

        void executeOrder(int id, int execQty) {
            auto it = getOrderPtr(id);
            executeOrder(it, execQty);
        }

        const Order& getOrderById(int id) {
            return *getOrderPtr(id);
        }

        const PriceLevel& getLevelByIndex(std::size_t index, Side side) {
            if (bookSides[side].priceLevels.size() > index) {
                auto it = bookSides[side].priceLevels.begin();
                std::advance(it, index);
                return it->second;
            }
            throw std::invalid_argument{"No " + to_string(side) + " level at index " + std::to_string(index)};
        }

        const PriceLevel& getLevelByPrice(int price, Side side) {
            auto it = bookSides[side].priceLevels.find(price);
            if (it == bookSides[side].priceLevels.end()) throw std::invalid_argument{"No " + to_string(side) + " level at " + std::to_string((double) price / PRICE_FACTOR)};
            return it->second;
        }

        int getBestOffer(Side side) {
            return getLevelByIndex(0, side).price;
        }

        int getMidPrice() {
            return (getBestOffer(B) + getBestOffer(S)) / 2;
        }
    private: 
        std::string symbol;
        std::unordered_map<int, Orders::iterator> ordersById;
        //__gnu_pbds::gp_hash_table<int, Orders::iterator> ordersById;
        sideBook bookSides[2] = {
            sideBook(B),
            sideBook(S)
        };

        PriceLevel* getLevelPointer(int price, Side side) {
            return &bookSides[side].priceLevels[price];
        }

        Orders::iterator getOrderPtr(int id) {
            auto it = ordersById.find(id);
            if (it == ordersById.end()) throw std::invalid_argument("No order with id " + std::to_string(id));
            return it->second;
        }
};

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
