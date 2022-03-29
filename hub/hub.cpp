#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>

#include "http.h"
#include "models.h"
#include "router.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <filesystem.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;

static constexpr auto throw_exception = [](std::exception_ptr eptr, auto&&...) {
    if (eptr) {
        std::rethrow_exception(eptr);
    }
};

static constexpr auto log_exception = [](std::exception_ptr eptr) {
    if (eptr) {
        try {
            std::rethrow_exception(eptr);
        } catch (std::exception& err) { std::cerr << "Ein Fehler trat auf: " << err.what() << std::endl; } catch (...) {
            std::cerr << "Ein unbekannter Fehler trat auf" << std::endl;
        }
    }
};

static constexpr auto logging_middleware = [](auto& res, auto& req, auto next) -> awaitable<void> {
    std::cout << "Eingehende Request an " << req.url << std::endl;
    co_await next();
    std::cout << "Ausgehende Response mit " << static_cast<int>(res.status_code) << std::endl;
};

struct state {
    // Wir speichern die letzten 120 Einträge
    static constexpr std::size_t history_size = 120;

    std::unordered_map<std::string, std::list<core::notification>> prosumers{};
    std::unordered_map<std::string, steady_timer> prosumer_timers;
    std::vector<websocket::stream<tcp::socket>> websockets{};

    awaitable<void> update_prosumer(core::notification notification) {
        auto not_exist = !prosumers.contains(notification.id);
        if (not_exist || prosumers[notification.id].back().timestamp < notification.timestamp) {
            if(prosumers[notification.id].size() == history_size) {
                prosumers[notification.id].pop_front();
            }
            auto id = notification.id;
            prosumers[notification.id].emplace_back(std::move(notification));

            co_await setup_unregister_prosumer_timer(std::move(id));
            co_await broadcast_prosumers();
        }
    }

    void handle_websocket(websocket::stream<tcp::socket> ws) {
        websockets.emplace_back(std::move(ws));
    }

    awaitable<void> broadcast_prosumers() {
        auto doc = nlohmann::json::object({});
        for(const auto& [id, notifications] : prosumers) {
            doc[id] = notifications.back().to_json();
        }
        auto output = doc.dump();
        co_await broadcast(buffer(output));
    }

private:
    template<typename Buffer>
    awaitable<void> broadcast(Buffer&& buffer) {
        for (auto& ws : websockets) {
            ws.async_write(buffer, detached);
        }
        co_return;
    }

    void cancel_timer(std::string id) {
        auto it = prosumer_timers.find(id);
        if(it != prosumer_timers.end()) {
            it->second.cancel();
            prosumer_timers.erase(it);
        }
    }

    awaitable<void> setup_unregister_prosumer_timer(std::string id) {
        cancel_timer(id);

        auto executor = co_await this_coro::executor;
        co_spawn(executor, [this, id = std::move(id)]() -> awaitable<void> {
            using namespace std::chrono_literals;

            auto executor = co_await this_coro::executor;
            steady_timer timer{executor, 5s};
            auto [it, _] = prosumer_timers.emplace(id, std::move(timer));

            co_await it->second.async_wait(use_awaitable);
            co_await unregister_prosumer(std::move(id));
        }, detached);
    }

    awaitable<void> unregister_prosumer(std::string id) {
        std::cout << "Prosumer mit der ID " << id << " wird abgemeldet" << std::endl;
        cancel_timer(id);
        prosumers.erase(id);
        co_await broadcast_prosumers();
    }
};


