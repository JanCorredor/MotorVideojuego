#pragma once

#include <cstdint>
#include <random>


class GameUUID //Universal Unique Identificator (Global Unique Identificator)
{
public:
    GameUUID() : uuid(Generate()) {}
    GameUUID(uint64_t uuid) : uuid(uuid) {}

    operator uint64_t() const { return uuid; }

private:
    uint64_t uuid; //Unisgned Int 64 Bits

    static uint64_t Generate()
    {
        static std::random_device rd;
        static std::mt19937_64 engine(rd());
        static std::uniform_int_distribution<uint64_t> dist;

        return dist(engine);
    }
};