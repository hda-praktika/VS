#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include <nlohmann/json.hpp>

namespace core {
enum class producer_type : std::uint8_t {
    coal,
    solar,
    wind,
    nuclear,
    water,
};
enum class consumer_type : std::uint8_t { personal, industrial };

// Daten, die von den Erzeugern/Verbrauchern an die Zentrale geschickt werden
struct notification {
    std::string id;
    std::uint64_t power;
    double pos_x;
    double pos_y;
    std::variant<producer_type, consumer_type> type;
    std::int64_t timestamp;

    nlohmann::json to_json() const {
        return nlohmann::json {
            {"id", id},
            {"power", power},
            {"pos_x", pos_x},
            {"pos_y", pos_y},
            {"type", type.index()},
            {"subtype", std::visit([](auto subtype) { return static_cast<std::uint8_t>(subtype); }, type)},
            {"timestamp", timestamp}
        };
    }

    std::string encode() const {
        return to_json().dump();
    }

    void decode(std::string_view str) {
        auto doc = nlohmann::json::parse(str);

        id = doc["id"].get<std::string>();
        power = doc["power"].get<decltype(power)>();
        pos_x = doc["pos_x"].get<decltype(pos_x)>();
        pos_y = doc["pos_y"].get<decltype(pos_y)>();
        switch (doc["type"].get<int>()) {
        case 0:
            type = doc["subtype"].get<producer_type>();
            break;
        case 1:
            type = doc["subtype"].get<consumer_type>();
            break;
        default:
            throw std::runtime_error { "Nicht erlaubter index für type" };
        }
        timestamp = doc["timestamp"].get<decltype(timestamp)>();
    }
};

};