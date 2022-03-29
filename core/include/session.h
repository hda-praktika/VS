#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include <boost/asio.hpp>

#include "http.h"
#include "http_error.h"

namespace core {
template <typename Socket> class session {
public:
    class req : public http::req {
        friend session;

        Socket& s_;
        bool header_read_ { false };

        boost::asio::awaitable<void> async_read_header() {
            co_await http::async_read_request(s_, buffer, *this, boost::asio::use_awaitable);
            header_read_ = true;
        }

    public:
        std::string body;
        boost::asio::dynamic_string_buffer<char, std::char_traits<char>, std::allocator<char>> buffer;

        req(Socket& s)
            : s_(s)
            , buffer(body) { }
        
        Socket&& get_socket() && {
            return std::move(s_);
        }

        void async_read_request() {
            return [](req& self) mutable -> boost::asio::awaitable<std::size_t> {
                if (!self.header_read_) {
                    co_await self.async_read_header();
                }
                auto written = co_await boost::asio::async_write(self.s_, self.buffer, boost::asio::use_awaitable);
                co_return written;
            }(*this);
        }
    };

    class res : public http::res {
        Socket& s_;
        bool header_written_ { false };

    public:
        res(Socket& s)
            : s_(s) { }

        boost::asio::awaitable<void> async_write_header() {
            if(!header_written_) {
                co_await http::async_write_response(s_, *this, boost::asio::use_awaitable);
                header_written_ = true;
            }
        }

        template <typename Buffer> auto async_write(Buffer&& buffer) {
            return [](res& self, Buffer&& buffer) mutable -> boost::asio::awaitable<std::size_t> {
                co_await self.async_write_header();
                auto written = co_await boost::asio::async_write(self.s_, std::forward<Buffer>(buffer), boost::asio::use_awaitable);
                co_return written;
            }(*this, std::forward<Buffer>(buffer));
        }
    };

    using handler = std::function<boost::asio::awaitable<void>(res&, req&)>;

    template<typename CompletionToken>
    static void co_spawn(Socket socket, handler h, CompletionToken&& token) {
        boost::asio::co_spawn(socket.get_executor(), [socket = std::move(socket), h = std::move(h)]() mutable -> boost::asio::awaitable<void> {
            // Request und Response Objekte anlegen
            req req{socket};
            res res{socket};

            // Annahme der HTTP-Verbindung
            try {
                // 1. Schritt: Header lesen und parsen
                co_await req.async_read_header();

                // 2. Schritt: Standardprotokoll bei der Antwort auf das Protokoll der Request setzen
                res.protocol = req.protocol;
            } catch(std::system_error& err) {
                if(err.code() != http::make_error_code(http::error::malformed_request) && err.code() != http::make_error_code(http::error::malformed_field)) {
                    throw err;
                }
                // Bei einer ungültigen Request eine Bad-Request antworten
                res.status_code = http::status_code::bad_request;
            }
            if(res.status_code == http::status_code::bad_request) {
                co_await res.async_write_header();
                co_return;
            }

            // 3. Schritt: Weitergabe der Kontrolle an den Handler
            co_await h(res, req);

            // 4. Schritt: Sicherstellen, dass überhaupt ein Antwortheader geschrieben wurde,
            // falls der Handler das nicht bereits getan haben sollte.
            co_await res.async_write_header();
        }, std::forward<CompletionToken>(token));
    }
};
};
