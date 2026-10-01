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

    //inline void append_reconstructed_namespace(rsl::dynamic_string& contentBuffer, const CXType& type)
    //{

    //}

    inline void append_reconstructed_type_name(rsl::dynamic_string& contentBuffer, const CXType& type)
    {
        using namespace rsl::literals;

        if (type.kind >= CXTypeKind::CXType_Pointer && type.kind <= CXTypeKind::CXType_RValueReference)
        {
            std::underlying_type_t<CXTypeKind> idx = type.kind - CXTypeKind::CXType_Pointer;

            const static rsl::string_view decoration[] = { "*", "OBJECTIVE C IS NOT SUPPORTED", "&", "&&" };
            append_reconstructed_type_name(contentBuffer, clang_getPointeeType(type));
            contentBuffer.append(decoration[idx]);
            if (clang_isConstQualifiedType(type))
            {
                contentBuffer.append("const"_sv);
            }
            return;
        }

        cx_string_view displayName(clang_getTypeSpelling(clang_getCursorType(clang_getTypeDeclaration(type))));

        // Primitive types (e.g. bool) return no declaration here, so just use the display name
        if (displayName.value().is_empty())
        {
            displayName = cx_string_view(clang_getTypeSpelling(clang_getUnqualifiedType(type)));
        }

        contentBuffer.append(displayName.value());
        if (clang_isConstQualifiedType(type))
        {
            contentBuffer.append(" const"_sv);
        }
    }
} // namespace rrg
