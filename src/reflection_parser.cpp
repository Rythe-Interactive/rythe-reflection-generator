#include "reflection_parser.hpp"

#include <rsl/logging>

namespace rrg
{
    rsl::result<void> rrg::reflection_parser::parse_translation_unit(CXTranslationUnit translationUnit)
    {
        CXCursor cursor = clang_getTranslationUnitCursor(translationUnit);

        clang_visitChildren(cursor, [](CXCursor cursor, CXCursor parent, CXClientData self) {
            return static_cast<reflection_parser*>(self)->visit_cursor(cursor, parent);
        }, this);

        return rsl::okay;
    }

    CXChildVisitResult reflection_parser::visit_cursor(CXCursor cursor, CXCursor)
    {
        CXCursorKind kind = clang_getCursorKind(cursor);
        CXString kindSpelling = clang_getCursorKindSpelling(kind);
        CXString cursorSpelling = clang_getCursorSpelling(cursor);
        rlog::trace("kind: \"{}\": \"{}\"", clang_getCString(kindSpelling), clang_getCString(cursorSpelling));
        clang_disposeString(kindSpelling);
        clang_disposeString(cursorSpelling);
        return CXChildVisit_Recurse;
    }
} // namespace rrg
