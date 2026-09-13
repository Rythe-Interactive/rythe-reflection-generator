#pragma once

#include <rsl/string>

#include <clang-c/Index.h>

namespace rrg
{
    inline rsl::string_view get_access_specifier_spelling(CX_CXXAccessSpecifier accessSpecifier)
    {
        using namespace rsl::literals;
        switch (accessSpecifier)
        {
            case CX_CXXInvalidAccessSpecifier: return "invalid"_sv;
            case CX_CXXPublic: return "public"_sv;
            case CX_CXXProtected: return "protected"_sv;
            case CX_CXXPrivate: return "private"_sv;
        }
        return "unknown"_sv;
    }
} // namespace rrg
