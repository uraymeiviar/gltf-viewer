#include "core/String.h"
#include <catch2/catch.hpp>

namespace ux3d::core::tests {
SCENARIO("string::startsWith", "[runtime.core.String]")
{
    GIVEN("string A that starts with string B")
    {
        core::string A = "hello Sailor!";
        core::string B = "hello";

        THEN("startsWith returns true")
        {
            CHECK(startsWith(A, B));
        }
    }
    GIVEN("string A that contains, but doesn't start with string B")
    {
        core::string A = "hello Sailor!";
        core::string B = "Sailor";

        THEN("startsWith returns false")
        {
            CHECK_FALSE(startsWith(A, B));
        }
    }
    GIVEN("string A and string B that are both empty")
    {
        core::string A;
        core::string B;

        THEN("startsWith returns true")
        {
            CHECK(startsWith(A, B));
        }
    }
}
}
