#pragma once

#include <array>
#include <memory>
#include <type_traits>

#include <utility>

namespace core::internal::util {

template<typename ConstIterator>
class const_iterator_pair {
    ConstIterator begin_;
    ConstIterator end_;

public:
    const_iterator_pair(ConstIterator begin, ConstIterator end) : begin_(begin), end_(end) {}

    ConstIterator begin() const {
        return cbegin();
    }

    ConstIterator cbegin() const {
        return begin_;
    }

    ConstIterator end() const {
        return cend();
    }

    ConstIterator cend() const {
        return end_;
    }
};

template<typename RealHandler, typename... Owns>
class owning_handler {
    RealHandler real_handler_;
    std::tuple<Owns...> owns_;

public:
    owning_handler(RealHandler&& real_handler, Owns&&... owns) : real_handler_(std::forward<RealHandler>(real_handler)), owns_(std::move(owns)...) {}

    template<typename... Args>
    decltype(auto) operator()(Args&&... args) {
        return std::forward<RealHandler>(real_handler_)(std::forward<Args>(args)...);
    }
};

template<typename Owns1, typename Handler>
auto make_owning_handler(Handler&& handler, Owns1&& owns1) {
    return owning_handler<Handler, Owns1>{
        std::forward<Handler>(handler),
        std::forward<Owns1>(owns1)
    };
}

}
