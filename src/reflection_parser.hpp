#pragma once
#include <rsl/utilities>

#include <clang-c/Index.h>

namespace rrg
{
    class reflection_parser
    {
    public:
        rsl::result<void> parse_translation_unit(CXTranslationUnit translationUnit);

    private:
        CXChildVisitResult visit_cursor(CXCursor cursor, CXCursor parent);
    };
}
