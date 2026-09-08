// Copyright (c) 2019 UX3D GmbH. All rights reserved.

/*
 *  Created by Phil on 26/11/2010.
 *  Copyright 2010 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 *  Modified version of the Catch2 junit reporter to match our (UX3D) needs
 */

#include "tests/catch_reporter_junit.h"
#include "catch2/catch.hpp"

#include <ctime>

namespace ux3d {
namespace tests {
    std::string getCurrentTimestamp()
    {
        // Beware, this is not reentrant because of backward compatibility issues
        // Also, UTC only, again because of backward compatibility (%z is C++11)
        time_t rawtime;
        std::time(&rawtime);
        auto const timeStampSize = sizeof("2017-01-16T17:06:45Z");

#ifdef _MSC_VER
        std::tm timeInfo = {};
        gmtime_s(&timeInfo, &rawtime);
#else
        std::tm* timeInfo;
        timeInfo = std::gmtime(&rawtime);
#endif

        char timeStamp[timeStampSize];
        const char* const fmt = "%Y-%m-%dT%H:%M:%SZ";

#ifdef _MSC_VER
        std::strftime(timeStamp, timeStampSize, fmt, &timeInfo);
#else
        std::strftime(timeStamp, timeStampSize, fmt, timeInfo);
#endif
        return std::string(timeStamp);
    }

    std::string Ux3dReporter::namespacesTag(const std::vector<std::string>& tags)
    {
        auto it = tags.begin();
        if (it != tags.end())
            return *it;
        return std::string();
    }

    Ux3dReporter::Ux3dReporter(Catch::ReporterConfig const& _config)
        : Catch::JunitReporter(_config)
    {
    }

    void Ux3dReporter::testGroupEnded(Catch::TestGroupStats const& testGroupStats)
    {
        double suiteTime = suiteTimer.getElapsedSeconds();
        CumulativeReporterBase::testGroupEnded(testGroupStats);
        writeGroupFixed(*m_testGroups.back(), suiteTime);
    }

    void Ux3dReporter::writeGroupFixed(TestGroupNode const& groupNode, double suiteTime)
    {
        Catch::XmlWriter::ScopedElement e = xml.scopedElement("testsuite");

        Catch::TestGroupStats const& stats = groupNode.value;
        xml.writeAttribute("name", stats.groupInfo.name);
        xml.writeAttribute("errors", unexpectedExceptions);
        xml.writeAttribute("failures", stats.totals.assertions.failed - unexpectedExceptions);
        xml.writeAttribute("tests", stats.totals.assertions.total());
        xml.writeAttribute("hostname", "tbd"); // !TBD
        if (m_config->showDurations() == Catch::ShowDurations::Never)
            xml.writeAttribute("time", "");
        else
            xml.writeAttribute("time", suiteTime);
        xml.writeAttribute("timestamp", getCurrentTimestamp());

        // Write properties if there are any
        if (m_config->hasTestFilters() || m_config->rngSeed() != 0) {
            auto properties = xml.scopedElement("properties");
            if (m_config->hasTestFilters()) {
                xml.scopedElement("property")
                    .writeAttribute("name", "filters")
                    .writeAttribute("value", Catch::serializeFilters(m_config->getTestsOrTags()));
            }
            if (m_config->rngSeed() != 0) {
                xml.scopedElement("property")
                    .writeAttribute("name", "random-seed")
                    .writeAttribute("value", m_config->rngSeed());
            }
        }

        // Write test cases
        for (auto const& child : groupNode.children)
            writeTestCaseFixed(*child);

        xml.scopedElement("system-out").writeText(Catch::trim(stdOutForSuite), Catch::XmlFormatting::None);
        xml.scopedElement("system-err").writeText(Catch::trim(stdErrForSuite), Catch::XmlFormatting::None);
    }

    void Ux3dReporter::writeTestCaseFixed(TestCaseNode const& testCaseNode)
    {
        Catch::TestCaseStats const& stats = testCaseNode.value;

        // All test cases have exactly one section - which represents the
        // test case itself. That section may have 0-n nested sections
        assert(testCaseNode.children.size() == 1);
        SectionNode const& rootSection = *testCaseNode.children.front();

        std::string className = stats.testInfo.className;

        if (className.empty()) {
            className = namespacesTag(stats.testInfo.tags);
            if (className.empty()) {
                className = "global";
            }
        }

        writeSection(className, "", rootSection);
    }

    CATCH_REGISTER_REPORTER("ux3d", Ux3dReporter)
}
}
