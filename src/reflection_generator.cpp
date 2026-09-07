#include "reflection_generator.hpp"

#include <rsl/logging>

namespace rrg
{
    namespace
    {
        struct generator_context
        {
            rfs::view& outputFile;
            rsl::result<void> result;
        };

        CXChildVisitResult visit_cursor(CXCursor cursor, CXCursor, rsl::pointer<generator_context> context)
        {
            CXCursorKind kind = clang_getCursorKind(cursor);
            CXString kindSpelling = clang_getCursorKindSpelling(kind);
            CXString cursorSpelling = clang_getCursorSpelling(cursor);

            rsl::dynamic_string line =
                    rsl::format("kind: \"{}\": \"{}\"\n", clang_getCString(kindSpelling), clang_getCString(cursorSpelling));

            if (rsl::result<void> result = context->outputFile.append(line.view()); result.has_errors())
            {
                context->result = result.propagate();
                clang_disposeString(kindSpelling);
                clang_disposeString(cursorSpelling);
                return CXChildVisit_Break;
            }

            rlog::trace(rlog::runtime_format(line.subview(0ull, -1ll)));

            clang_disposeString(kindSpelling);
            clang_disposeString(cursorSpelling);

            return CXChildVisit_Recurse;
        }
    } // namespace

    rsl::result<void> process_translation_unit(CXTranslationUnit translationUnit, rfs::view& outputFile)
    {
        CXCursor cursor = clang_getTranslationUnitCursor(translationUnit);
        generator_context context{
            .outputFile = outputFile,
        };

        if (context.outputFile.exists())
        {
            if (context.outputFile.exists())
            {
                if (rsl::result<void> result = context.outputFile.delete_entry(); result.has_errors())
                {
                    rlog::trace("Failed to delete file \"{}\".", context.outputFile.path());
                    return result.propagate();
                }
            }
        }

        rlog::trace("Creating file \"{}\".", context.outputFile.path());
        if (rsl::result<void> result = context.outputFile.create(); result.has_errors())
        {
            return result.propagate();
        }

        clang_visitChildren(cursor, [](CXCursor cursor, CXCursor parent, CXClientData self) {
            return visit_cursor(cursor, parent, { static_cast<generator_context*>(self) });
        }, &context);

        if (context.result.has_errors())
        {
            return context.result.propagate();
        }

        return rsl::okay;
    }
} // namespace rrg