int main() {
    using router = core::router<tcp::socket>;

    state state;

    io_context ctx { 1 };

    router r;

    // Requests und Respones loggen
    r.use(logging_middleware);

    // URL normalisieren
    r.use([](auto& res, auto& req, auto next) -> awaitable<void> {
        ghc::filesystem::path parsed { "/" };
        parsed /= req.url;
        parsed = ghc::filesystem::weakly_canonical(parsed);
        req.url = parsed.c_str();
        co_await next();
    });

    r.use("/api/v1/prosumers/", router::exact_match, [&state](auto& res, auto& req, auto next) -> awaitable<void> {
        auto doc = nlohmann::json::array({});
        for(const auto& [_, notifications] : state.prosumers) {
            doc.emplace_back(notifications.back().to_json());
        }
        auto output = doc.dump(4);
        res.set_content_length(output.size());
        res.set_content_type("application/json");
        co_await res.async_write(buffer(output));
    });

    r.use("/api/v1/prosumers/", [&state](auto& res, auto& req, auto next) -> awaitable<void> {
        auto prosumer_id = req.url.substr(18);
        if(!prosumer_id.empty() && prosumer_id[prosumer_id.size() - 1] == '/') {
            prosumer_id = prosumer_id.substr(0, prosumer_id.size() - 1);
        }

        if(!state.prosumers.contains(prosumer_id)) {
            std::string output{"Der Prosumer mit ID " + prosumer_id + " existiert nicht."};
            res.status_code = core::http::status_code::not_found;
            res.set_content_length(output.size());
            co_await res.async_write(buffer(output));
            co_return;
        }

        auto doc = nlohmann::json::array({});
        for(auto& notification : state.prosumers[prosumer_id]) {
            doc.emplace_back(notification.to_json());
        }
        auto output = doc.dump(4);
        res.set_content_length(output.size());
        res.set_content_type("application/json");
        co_await res.async_write(buffer(output));
    });

    r.use("/ws", [&state](auto& res, auto& req, auto next) -> awaitable<void> {
        http::request<http::string_body> beast_req;
        beast_req.method_string("GET");
        for (const auto& [field, value] : req.fields) {
            beast_req.set(field, value);
        }

        websocket::stream<tcp::socket> ws { std::move(req).get_socket() };
        co_await ws.async_accept(beast_req, use_awaitable);
        state.handle_websocket(std::move(ws));
    });

    r.use("/", router::exact_match, [](auto& res, auto& req, auto next) -> awaitable<void> {
        req.url = "/index.html";
        co_await next();
    });

    r.use([](auto& res, auto& req, auto next) -> awaitable<void> {
        ghc::filesystem::path target_file{"../frontend"};
        target_file /= req.url.substr(1);
        if(!ghc::filesystem::exists(target_file)) {
            res.status_code = core::http::status_code::not_found;
            co_await res.async_write(buffer(std::string_view{"Not Found"}));
            co_return;
        }

        static std::unordered_map<std::string_view, std::string_view> mime_types{
            {".html", "text/html; charset=utf-8"},
            {".css", "text/css; charset=utf-8"},
            {".js", "application/javascript; charset=utf-8"},
            {".svg", "image/svg+xml; charset=utf-8"}
        };
        auto extension = target_file.extension();
        if(mime_types.contains(extension.c_str())) {
            res.set_content_type(mime_types[extension.c_str()]);
        } else {
            res.set_content_type("text/plain; charset=utf-8");
        }

        std::ifstream input{target_file, std::ios::binary};
        std::string body{std::istreambuf_iterator<char>(input), {}};
        res.set_content_length(body.size());
        co_await res.async_write(buffer(body));
    });

    // HTTP-Server
    co_spawn(
        ctx,
        [&ctx, &r]() mutable -> awaitable<void> {
            tcp::endpoint endpoint { tcp::v4(), 3000 };
            tcp::acceptor acceptor { ctx, endpoint };

            for (;;) {
                tcp::socket socket { ctx };
                co_await acceptor.async_accept(socket, use_awaitable);
                r.handle_connection(std::move(socket), log_exception);
            }

            assert(!"Unreachable");
        },
        throw_exception);

    // UDP-Server
    co_spawn(
        ctx,
        [&ctx, &state]() mutable -> awaitable<void> {
            udp::endpoint endpoint { udp::v4(), 3000 };
            auto socket = use_awaitable.as_default_on(udp::socket { ctx, endpoint });

            std::string str;
            for (;;) {
                str.resize(1024);

                auto length = co_await socket.async_receive(buffer(str));
                str.resize(length);

                core::notification notification;
                notification.decode(str);
                co_await state.update_prosumer(std::move(notification));
            }
        },
        throw_exception);

    ctx.run();
}
