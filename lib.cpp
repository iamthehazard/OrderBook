#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <ext/pb_ds/assoc_container.hpp>
#include <map>
//#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <semaphore>

//example lines (so I don't have to keep opening events.in)
//NewOrder: {"exchTime":1725412500115000,"orderId":1591,"price":113.26,"qty":100,"recvTime":1725413100093350,"side":"S","symbol":"E"}
//OrderCanceled: {"exchTime":1725412516673000,"orderId":36941,"recvTime":1725413100093350,"symbol":"E"}
//Trade: {"exchTime":1725413100000000,"price":118.7,"qty":50,"recvTime":1725413100693106,"symbol":"F","tradeId":"36581","tradeTime":1725413100000000}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":0,"orderId":78849,"recvTime":1725413100693106,"symbol":"F"}
//OrderExecuted: {"exchTime":1725413100000000,"execQty":50,"leavesQty":10,"orderId":45517,"recvTime":1725413100693106,"symbol":"F"}

const int PRICE_FACTOR = 10000; //should be divisible by 2000
const int UNDEF_PRICE = INT32_MAX;

enum Side {
    B = 0,
    S = 1
};

std::string to_string(Side s) {
    return s ? "buy" : "sell";
}

using timestamp = uint64_t;

struct Order {
    int id;
    timestamp exchTime;
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
    //side?
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

//a single snapshot of L1 data
struct L1Datum {
    timestamp exchTime;
    //timestamp recvTime //add?
    int price[2];
    int volume[2];
    int count[2];

    std::string symbol;
};

template<class T>
class RingBuffer {
    public:
        RingBuffer() : size(0) {}

        RingBuffer(size_t sz) : size(sz), buffer(sz) {}

        RingBuffer(int sz, bool toLog) : size(sz), buffer(sz), LOG_WHEN_INVALID(toLog) {}

        bool add(T element) {
            if (numFilled == size) {
                //overwrite
                if (LOG_WHEN_INVALID) std::clog << "Overwrote with no available capacity.\n"; //ideally this doesn't happen; I don't think this breaks the code if it does, just lose some output
                buffer[writePos++] = element;
                if (writePos == size) writePos = 0;
                readPos++;
                if (readPos == size) readPos = 0;
                return false;
            }
            buffer[writePos++] = element;
            numFilled++;
            if (writePos == size) writePos = 0; //should be faster than %
            return true;
        }

        T get() {
            if (numFilled == 0) {
                if (LOG_WHEN_INVALID) std::clog << "Tried to read with no elements.\n";
                return T();
            }
            T toReturn = buffer[readPos++];
            numFilled--;
            if (readPos == size) readPos = 0;
            return toReturn;
        }

        size_t count() {
            return numFilled;
        }
    private:
        size_t size;
        std::vector<T> buffer;

        size_t readPos, writePos = 0;
        size_t numFilled = 0;

        bool LOG_WHEN_INVALID = false; //can turn off for performance reasons/on for debug?
};

class Instrument final {
    public:
        Instrument() = default;

        explicit Instrument(std::string const& sym) {
            symbol = sym;
            L1 = {
                0,
                UNDEF_PRICE,
                UNDEF_PRICE,
                0,
                0,
                0,
                0,
                symbol
            };
        }

        //if we want to specify an initialization time, I guess?
        /*explicit Instrument(std::string const& sym, timestamp startTime) {
            symbol = sym;
            L1 = {
                startTime,
                UNDEF_PRICE,
                UNDEF_PRICE,
                0,
                0,
                0,
                0,
                symbol
            };
        }*/

        void addOrder(Order const& order) {
            bool L1Update = L1.price[order.side] == UNDEF_PRICE || order.price == L1.price[order.side] || sideBookComp<int>(order.side)(order.price, L1.price[order.side]);

            auto pl = getLevelPointer(order.price, order.side);
            pl->price = order.price;
            ordersById[order.id] = pl->orders.insert(pl->orders.end(), order);
            pl->volume += order.qty;
            pl->count++;
            
            if (L1Update) {
                //std::cout << "add order L1 chg\n";
                callbackL1(order.exchTime);
            }
        }

        void removeOrder(Orders::iterator it, timestamp time) { //honestly can be private
            auto const& order = *it;
            int orderId = order.id;
            bool L1Update = L1.price[order.side] == UNDEF_PRICE || order.price == L1.price[order.side] || sideBookComp<int>(order.side)(order.price, L1.price[order.side]);
            auto pl = getLevelPointer(order.price, order.side);

            pl->volume -= order.qty;
            pl->count--;
            if (pl->count == 0) {
                bookSides[order.side].priceLevels.erase(order.price); //maybe not ideal performance-wise; change getLevelPointer to iterator?
            } else {
                pl->orders.erase(it);
            }

            ordersById.erase(orderId);
            //if update occurs at or better than cur best
            if (L1Update) {
                //std::cout << "remove order L1 chg\n";
                callbackL1(time);
            }
        }
        
