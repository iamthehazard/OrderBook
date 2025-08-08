#include <iostream>
#include <list>
#include <unordered_map>
#include <map>

const int PRICE_FACTOR = 10000; //should be divisible by 2000

std::map<std::string, bool> side = {
    {"S", 1},
    {"B", 0}
};

struct Order {
    int id;
    long long exchTime;
    int price; //price, always int
    int qty;
    bool isAsk;
    std::string symbol;
    //std::list<Order>::iterator it;
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
    return stream;
}

bool operator==(const Order& o1, const Order& o2) {
    if (o1.id != o2.id ||
        o1.exchTime != o2.exchTime ||
        o1.price != o2.price ||
        o1.qty != o2.qty ||
        o1.isAsk != o2.isAsk ||
        o1.symbol != o2.symbol) {
            return false;
    }
    return true;
}

bool operator!=(const Order& o1, const Order& o2) {
    return !(o1 == o2);
}

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
            pl->volume += order.qty;
            pl->count++;

            ordersById[order.id] = std::prev(pl->orders.end());;
        }

        void removeOrder(std::list<Order>::iterator it) {
            auto order = *it;
            auto pl = getLevelPointer(order.price, order.isAsk);

            pl->orders.erase(it);
            if (pl->orders.empty()) {
                if (order.isAsk) asks.erase(order.price);
                else bids.erase(order.price);
            }
            pl->volume -= order.qty;
            pl->count--;

            ordersById.erase(order.id);
        }
        
        void removeOrder(int id) {
            auto it = getOrderPtr(id);
            removeOrder(it);
        }

        void executeOrder(std::list<Order>::iterator it, int execQty) {
            auto order = *it;
            if (order.qty == execQty) removeOrder(order.id);
            else {
                it->qty -= execQty;
                getLevelPointer(order.price, order.isAsk)->volume -= execQty;
            }
        }

        void executeOrder(int id, int execQty) {
            auto it = getOrderPtr(id);
            executeOrder(it, execQty);
        }

        Order getOrderById(int id) {
            return *getOrderPtr(id);
        }

        PriceLevel getLevelByIndex(std::size_t index, bool isAsk) {
            if (isAsk) {
                if (asks.size() > index) {
                    auto it = asks.begin();
                    std::advance(it, index);
                    return it->second;
                }
                throw std::invalid_argument{"No ask level at index " + std::to_string(index) + ", current number of PriceLevels is " + std::to_string(asks.size())};
            } else {
                if (bids.size() > index) {
                    auto it = bids.begin();
                    std::advance(it, index);
                    return it->second;
                }
                throw std::invalid_argument{"No bid level at index " + std::to_string(index) + ", current number of PriceLevels is " + std::to_string(asks.size())};
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
        std::unordered_map<int, std::list<Order>::iterator> ordersById;
        std::map<int, PriceLevel> asks;
        std::map<int, PriceLevel, std::greater<int>> bids;

        PriceLevel* getLevelPointer(int price, bool isAsk) {
            if (isAsk) {
                return &asks[price];
            } else {
                return &bids[price];
            }
        }

        std::list<Order>::iterator getOrderPtr(int id) {
            auto it = ordersById.find(id);
            if (it == ordersById.end()) throw std::invalid_argument("No order with id " + std::to_string(id));
            return it->second;
        }
};

std::unordered_map<std::string, Instrument> instruments;

/*int main() {
    std::map<int, PriceLevel> mp;
    auto pt = &mp[0];
    std::cout << *pt << "\n";
    pt->price = 5;
    std::cout << mp.size() << "\n";
    auto pt2 = &mp[7];
    std::cout << mp.size() << "\n";
}*/