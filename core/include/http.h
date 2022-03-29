#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <boost/asio.hpp>

#include "http_error.h"
#include "util.h"

namespace core::http {

enum class status_code : unsigned int { ok = 200, bad_request = 400, not_found = 404 };

enum class verb { GET, POST, PUT, PATCH, DELETE };

enum class protocol { http10 = 10, http11 = 11 };

// req repräsentiert eine HTTP-Request
struct req {
    verb verb { verb::GET };
    std::string url { "/" };
    protocol protocol { protocol::http11 };
    std::unordered_map<std::string, std::string> fields {};
};

// res repräsentiert eine HTTP-Antwort
struct res {
    protocol protocol { protocol::http11 };
    status_code status_code { status_code::ok };
    std::unordered_map<std::string, std::string> fields {};

    void set_content_length(std::size_t length) {
        fields["Content-Length"] = std::to_string(length);
    }

    void set_content_type(std::string_view mime_type) {
        fields["Content-Type"] = mime_type;
    }
};

// Hilfsfunktionen zum Senden von Requests/Responses
namespace internal {
    constexpr std::string_view encode_verb(verb verb) noexcept {
        switch (verb) {
        case verb::GET:
            return "GET ";
        case verb::POST:
            return "POST ";
        case verb::PUT:
            return "PUT ";
        case verb::PATCH:
            return "PATCH ";
        case verb::DELETE:
            return "DELETE ";
        }
        return {};
    }

    constexpr std::string_view encode_protocol_request(protocol protocol) noexcept {
        switch (protocol) {
        case protocol::http10:
            return " HTTP/1.0\r\n";
        case protocol::http11:
            return " HTTP/1.1\r\n";
        }
        return {};
    }

    constexpr std::string_view encode_protocol_response(protocol protocol) noexcept {
        switch (protocol) {
        case protocol::http10:
            return "HTTP/1.0 ";
        case protocol::http11:
            return "HTTP/1.1 ";
        }
        return {};
    }

    constexpr std::string_view encode_status_code(status_code code) noexcept {
        // https://tools.ietf.org/html/rfc2616#section-10
        switch (code) {
        case status_code::ok:
            return "200 OK\r\n";
        case status_code::bad_request:
            return "400 Bad Request\r\n";
        case status_code::not_found:
            return "404 Not Found\r\n";
        }
        return "501 Not Implemented";
    }

