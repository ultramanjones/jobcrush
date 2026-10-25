# Job Crush tests.
#
# The README says the core works without a window. These tests check that.
# Each one builds the Logic classes it needs straight from src/, links QtCore
# and QtTest, and runs with no window. No QML and no live network.
# Two tests, Active Jobs folder and DiscoveryWorker, also link QtSql. They
# use a database file in a temporary folder, never the real one.
#
# Run them with:  ctest --test-dir build --output-on-failure

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

# One test program per test class, the way QtTest expects. The first
# argument names the program. The rest are its source files.
function(add_job_crush_test testName)
    qt_add_executable(${testName} ${ARGN})
    target_include_directories(${testName} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${testName} PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME ${testName} COMMAND ${testName})
endfunction()

# The scoring behind Top Prospects, and the clearance check beside it.
add_job_crush_test(tst_ProspectScorer
    tst_ProspectScorer.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/ProspectScorer.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.cpp
)

# What the user is looking for: how it is cleaned up, saved and loaded.
add_job_crush_test(tst_JobSearchProfile
    tst_JobSearchProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.cpp
)

# Reading the dates people type into the "Applied on" box.
add_job_crush_test(tst_TypedDateReader
    tst_TypedDateReader.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/TypedDateReader.cpp
)

# Reading Workday links and answers. Uses saved answers, not the network.
add_job_crush_test(tst_WorkdayBoardSource
    tst_WorkdayBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobScoutReply.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/WorkdayBoardSource.cpp
)

# Reading and writing the JobQuest.txt file in each job folder.
add_job_crush_test(tst_JobQuestFile
    tst_JobQuestFile.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/activejobs/JobQuestFile.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/TypedDateReader.cpp
)

# The Active Jobs folder and the AI app actions need the board and the
# database to test against. The test uses a temporary folder with its own
# database file, so it writes nothing to the real user's files.
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

# Reading SmartRecruiters answers. Uses saved answers, not the network.
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

# The background worker behind Discoveries: saving found jobs, and preparing
# the rows for a tab. Uses its own database in a temporary folder.
add_job_crush_test(tst_DiscoveryWorker
    tst_DiscoveryWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/model/JobCrushDatabase.cpp
    ${CMAKE_SOURCE_DIR}/src/model/JobPostingRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/DiscoveryWorker.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/DiscoveryWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/ProspectScorer.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobSearchProfile.cpp
)
target_link_libraries(tst_DiscoveryWorker PRIVATE Qt6::Sql)

# A job is called gone only when two answers from the company's board agree.
# A fake board stands in for the network.
add_job_crush_test(tst_PostingGoneConfirmer
    tst_PostingGoneConfirmer.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobScoutReply.h
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/PostingGoneConfirmer.h
    ${CMAKE_SOURCE_DIR}/src/logic/pipelines/PostingGoneConfirmer.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/GreenhouseBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/LeverBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/AshbyBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/SmartRecruitersBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/WorkdayBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/TaleoBoardSource.cpp
)

# Reading Taleo's answers: the portal number, a search row, a job page, and
# Taleo links. Uses answers shaped like the live ones checked 2026-10-23.
add_job_crush_test(tst_TaleoBoardSource
    tst_TaleoBoardSource.cpp
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/JobScoutReply.h
    ${CMAKE_SOURCE_DIR}/src/logic/jobscout/TaleoBoardSource.cpp
)
