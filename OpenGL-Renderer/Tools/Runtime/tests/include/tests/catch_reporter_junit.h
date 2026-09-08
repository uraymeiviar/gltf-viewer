// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <catch2/catch.hpp>

namespace ux3d {
namespace tests {
    class Ux3dReporter : public Catch::JunitReporter {
    public:
        explicit Ux3dReporter(Catch::ReporterConfig const& _config);

        void testGroupEnded(Catch::TestGroupStats const& testGroupStats) override;

    private:
        static std::string namespacesTag(const std::vector<std::string>& tags);

        void writeGroupFixed(TestGroupNode const& groupNode, double suiteTime);

        void writeTestCaseFixed(TestCaseNode const& testCaseNode);
    };
}
}
