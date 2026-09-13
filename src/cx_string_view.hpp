#pragma once

#include <rsl/containers>

#include <clang-c/Index.h>

namespace rrg
{
    struct cx_string_view
    {
        cx_string_view() noexcept = default;

        cx_string_view(cx_string_view&& other) noexcept
            : m_isSet(other.m_isSet),
              m_cxString(other.m_cxString),
              m_value(other.m_value)
        {
            other.m_isSet = false;
            other.m_cxString = {};
            other.m_value = {};
        }

        explicit cx_string_view(CXString cxString)
            : m_isSet(true),
              m_cxString(cxString),
              m_value(rsl::string_view::from_string_length(clang_getCString(cxString)))
        {}

        ~cx_string_view()
        {
            if (m_isSet)
            {
                clang_disposeString(m_cxString);
            }
        }

        rsl::string_view value() const noexcept { return m_value; }

    private:
        bool m_isSet = false; // m_cxString could be an empty string, it would still need to be disposed.
        CXString m_cxString;
        rsl::string_view m_value;
    };
} // namespace rrg
