#pragma once
#include <cstdint>
#include <string>
#include "String/String.hpp"

namespace yoctocc {

    class Label final {
    public:
        constexpr Label(const std::string& name) : name(name) {}

        constexpr inline std::string ref() const {
            return name;
        }

        constexpr inline std::string def() const {
            return name + ":";
        }

    private:
        std::string name;
    };

    inline constexpr Label makeLabel(const std::string& name) {
        return Label(name);
    }

    inline constexpr Label makeLabel(const std::string& prefix, uint64_t id) {
        return Label(".L." + prefix + "." + to_string(id));
    }

    inline constexpr Label makeLabel(const std::string& prefix, const std::string& name) {
        return Label(".L." + prefix + "." + name);
    }

    inline constexpr Label makeBeginLabel(uint64_t id) {
        return makeLabel("begin", id);
    }

    inline constexpr Label makeElseLabel(uint64_t id) {
        return makeLabel("else", id);
    }

    inline constexpr Label makeEndLabel(uint64_t id) {
        return makeLabel("end", id);
    }

    static_assert(makeBeginLabel(1).ref() == ".L.begin.1");
    static_assert(makeElseLabel(1).def() == ".L.else.1:");

} // namespace yoctocc
