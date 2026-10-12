#pragma once

namespace SkeletonAutoPatch
{
    /*
    The characters of a Havok string, without going through hkStringPtr::data() or c_str().

    hkStringPtr keeps a flag in bit 0 of its pointer and data() masks it off. In CommonLibSSE-NG from
    February 2026 on, that mask is 32 bits wide: hkStringPtr::StringFlags has a value that does not fit
    an int, and the library is compiled with /Zc:enumTypes, which makes the enum an unsigned int. data()
    then returns the pointer without its upper 32 bits. The game's strings normally live above 4 GiB.

    Read the pointer itself and clear only bit 0.
    */
    [[nodiscard]] inline const char* GetStringData(const RE::hkStringPtr& a_string) noexcept
    {
        static_assert(sizeof(RE::hkStringPtr) == sizeof(std::uintptr_t));

        std::uintptr_t raw = 0;
        std::memcpy(&raw, std::addressof(a_string), sizeof(raw));
        return reinterpret_cast<const char*>(raw & ~static_cast<std::uintptr_t>(1));
    }
}
