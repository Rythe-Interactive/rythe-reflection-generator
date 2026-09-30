#pragma once
#include <rsl/reflection>

namespace test
{
    struct [[rsl_reflect(rsl::dont_serialize)]] test_struct
    {
        int value;
        int get_value() { return value; }
    };

    class [[rsl_reflect()]] test_class
    {
    public:
        test_class() = default;
        test_class(const test_struct& value)
            : m_value(value)
        {}

        const test_struct& get_value(const int*, float) const noexcept { return m_value; }

    private:
        test_struct m_value;
    };

    const float globalVal = 0.567f;
} // namespace test
