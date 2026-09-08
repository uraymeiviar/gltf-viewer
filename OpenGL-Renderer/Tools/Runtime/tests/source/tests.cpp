// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#define CATCH_CONFIG_RUNNER // This tells Catch to provide a main() - only do this in one cpp file
#include "catch2/catch.hpp"

int main(int argc, char* argv[])
{
    int result = Catch::Session().run(argc, argv);
    return result;
}
