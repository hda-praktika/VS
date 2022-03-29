#pragma once

#include <system_error>

namespace core::http {
enum class error {
    // Keine 0

    malformed_request = 1,
    malformed_response,
    malformed_field
};

namespace internal {

class http_error_category : public std::error_category
{
public:
    const char* name() const noexcept override {
        return "core.http";
    }

    std::string message(int ev) const override
    {
        switch(static_cast<error>(ev)) {
            case error::malformed_request: return "malformed request";
            case error::malformed_response: return "malformed response";
            case error::malformed_field: return "malformed field";
        default:
            return "core.http error";
        }
    }

    std::error_condition default_error_condition(
        int ev) const noexcept override
    {
        return std::error_condition{ev, *this};
    }

    bool equivalent(int ev, std::error_condition const& condition) const noexcept override {
        return condition.value() == ev && this == &condition.category();
    }

    bool equivalent(std::error_code const& error, int ev) const noexcept override {
        return error.value() == ev && this == &error.category();
    }
};
}

inline std::error_code make_error_code(error ev) {
    static internal::http_error_category const cat{};
    return std::error_code{static_cast<std::underlying_type<error>::type>(ev), cat};
}

}