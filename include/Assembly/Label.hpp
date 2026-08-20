#pragma once
#include "String/String.hpp"
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace yoctocc {

class Label final {
public:
    enum class Direction {
        UNSPECIFIED,
        FORWARD,
        BACKWARD
    };
    constexpr Label(std::string name) : name(std::move(name)) {
    }

    [[nodiscard]] constexpr inline std::string ref(Direction direction = Direction::UNSPECIFIED) const {
        switch (direction) {
            case Direction::UNSPECIFIED:
                return name;
            case Direction::FORWARD:
                assert(isNumberString(name));
                return name + "f";
            case Direction::BACKWARD:
                return name + "b";
        }
        return name;
    }

    [[nodiscard]] constexpr inline std::string def() const {
        return name + ":";
    }

private:
    std::string name;
};

namespace labels {

inline constexpr Label label(const std::string& name) {
    return Label(name);
}

inline constexpr Label label(const std::string& prefix, uint64_t id) {
    return Label(".L." + prefix + "." + to_string(id));
}

inline constexpr Label label(const std::string& prefix, const std::string& name) {
    return Label(".L." + prefix + "." + name);
}

inline constexpr Label begin(uint64_t id) {
    return label("begin", id);
}

inline constexpr Label else_(uint64_t id) {
    return label("else", id);
}

inline constexpr Label end(uint64_t id) {
    return label("end", id);
}

inline constexpr Label false_(uint64_t id) {
    return label("false", id);
}

inline constexpr Label true_(uint64_t id) {
    return label("true", id);
}

static_assert(label("1").ref() == "1");
static_assert(label("1").ref(Label::Direction::FORWARD) == "1f");
static_assert(label("1").ref(Label::Direction::BACKWARD) == "1b");
static_assert(begin(1).ref() == ".L.begin.1");
static_assert(else_(1).def() == ".L.else.1:");

} // namespace labels

} // namespace yoctocc
