#include "script.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

#define GET_SCRIPT(state) *static_cast<script**>(lua_getextraspace(state))

void throw_lua_exception(lua_State* L) {
    throw std::runtime_error { std::string{"Lua-Fehler: "} + lua_tostring(L, -1) };
}

script::script(std::string path) {
    L_ = luaL_newstate();
    GET_SCRIPT(L_) = this;

    luaL_openlibs(L_);

    lua_register(L_, "sleep", script::sleep);
    lua_register(L_, "notify", script::notify);

    auto ec = luaL_loadfile(L_, path.c_str());
    if (ec) {
        throw_lua_exception(L_);
    }
}

script::~script() noexcept {
    lua_close(L_);
}

void script::load_arguments(std::vector<std::string> argv) {
    lua_newtable(L_);
    int arg_index = 1;
    for(auto& arg : argv) {
        lua_pushinteger(L_, arg_index++);
        lua_pushstring(L_, arg.c_str());
        lua_settable(L_, -3);
    }
    lua_setglobal(L_, "arg");
}

void script::run() {
    auto ec = lua_pcall(L_, 0, LUA_MULTRET, 0);
    if (ec) {
        throw_lua_exception(L_);
    }
}

int script::sleep(lua_State *L) {
    auto ms = lua_tonumber(L, 1);
    std::chrono::milliseconds time{static_cast<std::int64_t>(ms)};
    std::this_thread::sleep_for(time);
    return 0;
}

int script::notify(lua_State *L) {
    auto power = lua_tonumber(L, 1);
    auto self = GET_SCRIPT(L);
    self->notify_handler_(power);
    return 0;
}
