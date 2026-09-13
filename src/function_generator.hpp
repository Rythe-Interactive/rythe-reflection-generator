#pragma once

#include <rsl/utilities>

#include <clang-c/Index.h>

namespace rrg
{
    rsl::result<void> generate_function(rsl::dynamic_string& contentBuffer, CXCursor cursor);
}
