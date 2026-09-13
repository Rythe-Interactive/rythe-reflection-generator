#include "class_generator.hpp"

#include "cx_string_view.hpp"
#include "field_generator.hpp"
#include "function_generator.hpp"

namespace rrg
{
    using namespace rsl::literals;

    namespace
    {
        struct class_context
        {
            rsl::dynamic_string& contentBuffer;
            rsl::result<void> result;
        };

        CXChildVisitResult visit_members(CXCursor cursor, rsl::pointer<class_context> context)
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

            switch (kind)
            {
                case CXCursor_ClassDecl:
                case CXCursor_StructDecl:
                {
                    if (rsl::result<void> result = generate_class(context->contentBuffer, cursor); result.has_errors())
                    {
                        context->result = result.propagate();
                        return CXChildVisit_Break;
                    }
                    break;
                }
                case CXCursor_FieldDecl:
                {
                    if (rsl::result<void> result = generate_field(context->contentBuffer, cursor); result.has_errors())
                    {
                        context->result = result.propagate();
                        return CXChildVisit_Break;
                    }
                    break;
                }
                case CXCursor_CXXMethod:
                {
                    if (rsl::result<void> result = generate_function(context->contentBuffer, cursor); result.has_errors())
                    {
                        context->result = result.propagate();
                        return CXChildVisit_Break;
                    }
                    break;
                }
                default: break;
            }

            return CXChildVisit_Continue;
        }
    } // namespace

    rsl::result<void> generate_class(rsl::dynamic_string& contentBuffer, CXCursor cursor)
    {
        class_context context{ .contentBuffer = contentBuffer, .result = {} };

        CXString cursorSpelling = clang_getCursorSpelling(cursor);
        rsl::format_to(contentBuffer, "class: \"{}\"\n", clang_getCString(cursorSpelling));
        clang_disposeString(cursorSpelling);

        clang_visitChildren(cursor, [](CXCursor cursor, CXCursor, CXClientData ctx) {
            return visit_members(cursor, { static_cast<class_context*>(ctx) });
        }, &context);

        if (context.result.has_errors())
        {
            return context.result.propagate();
        }

        return rsl::okay;
    }
} // namespace rrg
