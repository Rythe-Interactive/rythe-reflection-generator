#include "field_generator.hpp"

#include <rsl/logging>

#include "cx_string_view.hpp"
#include "utilities.hpp"

namespace rrg
{
    using namespace rsl::literals;

    namespace
    {
        struct field_context
        {
            rsl::dynamic_string& contentBuffer;
            rsl::result<void> result;
        };

        CXChildVisitResult visit_attributes(CXCursor cursor, rsl::pointer<field_context> context)
        {
            if (clang_isAttribute(clang_getCursorKind(cursor)))
            {
                const cx_string_view attribute(clang_getCursorSpelling(cursor));
                if (!attribute.value().is_empty() && attribute.value() != "rsl_reflect_attr"_sv)
                {
                    context->contentBuffer.append(".add_attribute("_sv);
                    context->contentBuffer.append(attribute.value());
                    if (attribute.value().back() != ')')
                    {
                        context->contentBuffer.append("()"_sv);
                    }
                    context->contentBuffer.append(')');
                }

                return CXChildVisit_Continue;
            }

            return CXChildVisit_Break;
        }
    } // namespace

    rsl::result<void> generate_field(rsl::dynamic_string& contentBuffer, CXCursor cursor)
    {
        contentBuffer.append(".add_field(\""_sv);
        contentBuffer.append(cx_string_view(clang_getCursorSpelling(cursor)).value());
        contentBuffer.append("\"_sv,rrfl::field_builder<"_sv);
        append_reconstructed_type_name(contentBuffer, clang_getCursorType(cursor));

        contentBuffer.append(">{}"_sv);
        field_context context{ .contentBuffer = contentBuffer, .result = {} };
        clang_visitChildren(cursor, [](CXCursor cursor, CXCursor, CXClientData ctx) {
            return visit_attributes(cursor, { static_cast<field_context*>(ctx) });
        }, &context);

        if (context.result.has_errors())
        {
            return context.result.propagate();
        }

        const CX_CXXAccessSpecifier accessSpecifier = clang_getCXXAccessSpecifier(cursor);
        if (accessSpecifier != CX_CXXAccessSpecifier::CX_CXXInvalidAccessSpecifier)
        {
            contentBuffer.append(",rrfl::access_spec_type::"_sv);
            contentBuffer.append(get_access_specifier_spelling(accessSpecifier));
            contentBuffer.append("_access"_sv);
        }

        contentBuffer.append(')');

        return rsl::okay;
    }
} // namespace rrg
