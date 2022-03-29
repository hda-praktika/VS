#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>

#include "session.h"

namespace core {
template <typename Socket> class router {
    using req = typename session<Socket>::req;
    using res = typename session<Socket>::res;

    using predicate = std::function<bool(req&)>;
    using handler
        = std::function<boost::asio::awaitable<void>(res&, req&, std::function<boost::asio::awaitable<void>()>)>;
    using middleware = std::pair<predicate, handler>;

public:
    class next {
        using middleware_iterator = typename std::vector<middleware>::const_iterator;

        middleware_iterator it_;
        middleware_iterator end_;
        req& req_;
        res& res_;
        bool moved_to_next{false};

        void move_to_next() {
            if(moved_to_next) {
                return;
            }

            for (; it_ != end_; ++it_) {
                if (!(*it_).first(req_)) {
                    continue;
                }
                return;
            }
            throw std::runtime_error { "Keine Middleware mehr vorhanden" };
        }

    public:
        next(middleware_iterator it, middleware_iterator end, req& req, res& res)
            : it_(it)
            , end_(end)
            , req_(req)
            , res_(res) { }

        boost::asio::awaitable<void> operator()() {
            move_to_next();
            return (*it_).second(res_, req_, next { it_ + 1, end_, req_, res_ });
        }
    };

    class group {
        std::vector<middleware> middleware_;

        static constexpr auto true_predicate = [](auto& req) { return true; };

        struct exact_match_t {};

    public:
        static constexpr auto exact_match = exact_match_t{};

        template <typename Handler> void use(Handler&& h) {
            middleware_.emplace_back(true_predicate, std::forward<Handler>(h));
        }

        template <typename Handler> void use(std::string_view prefix, Handler&& h) {
            std::string prefix_str {prefix};
            middleware_.emplace_back([prefix = std::move(prefix_str)](auto& req){
                return req.url.starts_with(prefix);
            }, std::forward<Handler>(h));
        }

        template <typename Handler>
        void use(std::string_view match, exact_match_t, Handler&& h) {
            std::string match_str {match};
            middleware_.emplace_back([match = std::move(match_str)](auto& req){
                return req.url == match;
            }, std::forward<Handler>(h));
        }

        boost::asio::awaitable<void> operator()(res& res, req& req) {
            co_await(next { middleware_.cbegin(), middleware_.cend(), req, res })();
        }

        boost::asio::awaitable<void> operator()(res& res, req& req, auto next) { return (*this)(res, req); }
    };

private:
    group root_group_;

public:
    static constexpr auto exact_match = group::exact_match;

    router() {
        // Standard Middleware für Fehlerbehandlung
        root_group_.use([](auto& res, auto& req, auto next) -> boost::asio::awaitable<void> {
            try {
                co_await next();
            } /* catch (std::runtime_error& err) {

             }*/
            catch (std::exception& err) {
                std::cerr << "Bei der Behandlung der HTTP-Verbindung ist ein Fehler aufgetreten: " << err.what()
                          << std::endl;
            } catch (...) {
                std::cerr << "Bei der Behandlung der HTTP-Verbindung ist ein unbekannter Fehler aufgetreten"
                          << std::endl;
            }
        });
    }

    template <typename... Args> void use(Args&&... args) { root_group_.use(std::forward<Args>(args)...); }

    template <typename CompletionToken> auto handle_connection(Socket socket, CompletionToken&& token) {
        return session<Socket>::co_spawn(std::move(socket), root_group_, std::forward<CompletionToken>(token));
    }
};

}
