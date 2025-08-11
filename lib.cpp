#include <iostream>
#include <list>
#include <sstream>
#include <unordered_map>
#include <ext/pb_ds/assoc_container.hpp>
#include <map>

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

bool operator==(const Order& o1, const Order& o2) {
    if (o1.id != o2.id ||
        o1.exchTime != o2.exchTime ||
        o1.price != o2.price ||
        o1.qty != o2.qty ||
        o1.side != o2.side ||
        o1.symbol != o2.symbol) {
            return false;
    }
    return true;
}

bool operator!=(const Order& o1, const Order& o2) {
    return !(o1 == o2);
}

struct PriceLevel {
    int price;
    int volume = 0;
    int count = 0; //# of orders
    Orders orders;
};

bool operator==(const PriceLevel& pl1, const PriceLevel& pl2) {
    if (pl1.price != pl2.price || pl1.volume != pl2.volume || pl1.count != pl2.count) return false;
    if (pl1.orders.size() != pl2.orders.size()) return false;
    auto it1 = pl1.orders.begin();
    auto it2 = pl2.orders.begin();
    while (it1 != pl1.orders.end()) {
        if (*it1 != *it2) {
            return false;
        }
        it1++;
        it2++;
    }
    return true;
}

bool operator!=(const PriceLevel& pl1, const PriceLevel& pl2) {
    return !(pl1 == pl2);
}

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

        void removeOrder(Orders::iterator it) { //honestly can be private
            auto const& order = *it;
            auto pl = getLevelPointer(order.price, order.side);

            if (pl->orders.size() == 1) {
                bookSides[order.side].priceLevels.erase(order.price);
                ordersById.erase(order.id);
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

