# Job Crush tests.
#
# The README says the core works without a window. These tests are the proof.
# Each one builds the Logic classes it needs straight from src/, links QtCore
# and QtTest, and runs headless. No QML, no database file, no network.
# The Active Jobs folder test also links QtSql, for a database in a
# temporary folder.
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
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/ProspectScorer.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.cpp
)

add_job_crush_test(tst_JobSearchProfile
    tst_JobSearchProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.cpp
)

add_job_crush_test(tst_TypedDateReader
    tst_TypedDateReader.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/TypedDateReader.cpp
)

add_job_crush_test(tst_WorkdayBoardSource
    tst_WorkdayBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobScoutReply.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/WorkdayBoardSource.cpp
)

add_job_crush_test(tst_JobQuestFile
    tst_JobQuestFile.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/activejobs/JobQuestFile.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/TypedDateReader.cpp
)

# The Active Jobs folder and the AI app actions need the board and the
# database to test against.
# It uses a temporary folder with its own database file, so it still writes
# nothing to the real computer.
add_job_crush_test(tst_ActiveJobsFolder
    tst_ActiveJobsFolder.cpp
    ${CMAKE_SOURCE_DIR}/src/model/JobCrushDatabase.cpp
    ${CMAKE_SOURCE_DIR}/src/model/JobPostingRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/model/JobApplicationRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/model/PostingSourceRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/model/ActiveJobFolderRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/JobPipelines.h
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/JobPipelines.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/TypedDateReader.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/activejobs/JobQuestFile.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/activejobs/OlderJobFolderReader.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/activejobs/ActiveJobsFolder.h
    ${CMAKE_SOURCE_DIR}/src/logic/activejobs/ActiveJobsFolder.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/aiapps/AiAppRequests.cpp
)
find_package(Qt6 REQUIRED COMPONENTS Sql)
target_link_libraries(tst_ActiveJobsFolder PRIVATE Qt6::Sql)

add_job_crush_test(tst_SmartRecruitersBoardSource
    tst_SmartRecruitersBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobScoutReply.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/SmartRecruitersBoardSource.cpp
)

# Reads the shipped data files, so a typo in one fails here.
add_job_crush_test(tst_CompanyList
    tst_CompanyList.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/CompanyList.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/CareerPreset.cpp
)
target_compile_definitions(tst_CompanyList PRIVATE JOBCRUSH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
