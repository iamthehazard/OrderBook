//#include "main.cpp"
#define CATCH_CONFIG_MAIN
#include "include/catch.hpp"
#include "lib.cpp"

TEST_CASE("Order equality") {
    Order o1 = {
        16235,
        134968019283,
        5891,
        600,
        B,
        "A"
    };
    Order o2 = {
        16235,
        134968019283,
        5891,
        600,
        B,
        "B"
    };
    Order o3 = {
        16235,
        134968019283,
        5891,
        600,
        S,
        "A"
    };
    Order o4 = {
        16235,
        134968019283,
        5891,
        600,
        B,
        "A"
    };
    Order o5 = {
        1235,
        10469091235,
        106000,
        2340,
        B,
        "A"
    };
    REQUIRE(o1 == o1);
    REQUIRE(o1 != o2);
    REQUIRE(o1 != o3);
    REQUIRE(o1 == o4);
    REQUIRE(o1 != o5);
}

TEST_CASE("PriceLevel equality") {
    PriceLevel p1 = {
        160,
        600,
        2,
        {
            {0, 0, 0, 0, B, "A"},
            {5, 100, 60, 32, B, "A"}
        }
    };
    PriceLevel p2 = {
        160,
        600,
        2,
        {
            {0, 0, 0, 0, B, "A"},
            {5, 100, 60, 32, B, "A"}
        }
    };
    PriceLevel p3 = {
        160,
        600,
        2,
        {
            {5, 100, 60, 32, B, "A"},
            {0, 0, 0, 0, B, "A"}
        }
    };
    PriceLevel p4 = {
        160,
        600,
        2,
        {
            {0, 0, 0, 0, B, "A"}
        }
    };
    PriceLevel p5 = {
        177,
        600,
        2,
        {
            {0, 0, 0, 0, B, "A"},
            {5, 100, 60, 32, B, "A"}
        }
    };
    PriceLevel p6 = {
        160,
        600,
        2,
        {
            {0, 0, 0, 0, B, "A"},
            {4, 100, 60, 32, B, "A"}
        }
    };
    REQUIRE(p1 == p2);
    REQUIRE(p1 != p3);
    REQUIRE(p1 != p4);
    REQUIRE(p1 != p5);
    REQUIRE(p1 != p6);

    REQUIRE(p3 == p3);
}

TEST_CASE("adding") {
    Instrument ins("A");

    SECTION("empty properly throws") {
        REQUIRE_THROWS(ins.getOrderById(0));
        REQUIRE_THROWS(ins.getLevelByIndex(0, S));
        REQUIRE_THROWS(ins.getLevelByIndex(1, B));
        REQUIRE_THROWS(ins.getLevelByPrice(5, S));
        REQUIRE_THROWS(ins.getLevelByPrice(10052035, B));
    }

    Order o1 = {
        0,
        4,
        195900,
        50,
        S,
        "A"
    };
    PriceLevel pl1 = {
        195900,
        50,
        1,
        {o1}
    };
    SECTION("add one order") {
        ins.addOrder(o1);

        REQUIRE_NOTHROW(ins.getOrderById(0)); //basically checking that getOrderPtr doesn't throw anything
        REQUIRE(ins.getOrderById(0) == o1);

        REQUIRE(ins.getLevelByIndex(0, S) == pl1);
        REQUIRE_THROWS(ins.getLevelByIndex(1, S));
        REQUIRE_THROWS(ins.getLevelByIndex(0, B));

        REQUIRE(ins.getLevelByPrice(195900, S) == pl1);
    }

    Order o2 = {
        16,
        58921,
        195500,
        10,
        S,
        "A"
    };
    PriceLevel pl2 = {
        195500,
        10,
        1,
        {o2}
    };
    SECTION("add another order, diff price") {
        ins.addOrder(o1);
        ins.addOrder(o2);
        REQUIRE_NOTHROW(ins.getLevelByIndex(1, S));
        REQUIRE(ins.getLevelByIndex(0, S) == pl2);
        REQUIRE(ins.getLevelByIndex(1, S) == pl1);

        REQUIRE(ins.getLevelByPrice(195900, S) == pl1);
        REQUIRE(ins.getLevelByPrice(195500, S) == pl2);
    }

    Order o3 = {
        581,
        4010293,
        195900,
        20,
        S,
        "A"
    };
    PriceLevel pl3 = {
        195900,
        70,
        2,
        {o1, o3}
    };
    SECTION("add another order, same price") {
        ins.addOrder(o1);
        ins.addOrder(o2);
        ins.addOrder(o3);
        REQUIRE_NOTHROW(ins.getLevelByIndex(1, S));
        REQUIRE_THROWS(ins.getLevelByIndex(2, S));
        
        
        REQUIRE(ins.getLevelByIndex(1, S) == pl3);
        REQUIRE(ins.getLevelByPrice(195900, S) == pl3);
        REQUIRE(ins.getLevelByPrice(195500, S) == pl2);
    }

    Order o4 = {
        15092,
        5981,
        196020,
        500,
        B,
        "A"
    };
    SECTION("add order to other side") {
        ins.addOrder(o1);
        ins.addOrder(o2);
        ins.addOrder(o3);
        ins.addOrder(o4);
        REQUIRE_THROWS(ins.getLevelByIndex(2, S));
        REQUIRE_NOTHROW(ins.getLevelByIndex(0, B));
        REQUIRE_THROWS(ins.getLevelByPrice(196020, S));
        REQUIRE_NOTHROW(ins.getLevelByPrice(196020, B));
    }
}