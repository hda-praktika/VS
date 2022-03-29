#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

#include <lua.hpp>

class script {
    lua_State* L_;
    std::function<void(std::uint64_t)> notify_handler_;

public:
    script(std::string);
    ~script() noexcept;

    void load_arguments(std::vector<std::string>);

    template<typename Handler>
    void set_notify_handler(Handler&& handler) {
        notify_handler_ = std::forward<Handler>(handler);
    }

    void run();

    static int sleep(lua_State *L);
    static int notify(lua_State *L);
};
