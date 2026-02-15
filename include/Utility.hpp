#include <cstddef>

namespace yoctocc {

    static constexpr size_t alignTo(size_t n, size_t align) {
        return (n + align - 1) / align * align;
    }

} // namespace yoctocc
