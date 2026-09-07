#pragma once
#include <rsl/filesystem>
#include <rsl/utilities>

#include <clang-c/Index.h>

namespace rrg
{
    rsl::result<void> process_translation_unit(CXTranslationUnit translationUnit, const rfs::view& sourceFile, rfs::view& outputFile);
}