    template <typename AsyncWriteStream, bool IsResponse, typename ReqRes,
        typename CompletionToken = boost::asio::default_completion_token_t<typename AsyncWriteStream::executor_type>>
    auto async_write_reqres(AsyncWriteStream& stream, const ReqRes& reqres,
        CompletionToken&& token = boost::asio::default_completion_token_t<typename AsyncWriteStream::executor_type> {}) {
        return boost::asio::async_initiate<CompletionToken, void(std::error_code, std::size_t)>(
            [](auto&& completion_handler, AsyncWriteStream& stream, const ReqRes& reqres) {
                // Für das endgültige Schreiben des finalen HTTP-Headers auf das Socket
                // nutzen wir Scatter-Gather I/O, d.h. statt eines großen Puffers nehmen
                // wir eine Liste von Puffern (Puffersequenz), die der Kernel dann
                // selber zusammenkopiert. Dies ist nicht nur eine
                // Performanzoptimierung, sondern macht auch den Code übersichtlicher.
                std::size_t num_bufs = 0;

                if constexpr (IsResponse) {
                    // Ein Puffer hält die HTTP-Protokoll-Version
                    num_bufs += 1;

                    // Ein Puffer hält den Status-Code (+ Reason)
                    num_bufs += 1;
                } else {
                    // Ein Puffer hält das HTTP-Verb
                    num_bufs += 1;

                    // Ein Puffer hält die URL
                    num_bufs += 1;

                    // Ein Puffer hält die HTTP-Protokoll-Version
                    num_bufs += 1;
                }

                // Dann gibt es noch für jedes HTTP-Feld im Header vier Puffer:
                num_bufs += 4 * reqres.fields.size();

                // Und der Header schließt mit einem \r\n ab.
                num_bufs += 1;

                // Puffersequenz allokieren
                auto bufs = std::make_unique<boost::asio::const_buffer[]>(num_bufs);

                // Nun müssen wir die allokierte Puffersequenz noch mit unseren Puffern
                // füllen. Dazu wird zunächst die Statuszeile angefertigt. Leerzeichen
                // und \r\n sind bereits in den encoded_ Variablen drin.
                std::size_t i = 0;
                if constexpr (IsResponse) {
                    auto encoded_protocol = internal::encode_protocol_response(reqres.protocol);
                    auto encoded_status = internal::encode_status_code(reqres.status_code);

                    bufs[i++] = boost::asio::buffer(encoded_protocol);
                    bufs[i++] = boost::asio::buffer(encoded_status);
                } else {
                    auto encoded_verb = internal::encode_verb(reqres.verb);
                    auto encoded_protocol = internal::encode_protocol_request(reqres.protocol);

                    bufs[i++] = boost::asio::buffer(encoded_verb);
                    bufs[i++] = boost::asio::buffer(reqres.url);
                    bufs[i++] = boost::asio::buffer(encoded_protocol);
                }

                // Anschließend alle Header-Felder schreiben
                for (const auto& [field, value] : reqres.fields) {
                    bufs[i++] = boost::asio::buffer(field);
                    bufs[i++] = boost::asio::buffer(std::string_view { ": " });
                    bufs[i++] = boost::asio::buffer(value);
                    bufs[i++] = boost::asio::buffer(std::string_view { "\r\n" });
                }

                // Der Header schließt mit einem \r\n ab.
                assert(i == num_bufs - 1);
                bufs[i] = boost::asio::buffer(std::string_view { "\r\n" });

                // Schließlich muss die Puffersequenz geschrieben werden
                boost::asio::async_write(stream,
                    core::internal::util::const_iterator_pair { bufs.get(), bufs.get() + num_bufs },
                    core::internal::util::make_owning_handler<decltype(bufs)>(
                        std::forward<decltype(completion_handler)>(completion_handler), std::move(bufs)));
            },
            token, std::ref(stream), std::ref(reqres));
    }
} // namespace internal

template <typename AsyncReadStream, typename Response,
    typename CompletionToken = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type>>
auto async_write_response(AsyncReadStream& stream, Response& res,
    CompletionToken&& token = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type> {}) {
    return internal::async_write_reqres<AsyncReadStream, true>(stream, res, std::forward<CompletionToken>(token));
}

template <typename AsyncReadStream, typename Request,
    typename CompletionToken = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type>>
auto async_write_request(AsyncReadStream& stream, Request& req,
    CompletionToken&& token = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type> {}) {
    return internal::async_write_reqres<AsyncReadStream, false>(stream, req, std::forward<CompletionToken>(token));
}

// Hilfsfunktionen zum Empfangen von Requests/Responses
namespace internal {
    template <typename AsyncReadStream, typename DynamicBuffer,
        typename CompletionToken = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type>>
    auto async_read_line(AsyncReadStream& stream, DynamicBuffer& buffer,
        CompletionToken&& token = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type> {}) {
        return boost::asio::async_initiate<CompletionToken, void(std::error_code, std::string_view, std::size_t)>(
            [](auto&& completion_handler, AsyncReadStream& stream, DynamicBuffer& buffer) {
                boost::asio::async_read_until(stream, buffer, "\r\n",
                    [completion_handler = std::forward<decltype(completion_handler)>(completion_handler), &stream,
                        &buffer](std::error_code ec, std::size_t matching_bytes) mutable {
                        if (ec) {
                            completion_handler(ec, std::string_view {}, 0);
                            return;
                        }

                        const auto& input_buf = buffer.data();
                        std::string_view sv { static_cast<const char*>(input_buf.data()), matching_bytes - 2 };
                        completion_handler(ec, sv, matching_bytes);
                    });
            },
            token, std::ref(stream), std::ref(buffer));
    }

    template <typename Response> bool parse_status_line(std::string_view line, Response& res) {
        // Vorab-Checks der Zeile
        if (line.size() < 12 || !line.starts_with("HTTP/1.")) {
            return false;
        }

        if (line[7] == '0') {
            res.protocol = protocol::http10;
        } else if (line[7] == '1') {
            res.protocol = protocol::http11;
        } else {
            return false;
        }

        if (line[8] != ' ') {
            return false;
        }

        std::underlying_type_t<status_code> code;
        auto fc_res = std::from_chars(line.data() + 9, line.data() + 12, code, 10);
        if (fc_res.ec != std::errc {} || fc_res.ptr != (line.data() + 12)) {
            return false;
        }
        res.status_code = static_cast<status_code>(code);

        return true;
    }

