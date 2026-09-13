#include "function_generator.hpp"

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
            CXCursorKind kind = clang_getCursorKind(cursor);

            if (clang_isAttribute(kind))
            {
                cx_string_view attribute(clang_getCursorSpelling(cursor));
                if (!attribute.value().is_empty())
                {
                    rsl::format_to(context->contentBuffer, "attribute: \"{}\"\n", attribute.value());
                }
                return CXChildVisit_Continue;
            }

            return CXChildVisit_Break;
        }
    } // namespace

    rsl::result<void> generate_function(rsl::dynamic_string& contentBuffer, CXCursor cursor)
    {
        function_context context{ .contentBuffer = contentBuffer, .result = {} };

        CXString cursorSpelling = clang_getCursorSpelling(cursor);
        rsl::format_to(contentBuffer, "function: \"{}\"\n", clang_getCString(cursorSpelling));
        clang_disposeString(cursorSpelling);

        CXType returnType = clang_getCursorResultType(cursor);

        rsl::format_to(
                contentBuffer,
                "access specifier: {}\nreturn type: \"{}\"\n",
                get_access_specifier_spelling(clang_getCXXAccessSpecifier(cursor)),
                cx_string_view(clang_getTypeSpelling(returnType)).value());

        clang_visitChildren(cursor, [](CXCursor cursor, CXCursor, CXClientData ctx) {
            return visit_attributes(cursor, { static_cast<function_context*>(ctx) });
        }, &context);

        if (context.result.has_errors())
        {
            return context.result.propagate();
        }

        return rsl::okay;
    }
} // namespace rrg
