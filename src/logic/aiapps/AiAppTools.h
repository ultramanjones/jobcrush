#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

// AiAppTools
//
// The actions Job Crush offers to AI apps such as Claude desktop, ChatGPT and
// Gemini, through the shared standard called MCP (Model Context Protocol).
//
// This file only DESCRIBES the actions: their names, what they do, and what
// they take. Doing them is AiAppRequests. The description is kept apart and
// header-only because two programs need it: Job Crush itself, and the small
// bridge program an AI app starts (see tools/ai-app-bridge). The bridge can
// tell an AI app what Job Crush offers even while Job Crush is closed.
//
// The descriptions are written for the AI, not for the user: they say when to
// use each action and what to pass.
namespace AiAppTools {

// The name of the local connection Job Crush listens on. Only programs run by
// the same Windows user can open it.
inline const QString ConnectionName = QStringLiteral("JobCrush-AiApps");

// How Job Crush names itself to an AI app.
inline const QString ServerName = QStringLiteral("job-crush");
inline const QString ServerVersion = QStringLiteral("0.1.0");

// The MCP versions Job Crush understands, newest first. An AI app that asks
// for one of these gets it; any other gets the newest.
inline QStringList protocolVersions()
{
    return { QStringLiteral("2025-06-18"), QStringLiteral("2025-03-26"),
             QStringLiteral("2024-11-05") };
}

// The version to answer with: the one the AI app asked for when Job Crush
// knows it, otherwise the newest.
inline QString protocolVersionFor(const QString &requestedVersion)
{
    return protocolVersions().contains(requestedVersion) ? requestedVersion
                                                         : protocolVersions().first();
}

// The action names.
inline const QString ListJobs     = QStringLiteral("list_jobs");
inline const QString AddJob       = QStringLiteral("add_job");
inline const QString MarkApplied  = QStringLiteral("mark_applied");
inline const QString MoveJob      = QStringLiteral("move_job");
inline const QString AddNote      = QStringLiteral("add_note");
inline const QString WhatsWaiting = QStringLiteral("whats_waiting");

// --- Builders for the JSON Schema that describes each action's arguments --

// A text argument.
inline QJsonObject textProperty(const QString &description)
{
    return { { QStringLiteral("type"), QStringLiteral("string") },
             { QStringLiteral("description"), description } };
}

// A board column argument. The AI may only pass one of the listed words.
inline QJsonObject columnProperty(const QString &description)
{
    return { { QStringLiteral("type"), QStringLiteral("string") },
             { QStringLiteral("enum"), QJsonArray { QStringLiteral("Crushed"),
                                                    QStringLiteral("Applied"),
                                                    QStringLiteral("Interviewing"),
                                                    QStringLiteral("Offer"),
                                                    QStringLiteral("Closed") } },
             { QStringLiteral("description"), description } };
}

// The job_id argument: a card's number on the board.
inline QJsonObject jobIdProperty()
{
    return { { QStringLiteral("type"), QStringLiteral("integer") },
             { QStringLiteral("description"),
               QStringLiteral("The job's number, as list_jobs shows it after the #.") } };
}

// The whole argument list of one action, and which arguments it must have.
inline QJsonObject inputSchema(const QJsonObject &properties, const QStringList &required)
{
    QJsonObject schema { { QStringLiteral("type"), QStringLiteral("object") },
                         { QStringLiteral("properties"), properties } };
    if (!required.isEmpty()) {
        schema.insert(QStringLiteral("required"), QJsonArray::fromStringList(required));
    }
    return schema;
}

// One action as MCP describes it: its name, what it does, and its arguments.
inline QJsonObject tool(const QString &name, const QString &description,
                        const QJsonObject &schema)
{
    return { { QStringLiteral("name"), name },
             { QStringLiteral("description"), description },
             { QStringLiteral("inputSchema"), schema } };
}

// Every action, in the shape MCP's tools/list answer wants.
inline QJsonArray toolDefinitions()
{
    const QString columnMeanings = QStringLiteral(
        "Saved: the user wants the job but has not applied. Applied: the "
        "application was sent. Interview: an interview is scheduled. Offer: they "
        "made an offer. Closed: it is over (rejected, withdrawn, or turned down).");

    return {
        tool(ListJobs,
             QStringLiteral("List the jobs on the user's Job Crush board, with each job's "
                            "number, company, title, column, applied date and notes. Call "
                            "this first to find a job's number before changing it."),
             inputSchema({ { QStringLiteral("column"),
                             columnProperty(QStringLiteral("Only list jobs in this column. "
                                                           "Leave out for every job.")) },
                           { QStringLiteral("search"),
                             textProperty(QStringLiteral("Only list jobs whose company or "
                                                         "title contains this text.")) } },
                         {})),
        tool(AddJob,
             QStringLiteral("Put a job on the user's Job Crush board. Use this when the "
                            "user finds a job or applies to one, on any site. If the same "
                            "job is already on the board, nothing is added and the answer "
                            "gives its number. Never guess a value; leave out what you do "
                            "not know. ") + columnMeanings,
             inputSchema({ { QStringLiteral("company"), textProperty(QStringLiteral("The employer's name.")) },
                           { QStringLiteral("title"), textProperty(QStringLiteral("The job title.")) },
                           { QStringLiteral("link"), textProperty(QStringLiteral("The web address of the job posting.")) },
                           { QStringLiteral("found_on"), textProperty(QStringLiteral("The site the job was found on, for example FlexJobs.")) },
                           { QStringLiteral("column"), columnProperty(QStringLiteral("Where the job goes. Saved when left out.")) },
                           { QStringLiteral("applied_on"), textProperty(QStringLiteral("The day the application was sent, like 10/14/2026, yesterday, or 3 days ago.")) },
                           { QStringLiteral("pay"), textProperty(QStringLiteral("The pay, as the posting says it.")) },
                           { QStringLiteral("where"), textProperty(QStringLiteral("City and state, or Remote.")) },
                           { QStringLiteral("notes"), textProperty(QStringLiteral("Anything the user should remember about this job.")) },
                           { QStringLiteral("created_by"), textProperty(QStringLiteral("Your own name, for example Claude or ChatGPT.")) } },
                         { QStringLiteral("company"), QStringLiteral("title") })),
        tool(MarkApplied,
             QStringLiteral("Record that the user sent an application. Moves the job to "
                            "Applied and sets the applied date."),
             inputSchema({ { QStringLiteral("job_id"), jobIdProperty() },
                           { QStringLiteral("applied_on"),
                             textProperty(QStringLiteral("The day it was sent, like 10/14/2026, "
                                                         "yesterday, or 3 days ago. Today when "
                                                         "left out.")) } },
                         { QStringLiteral("job_id") })),
        tool(MoveJob,
             QStringLiteral("Move a job to another column on the board. ") + columnMeanings,
             inputSchema({ { QStringLiteral("job_id"), jobIdProperty() },
                           { QStringLiteral("column"), columnProperty(QStringLiteral("The column to move it to.")) } },
                         { QStringLiteral("job_id"), QStringLiteral("column") })),
        tool(AddNote,
             QStringLiteral("Add a note to a job, below the notes it already has. The "
                            "date is put in front of the note."),
             inputSchema({ { QStringLiteral("job_id"), jobIdProperty() },
                           { QStringLiteral("note"), textProperty(QStringLiteral("The note to add.")) } },
                         { QStringLiteral("job_id"), QStringLiteral("note") })),
        tool(WhatsWaiting,
             QStringLiteral("List what is waiting on the user: jobs saved but not applied "
                            "to yet, applications sent over two weeks ago with no news "
                            "(time to follow up), and jobs at the interview stage."),
             inputSchema({}, {})),
    };
}

// --- JSON-RPC: the message shape MCP uses ---------------------------------

// The answer to "initialize": which version, what Job Crush offers, and its
// name. Both the bridge and Job Crush give this same answer.
inline QJsonObject initializeResult(const QString &requestedVersion)
{
    return {
        { QStringLiteral("protocolVersion"), protocolVersionFor(requestedVersion) },
        { QStringLiteral("capabilities"),
          QJsonObject { { QStringLiteral("tools"), QJsonObject {} } } },
        { QStringLiteral("serverInfo"),
          QJsonObject { { QStringLiteral("name"), ServerName },
                        { QStringLiteral("version"), ServerVersion } } },
        { QStringLiteral("instructions"),
          QStringLiteral("Job Crush tracks the user's job search on a board with five "
                         "columns: Crushed, Applied, Interviewing, Offer, Closed. Keep it up to "
                         "date whenever you help the user find or apply to a job.") },
    };
}

// A JSON-RPC success answer to the request with this id.
inline QJsonObject resultMessage(const QJsonValue &requestId, const QJsonObject &result)
{
    return { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
             { QStringLiteral("id"), requestId },
             { QStringLiteral("result"), result } };
}

// A JSON-RPC error answer to the request with this id.
inline QJsonObject errorMessage(const QJsonValue &requestId, int errorCode,
                                const QString &errorText)
{
    return { { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
             { QStringLiteral("id"), requestId },
             { QStringLiteral("error"),
               QJsonObject { { QStringLiteral("code"), errorCode },
                             { QStringLiteral("message"), errorText } } } };
}

// The answer to a tools/call, in the shape MCP wants: a list holding one
// piece of text.
inline QJsonObject toolCallResult(const QString &answerText, bool isError)
{
    return { { QStringLiteral("content"),
               QJsonArray { QJsonObject { { QStringLiteral("type"), QStringLiteral("text") },
                                          { QStringLiteral("text"), answerText } } } },
             { QStringLiteral("isError"), isError } };
}

// JSON-RPC's own error numbers.
constexpr int MethodNotFound = -32601;
constexpr int ParseError = -32700;

} // namespace AiAppTools