        void removeOrder(int id, timestamp time) {
            auto it = getOrderPtr(id);
            removeOrder(it, time);
        }

        //one issue here: when a trade is executed at the bbo the L1 callback will be triggered twice (when in reality it should only trigger after the trade finishes)
        //although this kind of generally ties into issues that arise from the fact that we're not using packets (similar to the "aggressive orders that get immediately filled" but show up in our book history)
        void executeOrder(Orders::iterator it, int execQty, timestamp time) {
            auto const& order = *it;
            if (order.qty < execQty) throw std::invalid_argument{"execQty " + std::to_string(execQty) + " greater than order qty " + std::to_string(order.qty)};
            if (order.qty == execQty) removeOrder(it, time);
            else {
                bool L1Update = L1.price[order.side] == UNDEF_PRICE || order.price == L1.price[order.side] || sideBookComp<int>(order.side)(order.price, L1.price[order.side]);
                it->qty -= execQty;
                getLevelPointer(order.price, order.side)->volume -= execQty;
                //if update occurs at or better than cur best
                if (L1Update) {
                    //std::cout << "exec order L1 chg\n";
                    callbackL1(time);
                }
            }
        }

        void executeOrder(int id, int execQty, timestamp time) {
            auto it = getOrderPtr(id);
            executeOrder(it, execQty, time);
        }

        const Order& getOrderById(int id) {
            return *getOrderPtr(id);
        }

        const PriceLevel& getLevelByIndex(std::size_t index, Side side) {
            if (bookSides[side].priceLevels.size() > index) {
                auto it = bookSides[side].priceLevels.begin();
                if (index != 0) std::advance(it, index); //performance optimization?
                return it->second;
            }
            throw std::invalid_argument{"No " + to_string(side) + " level at index " + std::to_string(index)};
        }

        //performance friendly
        const std::tuple<int, int, int> getLevelDataByIndex(std::size_t index, Side side) {
            if (bookSides[side].priceLevels.size() > index) {
                auto it = bookSides[side].priceLevels.begin();
                if (index != 0) std::advance(it, index); //performance optimization?
                return {it->second.price, it->second.volume, it->second.count};
            }
            throw std::invalid_argument{"No " + to_string(side) + " level at index " + std::to_string(index)};
        }

        const PriceLevel& getLevelByPrice(int price, Side side) {
            auto it = bookSides[side].priceLevels.find(price);
            if (it == bookSides[side].priceLevels.end()) throw std::invalid_argument{"No " + to_string(side) + " level at " + std::to_string((double) price / PRICE_FACTOR)};
            return it->second;
        }

        //TODO: impl getLevelDataByPrice?

        void setCallback(void(*cb)(L1Datum)) {
            callback = cb;
        }

        std::string getSymbol() {
            return symbol;
        }
    private:
        std::string symbol;
        std::unordered_map<int, Orders::iterator> ordersById;
        //__gnu_pbds::gp_hash_table<int, Orders::iterator> ordersById;
        sideBook bookSides[2] = {
            sideBook(B),
            sideBook(S)
        };
        L1Datum L1;
        void(*callback)(L1Datum) = [](auto x) {}; //empty fn

        //will create level if doesn't already exist
        //this is intended - works for addOrder, and when removing/executing the order should already exist
        PriceLevel* getLevelPointer(int price, Side side) {
            return &bookSides[side].priceLevels[price];
        }

        Orders::iterator getOrderPtr(int id) {
            auto it = ordersById.find(id);
            if (it == ordersById.end()) throw std::invalid_argument("No order with id " + std::to_string(id));
            return it->second;
        }

        void callbackL1(timestamp t) {
            //roll into same
            std::tuple<int, int, int> bestBid, bestAsk;
            try {
                bestBid = getLevelDataByIndex(0, B);
            } catch(std::invalid_argument e) {
                bestBid = {UNDEF_PRICE, 0, 0};
            }
            try {
                bestAsk = getLevelDataByIndex(0, S);
            } catch(std::invalid_argument e) {
                bestAsk = {UNDEF_PRICE, 0, 0};
            }
            L1 = {
                t,
                std::get<0>(bestBid),
                std::get<0>(bestAsk),
                std::get<1>(bestBid),
                std::get<1>(bestAsk),
                std::get<2>(bestBid),
                std::get<2>(bestAsk),
                symbol
            };
            
            callback(L1);
            
            //std::thread t1(callback, L1);
            //t1.detach();
        }
};

