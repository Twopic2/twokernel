#include "stats/trace.hpp"

#ifdef TWOKERNEL_ENABLE_TRACE
namespace Stats::Trace {
    Scope::Scope(std::string_view name) : active_(enabled()) {
        if (!active_) {
            return;
        }
        auto length = name.size() < sizeof(name_) ? name.size() : sizeof(name_) - 1;
        if (length < name.size()) {
            while (length && (static_cast<unsigned char>(name[length]) & 0xc0) == 0x80) {
                --length;
            }
        }
        for (std::size_t i = 0; i < length; ++i) {
            name_[i] = name[i];
        }
        context_ = Detail::scope_context();
    }

    Scope::~Scope() {
        if (active_) {
            const auto end = timestamp_us();
            if (end >= context_.start_us) {
                interval(name_, context_.track_id, context_.start_us,
                         end - context_.start_us);
            }
        }
    }
}
#endif
