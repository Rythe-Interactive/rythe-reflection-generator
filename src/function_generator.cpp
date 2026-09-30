#include "function_generator.hpp"

#include <rsl/logging>

#include "cx_string_view.hpp"
#include "utilities.hpp"

namespace rrg
{
    using namespace rsl::literals;

    namespace
    {
        struct function_context
        {
            rsl::dynamic_string& contentBuffer;
            rsl::result<void> result;
        };

        CXChildVisitResult visit_attributes(CXCursor cursor, rsl::pointer<function_context> context)
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
            }

            return CXChildVisit_Continue;
        }
    } // namespace

    rsl::result<void> generate_function(rsl::dynamic_string& contentBuffer, CXCursor cursor)
    {
        contentBuffer.append(".add_function(\""_sv);
        contentBuffer.append(cx_string_view(clang_getCursorSpelling(cursor)).value());
        contentBuffer.append("\"_sv,rrfl::function_builder<");

        append_reconstructed_type_name(contentBuffer, clang_getCursorResultType(cursor));
        contentBuffer.append('(');

        const CXType functionType = clang_getCursorType(cursor);
        const int numArgs = clang_getNumArgTypes(functionType);
        for (int i = 0; i < numArgs; ++i)
        {
            append_reconstructed_type_name(contentBuffer, clang_getArgType(functionType, i));
            if (i != (numArgs - 1))
            {
                contentBuffer.append(',');
            }
        }

        contentBuffer.append(")>{}"_sv);
        function_context context{ .contentBuffer = contentBuffer, .result = {} };
        clang_visitChildren(cursor, [](CXCursor cursor, CXCursor, CXClientData ctx) {
            return visit_attributes(cursor, { static_cast<function_context*>(ctx) });
        }, &context);

        if (context.result.has_errors())
        {
            return context.result.propagate();
        }

        contentBuffer.append(",rrfl::access_spec_type::"_sv);
        contentBuffer.append(get_access_specifier_spelling(clang_getCXXAccessSpecifier(cursor)));
        contentBuffer.append("_access)"_sv);

        return rsl::okay;
    }
} // namespace rrg
