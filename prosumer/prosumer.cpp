#include <chrono>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cxxopts.hpp>

#include "models.h"
#include "script.h"

using namespace boost::asio;
using namespace boost::asio::ip;

bool parse_consumer_producer(cxxopts::ParseResult& result) {
    if (result.count("consumer") == 0 && result.count("producer") == 0) {
        std::cerr << "-C,--consumer oder -P,--producer müssen angegeben werden!" << std::endl;
        exit(1);
    }
    if (result.count("consumer") && result.count("producer")) {
        std::cerr << "-C,--consumer und -P,--producer sind nicht kombinierbar!" << std::endl;
        exit(1);
    }
    return result.count("consumer");
}

auto parse_type(cxxopts::ParseResult& result, bool is_consumer) {
        decltype(core::notification::type) type;
    if (!result.count("type")) {
        if (is_consumer) {
            type = core::consumer_type::personal;
        } else {
            type = core::producer_type::coal;
        }
    } else {
        auto type_str = result["type"].as<std::string>();
        if (is_consumer) {
            static std::unordered_map<std::string_view, core::consumer_type> consumer_types {
                { "personal", core::consumer_type::personal },
                { "industrial", core::consumer_type::industrial },
            };
            if (!consumer_types.contains(type_str)) {
                std::cerr << "Consumer-Typ " << type_str << " existiert nicht!" << std::endl;
                exit(1);
            }
            type = consumer_types[type_str];
        } else {
            static std::unordered_map<std::string_view, core::producer_type> producer_types {
                { "coal", core::producer_type::coal },
                { "nuclear", core::producer_type::nuclear },
                { "solar", core::producer_type::solar },
                { "water", core::producer_type::water },
                { "wind", core::producer_type::wind },
            };
            if (!producer_types.contains(type_str)) {
                std::cerr << "Producer-Typ " << type_str << " existiert nicht!" << std::endl;
                exit(1);
            }
            type = producer_types[type_str];
        }
    }
    return type;
}

script parse_script(cxxopts::ParseResult& result) {
    if (!result.count("script")) {
        std::cerr << "Es wurde kein Lua-Skript angegeben!" << std::endl;
        exit(1);
    }
    auto script_path = result["script"].as<std::string>();
    return { script_path };
}

void load_script_args(cxxopts::ParseResult& result, script& s) {
    auto script_args = result["arg"].as<std::vector<std::string>>();
    s.load_arguments(script_args);
}

auto parse_prosumer_id(cxxopts::ParseResult& result) {
    if (!result.count("id")) {
        auto uuid = boost::uuids::random_generator()();
        std::stringstream ss;
        ss << uuid;
        return ss.str();
    }
    return result["id"].as<std::string>();
}

auto parse_position(cxxopts::ParseResult& result) {
    if(!result.count("X") || !result.count("Y")) {
        std::cerr << "Bitte X- und Y-Koordinaten angeben!" << std::endl;
        exit(1);
    }
    auto x = result["X"].as<double>();
    auto y = result["Y"].as<double>();
    if(x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
        std::cerr << "Koordinaten müssen zwischen 0 und 1 liegen!" << std::endl;
        exit(1);
    }
    return std::make_pair(x, y);
}

int main(int argc, char** argv) {
    static cxxopts::Options options { "prosumer", "Producer und Consumer in einem Programm" };
    // clang-format off
    options.add_options()
        ("I,id", "Prosumer-ID", cxxopts::value<std::string>())
        ("C,consumer", "Consumer-Modus")
        ("P,producer", "Producer-Modus")
        ("T,type", "Typ", cxxopts::value<std::string>())
        ("X", "X-Position", cxxopts::value<double>())
        ("Y", "Y-Position", cxxopts::value<double>())
        ("s,script", "Lua-Skript", cxxopts::value<std::string>())
        ("a,arg", "Lua-Skript Argument", cxxopts::value<std::vector<std::string>>())
        ("h,help", "Hilfe-Seite anzeigen");
    // clang-format on
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    bool is_consumer = parse_consumer_producer(result);
    auto type = parse_type(result, is_consumer);
    auto s = parse_script(result);
    load_script_args(result, s);
    auto prosumer_id = parse_prosumer_id(result);
    double x, y;
    std::tie(x, y) = parse_position(result);

    io_context ctx { 1 };
    udp::endpoint endpoint { address::from_string("127.0.0.1"), 3000 };
    auto socket = udp::socket { ctx };
    socket.open(udp::v4());
    s.set_notify_handler([&](std::uint64_t power) {
        auto now = std::chrono::system_clock::now();
        auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        core::notification notification {
            .id = prosumer_id,
            .power = power,
            .pos_x = x,
            .pos_y = y,
            .type = type,
            .timestamp = unix_timestamp,
        };
        auto str = notification.encode();

        socket.send_to(buffer(str), endpoint);
    });

    s.run();
}
