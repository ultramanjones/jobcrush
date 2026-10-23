#pragma once

#include <QDate>
#include <QJsonObject>
#include <QString>

class ActiveJobsFolder;
class JobPipelines;
class JobPostingRepository;

// AiAppAnswer
//
// What Job Crush says back to an AI app after an action. Plain text the AI
// reads, and whether the action failed.
struct AiAppAnswer {
    QString text;
    bool isError = false;
};

// AiAppRequests
//
// Does the actions AI apps ask for (see AiAppTools.h): list the board, add a
// job, mark one applied, move one, add a note, and say what is waiting on the
// user.
//
// Job Crush is the only program that writes its database. An AI app asks;
// this class does the work through the same Logic the screens use, so a job
// an AI adds is exactly like a job the user adds, and it gets a job folder
// the same way.
class AiAppRequests {
public:
    AiAppRequests(JobPipelines &pipelines,
                  JobPostingRepository &postingRepository,
                  ActiveJobsFolder &activeJobsFolder);

    // Runs one action by name with its arguments. An unknown name or a
    // missing argument is answered as an error, in a sentence the AI can act
    // on. today is passed in so the answer is the same every time it is
    // tested.
    AiAppAnswer runAction(const QString &actionName, const QJsonObject &arguments,
                          const QDate &today) const;

private:
    AiAppAnswer listJobs(const QJsonObject &arguments) const;
    AiAppAnswer addJob(const QJsonObject &arguments, const QDate &today) const;
    AiAppAnswer markApplied(const QJsonObject &arguments, const QDate &today) const;
    AiAppAnswer moveJob(const QJsonObject &arguments) const;
    AiAppAnswer addNote(const QJsonObject &arguments, const QDate &today) const;
    AiAppAnswer whatsWaiting(const QDate &today) const;

    JobPipelines &board;
    JobPostingRepository &postingRepository;
    ActiveJobsFolder &activeJobs;
};