    template <typename Request> bool parse_request_line(std::string_view line, Request& req) {
        // HTTP-Verb lesen
        auto first_space = line.find(' ');
        if (first_space == line.npos) {
            return false;
        }
        auto http_verb = line.substr(0, first_space);
        if (http_verb == "GET") {
            req.verb = verb::GET;
        } else if (http_verb == "POST") {
            req.verb = verb::POST;
        } else if (http_verb == "PUT") {
            req.verb = verb::PUT;
        } else if (http_verb == "PATCH") {
            req.verb = verb::PATCH;
        } else if (http_verb == "DELETE") {
            req.verb = verb::DELETE;
        } else {
            return false;
        }

        // URI lesen
        line = line.substr(first_space + 1);
        first_space = line.find(' ');
        if (first_space == line.npos) {
            return false;
        }
        req.url = line.substr(0, first_space);

        // Protokoll-Version lesen
        line = line.substr(first_space + 1);
        if (line == "HTTP/1.0") {
            req.protocol = protocol::http10;
        } else if (line == "HTTP/1.1") {
            req.protocol = protocol::http11;
        } else {
            return false;
        }

        return true;
    }

    template <typename Response> bool parse_field(std::string_view line, Response& res) {
        auto seppos = line.find(": ");
        if (seppos == line.npos) {
            return false;
        }

        std::string field_key { line.data(), seppos };
        std::string_view field_value { line.data() + seppos + 2, line.size() - (seppos + 2) };
        res.fields[field_key] = field_value;
        return true;
    }

    template <bool IsResponse> class async_reqres_reader {
    public:
        template <typename AsyncReadStream, typename DynamicBuffer, typename ReqRes, typename CompletionHandler>
        static inline void init(
            AsyncReadStream& stream, DynamicBuffer& buffer, ReqRes& reqres, CompletionHandler&& handler) {
            async_read_line(stream, buffer,
                [&stream, &buffer, &reqres, handler = std::forward<CompletionHandler>(handler)](
                    std::error_code ec, std::string_view line, std::size_t matching_bytes) mutable {
                    if (ec) {
                        handler(ec);
                        return;
                    }

                    bool success;
                    // Request- bzw. Status-Line parsen
                    if constexpr (IsResponse) {
                        success = parse_status_line(line, reqres);
                    } else {
                        success = parse_request_line(line, reqres);
                    }

                    buffer.consume(matching_bytes);

                    if (!success) {
                        handler(make_error_code(IsResponse ? error::malformed_response : error::malformed_request));
                        return;
                    }

                    // Anschließend mit dem Lesen der Header-Zeilen fortfahren
                    read_header_line(stream, buffer, reqres, std::move(handler));
                });
        }

    private:
        template <typename AsyncReadStream, typename DynamicBuffer, typename ReqRes, typename CompletionHandler>
        static inline void read_header_line(
            AsyncReadStream& stream, DynamicBuffer& buffer, ReqRes& reqres, CompletionHandler&& handler) {
            async_read_line(stream, buffer,
                [&stream, &buffer, &reqres, handler = std::forward<CompletionHandler>(handler)](
                    std::error_code ec, std::string_view line, std::size_t matching_bytes) mutable {
                    if (ec) {
                        handler(ec);
                        return;
                    }

                    if (line.empty()) {
                        buffer.consume(matching_bytes);
                        handler(ec);
                        return;
                    }

                    auto success = parse_field(line, reqres);
                    if (!success) {
                        handler(make_error_code(error::malformed_field));
                        return;
                    }

                    buffer.consume(matching_bytes);
                    read_header_line(stream, buffer, reqres, std::move(handler));
                });
        }
    };

    template <typename AsyncReadStream, typename DynamicBuffer, bool IsResponse, typename ReqRes,
        typename CompletionToken = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type>>
    auto async_read_reqres(AsyncReadStream& stream, DynamicBuffer& buffer, ReqRes& reqres, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(std::error_code)>(
            [](auto&& completion_handler, AsyncReadStream& stream, DynamicBuffer& buffer, ReqRes& reqres) {
                async_reqres_reader<IsResponse>::init(
                    stream, buffer, reqres, std::forward<decltype(completion_handler)>(completion_handler));
            },
            token, std::ref(stream), std::ref(buffer), std::ref(reqres));
    }
} // namespace internal

template <typename AsyncReadStream, typename DynamicBuffer, typename Response,
    typename CompletionToken = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type>>
auto async_read_response(AsyncReadStream& stream, DynamicBuffer& buffer, Response& res,
    CompletionToken&& token = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type> {}) {
    return internal::async_read_reqres<AsyncReadStream, DynamicBuffer, true>(
        stream, buffer, res, std::forward<CompletionToken>(token));
}

template <typename AsyncReadStream, typename DynamicBuffer, typename Request,
    typename CompletionToken = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type>>
auto async_read_request(AsyncReadStream& stream, DynamicBuffer& buffer, Request& req,
    CompletionToken&& token = boost::asio::default_completion_token_t<typename AsyncReadStream::executor_type> {}) {
    return internal::async_read_reqres<AsyncReadStream, DynamicBuffer, false>(
        stream, buffer, req, std::forward<CompletionToken>(token));
}
} // namespace core::http
