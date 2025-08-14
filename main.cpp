#include "include/json.hpp"
#include "lib.cpp"
#include <atomic>
#include <fstream>
#include <thread>

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

const size_t BUFFER_SIZE = 10;

std::atomic<bool> doneWriting = false;
std::counting_semaphore<BUFFER_SIZE> numFilled{0};
std::mutex bufferManip;
std::binary_semaphore programDoneManip{1};

RingBuffer<L1Datum> L1Buf(BUFFER_SIZE);
std::ofstream L1Stream("l1.out");
void readBufferTask() {
    while (true) {
        programDoneManip.acquire();
        //std::cout << "hi\n";
        //std::cout << numFilled.get_count() << "\n";
        if (doneWriting) {
            numFilled.try_acquire(); //should pass, since we just incremented
            programDoneManip.release(); //doesn't really matter where this goes, since in this branch we've already updated doneWriting
            //std::cout << "a\n" << std::flush;
            if (!numFilled.try_acquire()) return;
            else numFilled.release();
        } else { 
            //std::cout << "b\n";
            if (!numFilled.try_acquire()) {
                programDoneManip.release();
                //std::cout << "c\n" << std::flush;
                //if doneWriting update comes in right here, we immediately acquire and return (equivalent to getting woken up)
                numFilled.acquire();
                if (doneWriting) return;
            }
            programDoneManip.release();
        }
        L1Datum L1D;
        {
            std::lock_guard<std::mutex> g(bufferManip);
            L1D = L1Buf.get();
        }
        //process L1D below

        L1Stream << toCsvLine(L1D) << "\n";
    }
}

void writeBuffer(L1Datum L1D) { //not a task; calls only when there's something to add
    {
        std::lock_guard<std::mutex> g(bufferManip);
        L1Buf.add(L1D);
        //std::cout << toCsvLine(L1D) << "\n";
    }
    numFilled.release();
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
        instruments[sym].setCallback(&writeBuffer);
    }

    L1Stream << "recv_time,symbol,bid_price,bid_size,ask_price,ask_size\n";
    std::thread readBufThread(readBufferTask);

    std::string type, data;
    for (int i = 0; i < 100000; i++) { std::cin >> type >> data; //use for partial reads (testing)
    //while (std::cin >> type >> data) {
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
                symbol
            });
        } else if (type == "OrderCanceled:") {
            instrument->removeOrder(j["orderId"].template get<int>(), j["exchTime"].template get<timestamp>());
        } else if (type == "OrderExecuted:") {
            instrument->executeOrder(j["orderId"].template get<int>(), j["execQty"].template get<int>(), j["exchTime"].template get<timestamp>());
        } else if (type == "Trade:") {
            //don't have to do anything yet
        } else {
            std::cerr << "Invalid type for message " << type << " " << data << "\n";
        }
    }

    programDoneManip.acquire();
    doneWriting = true;
    numFilled.release();
    programDoneManip.release();
    readBufThread.join(); //should terminate quickly

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
