# Job Crush tests.
#
# The README says the core works without a window. These tests are the proof.
# Each one builds the Logic classes it needs straight from src/, links QtCore
# and QtTest, and runs headless. No QML, no database file, no network.
#
# Run them with:  ctest --test-dir build --output-on-failure

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

# One test program per class under test, the way QtTest expects.
function(add_job_crush_test testName)
    qt_add_executable(${testName} ${ARGN})
    target_include_directories(${testName} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${testName} PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME ${testName} COMMAND ${testName})
endfunction()

add_job_crush_test(tst_ProspectScorer
    tst_ProspectScorer.cpp
    ${CMAKE_SOURCE_DIR}/src/modelview/jobscout/ProspectScorer.cpp
    ${CMAKE_SOURCE_DIR}/src/modelview/jobscout/JobSearchProfile.h
    ${CMAKE_SOURCE_DIR}/src/modelview/jobscout/JobSearchProfile.cpp
)

add_job_crush_test(tst_JobSearchProfile
    tst_JobSearchProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/modelview/jobscout/JobSearchProfile.h
    ${CMAKE_SOURCE_DIR}/src/modelview/jobscout/JobSearchProfile.cpp
)

add_job_crush_test(tst_TypedDateReader
    tst_TypedDateReader.cpp
    ${CMAKE_SOURCE_DIR}/src/modelview/pipelines/TypedDateReader.cpp
)

add_job_crush_test(tst_WorkdayBoardSource
    tst_WorkdayBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/modelview/jobscout/JobScoutReply.h
    ${CMAKE_SOURCE_DIR}/src/modelview/jobscout/WorkdayBoardSource.cpp
)
