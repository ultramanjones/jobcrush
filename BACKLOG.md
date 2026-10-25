# Job Crush — Backlog

What is queued, what is deliberately parked, and why. Kept current as work
lands: an item moves out of this file when it ships, and anything new goes in
the moment it is asked for rather than the moment somebody gets round to it.

A backlog nobody updates is a lie with a filename.

**The solutions are researched.** `BACKLOG_RESEARCH.md`, in the "QtQuick App"
planning folder, carries one section per item below: the endpoints, the field
names, the licence traps and the decisions, so picking one up is implementation
rather than investigation. Five findings in it change plans — read its opening
section before starting anything here.

---

## Next up

**29. Pick where Job Crush saves files.** Ultra, 2026-10-16. Job Crush
saves copies and packets in Documents\Job Crush Packets. When OneDrive backs
up Documents, Windows says Documents is the OneDrive one, so files land in
OneDrive. Many people use that, so it stays the default. But Settings needs a
"Where Job Crush saves files" picker, like the Active Jobs folder picker.
AppPreferences already stores exportFolderPath; only the Settings control is
missing.

**28. Home country and home metro filter.** Ultra, 2026-10-16. Followed
companies send jobs from every country (Bangalore, Jakarta). Today, any job
marked remote gets through the area filter, even "Remote - India".
- Pick your home country, for example United States.
- Pick your home city as a metro area, for example the Pittsburgh area. No
  separate state step.
- "Look for jobs in other countries?" Default: no.
- No: an on-site or hybrid job in another country is out.
- Remote jobs tied to another country ("Remote - India") go in their own tab,
  "Remote, listed in another country". Companies do not use that wording the
  same way, so Job Crush does not decide. They are not hidden and not thrown
  away, but they do not flood the main list. Plain "Remote", or remote in the
  home country, stays in the main list.
- Yes: the user is ready to move there. Jobs in other countries come in.
  Rare, but allowed. Do not make these people leave Job Crush.
- Hybrid counts as on-site. A hybrid job in Jakarta means being in Jakarta.

**2. Job source coverage.** The source interface and the roster are done;
Remotive and Arbeitnow are wired. Every other free board is a class that
implements `JobSourceProvider` and a row in the roster. No scraping — APIs and
published feeds only.

**3a. The rest of Workday and Taleo.** Taleo and Workday's second link shape
shipped on 2026-10-23 (see Shipped). What is left:

- **Check it from a real install.** The Workday and Taleo calls were checked
  live in a browser. They were NOT run from the app itself, because the cloud
  machine that wrote the code cannot reach them. If either refuses requests
  that do not come from a browser, the first pasted link will say so in the
  message, with the status code.
- **Company and title only, no link — the rest.** Done for the 12 Workday
  companies in the company list (2026-10-16): a typed company that is on the
  list is searched on its recorded Workday site. Any other Workday or Taleo
  employer still needs a pasted link, or its site added to the list.

**4. OAuth for OpenAI (Sign in with ChatGPT).** Launched 2 August 2026 as a
closed beta: six partners, no open enrollment. The flow gets built against the
published shape and sits behind an honest "waiting on OpenAI program access"
state rather than a button that does nothing.

**11. OCR for scanned documents.** People scan real-world paper and find it
useless in the digital world — the transcript that started this was a photo of
a page, and Job Crush could only tell them so. Local OCR must not spend the
user's AI credits: nothing reaches for the brain on its own. Handing the page
to Moonlight instead is a separate, offered choice the user makes knowingly,
and on hard pages — handwriting, odd layouts — it is the better reader. See
item 14.

**13. Every job knows where it can be applied to — the rest.** Still to do:
when the employer's own board has no match and a brain is connected, ask
Moonlight for the employer's hiring-system account name (a person reads "The
Home Depot" and knows to try "homedepot"). Left out for now because it
spends AI credits on every crush; Ultra to decide.

**14. Convert a document from one format to another.** Somebody asks for a Word
copy and all the user has is a PDF. Today that means searching the web for a
converter, landing on whichever ad-funded site paid for the top result, and
uploading a document with their home address and their whole work history on it
to a stranger. Job Crush should just do it.

**This needs no brain and costs no credits.** Both halves already exist and are
already shipped: `DocumentTextExtractor` reads PDF, .docx and plain text
(ProDocs takes those drops today), and `ZipArchiveWriter` and `PacketExporter`
write .docx and PDF (Staging exports those today). Nothing new has to be
learned — the two ends have to be joined and given a button. It works with no
AI connected, offline, and the document never leaves the machine, which is the
whole selling point over the search-result converters.

Where it goes: **ProDocs**, on each document — "Save a copy as ▸ Word / PDF /
plain text". The file is already there; that is where a person will look for
it. The Moonlight chat page should also accept a dropped file and do the same
thing, because that is where people will ASK for it, but the chat page is the
second door, not the feature.

*What the plain conversion cannot do, and what to offer instead.* Two places
this falls short, and neither one is allowed to end in an apology. When all
else fails, be helpful — we can do these, so we say so, and the way to do it is
one click away and not a paragraph of instructions.

**The page design does not survive.** The words come out of the PDF and a new
Word file is built from them; nothing carries the fonts, the columns, the
margins or the rules. A two-column resume becomes one column of plain text. The
button says that before it runs, and next to it: *"Want it to look right?
Moonlight can lay it out properly."* Moonlight is handed the extracted words
and writes them back as a structured document — headings, dates, bullets in the
right places — which `MarkdownDocumentReader` and `PacketExporter` already turn
into a .docx. It is not a copy of the original PDF and is not sold as one. It
is a clean, well-shaped Word resume, which is what the person actually wanted.

**A scanned PDF has no words in it at all.** It is a picture of a page. The
message says exactly that, then how much of the document is affected, then the
way out — and the way out is a button sitting right there, not an instruction:
*"Moonlight can read this. Do it now?"* One click and it is done. A connected
brain looks at the image and reads it back as text; on handwriting and odd
layouts it beats the OCR in item 11 outright. The user is told it spends their
AI credits before it runs, and chooses. Typing it in by hand stays on the
screen as the other option, because somebody with no brain connected still
needs a way forward — but it is no longer the ONLY thing we can offer, and it
stops being the headline.

Two things in the existing detector have to change for this:

*Count the pages, not the document.* `extractFromPortableDocument` adds up the
text from every page and compares the total against 40 characters. A five-page
PDF with one real text page and four scans clears 40 easily and is reported as
read, with four fifths of it silently missing — worse than a full scan, because
a full scan at least tells the user. Count the pages that gave nothing and say
it: *"3 of the 5 pages in this PDF are pictures, so Job Crush could not read
them."* Then the same button.

*The current message is now wrong.* It ends with "if a scan is all you have,
type the important parts into Experience & Education yourself", which was true
when nothing could read a picture. It stops being the last resort the day
Moonlight can read one. That paragraph is where the offer goes.

Neither offer appears when no brain is connected. There the message says what
is missing and how to connect one, same rule as item 6.

*Done 2026-10-16:* the page count for picture pages, and the layout offer
("Word, laid out by Moonlight"). Still to do: the "Moonlight can read this"
button for scans, which needs a brain that reads images.

**16. A list of companies — the rest.** The list and the button shipped
2026-10-16 (see Shipped). 29 of the 52 companies have no feed recorded yet.
For each, find its Workday site (or other feed), check it live, and add the
followLink. Workday guesses that failed on 2026-10-16:
bostonscientific.wd1/BSCCareers and garmin.wd1/Garmin. The Greenhouse board named "ics" is a cooling company, not ICS the Qt
consultancy. One medical company was on the starter list in
`docs/job-crush-handoff.md` and was left off the shipped file on purpose (see
the questions in the handoff).

**17. Career presets — the rest.** The C++ and Qt preset and the clearance
flag shipped 2026-10-16 (see Shipped). Still to do: when a brain is attached,
Moonlight reads the user's documents, works out their line of work, and
builds a preset and a company list for it. The scorer still counts every
skill the same; the preset file already keeps "strongSkills" and
"goodSkills" apart, so weighting strong ones higher is a change to
ProspectScorer only.

**19. More feeds — careers pages.** Many employer careers pages carry
`JobPosting` data for Google; `PostedJobPageReader` already reads it from a
pasted page, so a company with no hiring system feed can still be checked
from its careers page. SmartRecruiters shipped 2026-10-16 (see Shipped).
Workday is item 3a.

**20. A web version of Job Crush.** Ultra, 2026-10-11. A portfolio piece that
shows web work on top of desktop work, built on Qt 6's newer server-side tools
(Qt HTTP Server, Qt WebChannel), not on Qt for WebAssembly. Users pay a small
subscription for the gathering work. It shows only publicly listed jobs.

The web version and the desktop version run under different rules. One flag
says which one is running. On the web: no full copied job descriptions, only
key facts or a short summary in our own words, plus a link to the employer's
posting. The desktop app keeps its full behavior. The web version tells users
the desktop app does more, and offers it.

Both versions read from one shared job collector, so the collecting is written
once. The full plan is in `docs/job-crush-handoff.md`: how the layers split between
browser and server, one database with a user ID on every row, logins through an
existing login service, and the risks. Get one hour with a lawyer on the terms
of use before charging anyone.

Test first. Qt HTTP Server is new and basic. Before building anything else,
build a small server that answers one job search as JSON through the real
Logic and Model layers, and load-test it. If it falls short, keep Logic and
Model in C++ and put a common web server in front. The HTML front end does
not change either way.

Expect thin web ViewModels. The desktop ViewModels hold state and send
signals to QML. The web needs ViewModels that turn one request into one JSON
answer and keep nothing. Logic and Model are shared as they are, but Logic
must work when two users ask at once: one database connection per request,
and every row filtered by user ID.

**21. Many users, one login.** Ultra, 2026-10-11. Job Crush is multi-user
from here on, on the desktop too. Each person logs in and the app knows who
they are. Use a standard login service (Log in with Google first). The desktop
opens the same web login in the browser and gets the user back. Job Crush needs
the internet to find jobs anyway, so an online login costs nothing.

The rule for all new Logic code: every call takes the user ID as an input.
A user's data lives in the database under their user ID, never in a Logic
object's memory. On the web, one running program serves many people at the
same moment. A Logic object that remembers "the current user" will hand one
person's search to another.

Store each job once, shared by everyone. A separate small table says which
user saw it, saved it, or put it on their board. Index the user ID column from
day one. Clear out jobs that have been gone for months and sit on nobody's board.


**25. A Windows installer — check it on Windows.** Built 2026-10-16 (see
Shipped), but it has never run on a Windows machine. Run the "Windows
installer" GitHub workflow once, install the result on a computer with no Qt
(or a fresh Windows user account), and check that Job Crush starts, the
database opens, a PDF drops into ProDocs, and Claude desktop can reach the
bridge.

Ultra, 2026-10-23: when the next few features and the code cleanup are done
and he has looked at the code, remind him to put a new installer on GitHub as
a Release, so employers can download it.

**26. A website with short videos.** Ultra, 2026-10-16. Do this once the app
is feature ready. One very fast video: set up Job Crush and get started. Then
a few short optional ones: AI Buddy Mode, and making resumes and cover letters
with Job Crush. Short and quick, each one.

**30. Say why a job was closed.** Ultra, 2026-10-19. When a card goes to
Closed, offer a reason. It is optional. The user can skip it. Reasons:
- They said no (rejected).
- Not a good fit.
- The posting came down and they never contacted me.
- I withdrew.
- I turned down an offer.
- Other, with a short note.

Store the reason with the card, show it on the card, and let the user change
it later. Add a "Closed because:" line to JobQuest.txt and to the AI Buddy Mode
text, so an outside AI (like the Make Money Now project, which now makes job
quests from Gmail) can fill it in too. This also answers the open question
from 2026-09-15: a job dropped after a closer look and a rejected application
both land in Closed, and the Stats page cannot tell them apart. With a reason,
it can.


**34. Discoveries tabs: pick more than one.** Ultra, 2026-10-25. The tabs
show one list at a time. The user wants to see several at once, for example
Manual Add plus Companies you follow. The row mixes two kinds of tab: ways
of looking at every job (Top Prospects, Manual Add, Outside your area) and
the site a job came from. A redesign should make that difference clear.
Also: the "Show me" button for a job just added lives inside the Add form,
so it goes away when the form closes.

**33. Bugs found in the comment pass.** 2026-10-25. Found while every file
was read for comments. Not fixed then, because that pass changed no
behavior. Fixed in the same pass: a possible crash when a chat is cleared
mid-answer; the education "edited by you" flag was never saved on insert; a
scorer test that never ran; two links opened without the http/https check;
an empty USAJOBS apply link.
- JobSearchProfileViewModel.cpp: "$120,000.50" is read as 12000050, and a
  number too big for an int becomes 0.
- EducationListViewModel.cpp and WorkExperienceListViewModel.cpp: when a
  save fails, the edit stays on screen but is not saved.
- ProfessionalDocumentListViewModel.cpp: sizes can show 0.1 MB low (a tenth
  of a megabyte is taken as 1024*105 bytes).
- StagingWorkbench.cpp: the "Checklist" title branch can never run.
- TaskInstructionWriter.cpp: with no history and no documents, the early
  return drops the user's notes on the job.
- ActiveJobsFolder.cpp: the folder check also refuses a sibling folder such
  as "Active Jobs Old".
- JobQuestFile.cpp: a blank line inside Notes can leave old note text behind
  after a rewrite.
- FollowedEmployerRoster.cpp: a link to a board Job Crush can't read is
  counted as "already on your list".
- UrlCanonicalizer.h: gh_jid and ashby_jid are stripped as tracking
  parameters, but they are job ids, so two jobs can get the same link.
- WordDocumentReader.cpp: "&amp;" is decoded first, so "&amp;lt;" becomes
  "<". Numeric escapes are never decoded.
- DroppedFileClassifier.cpp: on a tie the first kind wins, so "Reference
  letter" is filed as a cover letter.
- ResumeInsightParser.cpp: words match inside longer words ("lead" in
  "Leadership").
- AcademicTranscriptReader.cpp: winter sorts last in the year.
- FormattedDocumentWriter.cpp: the PDF check only asks if the file exists,
  so an old file passes.
- InlineEditField.qml: showStoredText() breaks the binding to fieldText.
- TokenEntryField.qml: Tab can't leave the field.
- AppPreferences::loadFromSettings never sends moonlightTipsTurnedOnChanged.
- removePacketForApplication is never called, so a deleted campaign leaves
  its packet rows behind.
- Messages that end with "try again" or no next step (UI rule): see
  StagingWorkbench, StagingPacketViewModel, JobPipelines,
  PostingStillListedChecker, TaskReplyReader, ProDocsIntake,
  MoonlightDocumentCleanup, DocumentTextExtractor, Arbeitnow and Remotive
  sources, BrainChatSession (the reply's suggested next step is never shown).
- Logic that sits in a viewmodel or view and belongs in the Logic layer:
  AiCredentialRosterViewModel (which provider a key is), JobSearchProfile
  ViewModel (preset merge), StagingPacketViewModel (export folder and file
  name), the provider list and a/an rule in SettingsPage.qml, the next
  document kind in ProDocsPage.qml. Some viewmodels call repositories
  directly: Education, WorkExperience, ProfessionalDocumentList,
  StagedJobList, StagingPacket.

---

## Shipped

Taleo (item 3a). A pasted Taleo link is read, a Taleo company can be
followed, and Taleo jobs can be checked for "gone". Taleo has no public API;
Job Crush asks the same JSON search the careers page uses, which needs a
"portal" number read once from the site's search page. Searching for a job's
number finds exactly that job, which is how one job is read and checked. The
description comes from the job's own page. Followed Taleo companies are
searched by the profile's titles, the same as Workday. Checked live in a
browser on 2026-10-23 against hdr.taleo.net and cu.taleo.net, without
cookies.

Workday's second link shape (item 3a). Links like
wd5.myworkdaysite.com/recruiting/uw/UWHires/job/... are read, followed and
checked. The JSON service there is the same as on myworkdayjobs.com. Checked
live in a browser on 2026-10-23 against the University of Washington site.

The window no longer freezes during a sweep (item 27). Found jobs are saved
on a background thread, one company at a time, as each company answers, and
Discoveries fills in while the sweep goes on. The rows for each tab are also
loaded and scored on that thread. The database is in WAL mode, so the screen
can read while the background thread writes. Two more causes were found and
fixed: the "have we seen this job?" lookup read the whole table for every job
saved (it now uses its index), and the held-back count was worked out three
times per screen update. Tested in the cloud with a fake sweep of 7,000 jobs:
the window never paused for more than about 0.1 second. A real sweep has not
been run yet.

Staging leaves out closed jobs (item 31). A job in Closed is not listed, and
if it was the one showing, the packet panel clears.

"Gone" now needs two answers that agree (new item 32, Ultra 2026-10-23,
after three jobs were marked gone that were still listed). A job is marked
gone only when its own page on the board says "not found" AND the company's
list of jobs, or a search of it for the title, does not have it by id or by
title. If either answer is missing, the job counts as "couldn't be checked
for sure". The job's own link is checked first, then every other place it
was found; any one that still lists it wins. The sweep of followed companies
uses the same two-answer check before marking anything. A card marked gone
has "It's still open, I checked": the mark comes off, and Job Crush never
marks that job gone again, even if a check was already running when the
button was pressed. A company list cut short at the page limit counts as
"couldn't be checked". On Workday the board is also searched for the job's
requisition number, in case the employer changed the title. Tested with a
fake board; not run against live boards yet.

Brain levels (item 6). Each brain has Quick, Balanced and Best in Settings,
with the real model name shown small. Model names checked 2026-10-23:
Anthropic claude-haiku-5-5 / claude-sonnet-5-5 / claude-opus-5-5; OpenAI
gpt-5.6-luna / terra / sol; Gemini gemini-3.5-flash-lite / gemini-3.5-flash /
gemini-4-argon; OpenRouter's "latest" Claude names; Ollama picks the
smallest, middle or largest model the user downloaded. This also replaced
three old defaults that are shutting down: gemini-2.5-flash (closed to new
projects), claude-sonnet-4-5 (shuts 2026-11-30) and gpt-5-mini (shuts
2026-12-11). Still to do: ask each vendor for its model list at runtime, so
a retired name never breaks a brain. Until then, this list must be updated
by hand.

"Word, laid out by Moonlight" (the layout half of item 14). On ProDocs, next
to "Save a copy as", shown only when a brain is connected. Moonlight gets the
document's words and sends them back as headings, dates and bullets, with the
words unchanged; that is written as a new Word file where the other copies
go, named like Resume_Moonlight_202610162100.docx (year, month, day, hour,
minute, so the copies sort by name). Built and tested in the cloud, but NOT
run against a real brain yet.

Crushing a job looks for it elsewhere (most of the rest of item 13). When a
job lands on the board, Job Crush looks for the same job on the employer's
own board (Greenhouse, Lever, Ashby, SmartRecruiters, Workday) and ADDS it
as another place to apply. The job itself is never replaced, and a job
already read off the employer's own board sends no requests. Each card has
"look for this job elsewhere" to run the same search by hand, for jobs added
before this existed. Quiet when nothing turns up. Not run against real
boards from the cloud.

The Apply block in Staging (part of item 13). Above "Download as", one button
per place the job can be applied to ("Apply on Greenhouse", "Apply on
USAJOBS"), plus the job's own link. The user picks; Job Crush never applies.
And "Write it out as one file" now saves the packet into the job's own folder
in Active Jobs when it has one, beside its JobQuest.txt, as item 22 asked.
"Open the folder" opens that same folder.

"Moonlight, clean this up!" (the second item 6). Each readable document on
ProDocs has the button, with the line above it. It hands the document's text
to the connected brain, which reads the jobs and schools again into a fixed
JSON shape, never guessing. The answer replaces Job Crush's own reading of
that one document; anything the user typed into is kept. With no brain the
button is greyed and the line says to connect one in Settings. It spends AI
credits, so it only runs when pressed. Checked with a stand-in brain: the
two schools the built-in reader had glued together came out as two.
Note: "Read my documents again" on the Experience tab still throws away every
untouched entry, Moonlight's included, and reads with the built-in reader.

A hints strip on the Moonlight chat page (item 15). One tip at a time above
the message box, a new one every 20 seconds, each naming a feature that
exists today (MoonlightHints.h). "hide tips" hides it until Job Crush is
closed; the tips come back the next time it opens.

Paste any text and let Moonlight fill the form in (item 12). "Add a job" has
a third box, "Or paste the text". With a brain connected, "Have Moonlight
fill it in" reads it (no web search, so it costs a few cents) and fills in
company, title, link, place, pay, posted date and remote. Anything the text
did not say stays empty with an amber edge and the hint "not in the text.
Type it if you know it". Moonlight is told never to guess, and a link that
is not a web address is thrown away. "Add this job" then runs the usual hunt
for the employer's own posting; if none is found, the form is what gets
saved, with the description. With no brain, the button is off and a line
says why. Checked on screen with a stand-in brain.
Fixed while here: a pasted job that no board answered for used to say "Job
Crush found the posting on Greenhouse". It now says no board had it. And
Ollama is never asked to search the web, since it cannot; the message says
so instead.

The C++ and Qt career preset (part of item 17). `data/career-presets/
cpp-qt.json` holds six job titles and the strong and good skills from this
item. "Fill in the C++ and Qt preset" in Settings adds them to the search
profile; nothing the user typed is removed. Discoveries rows say "Asks for a
security clearance" when the title or description does (secret, top secret,
TS/SCI, polygraph, "obtain a clearance"). A flag, never a filter.

A list of companies, kept as data (item 16). `data/company-lists/
cpp-qt-medtech-tech.json` holds the 52 companies from the starter list in
`docs/job-crush-handoff.md`, each with sector, hiring system, follow link and
the day it was checked. 23 have a live feed Job Crush can read, each checked
against the real feed on 2026-10-16 through a browser. On Greenhouse, Lever,
Ashby and SmartRecruiters: Anduril (24 C++ or Qt titles), Shield AI (8),
Epirus, Saronic, Skydio, Bosch, Continental, Lucid Motors, Motional, Zoox and
Waymo. On Workday: Northrop Grumman, Stryker, Medtronic, Leidos, GE
HealthCare, RTX (Collins, Raytheon, Pratt), Philips, Baxter, Abbott, Harman,
GM and Autodesk. Settings has "Add the C++ and Qt starter list", which
watches all 23 at once.

Following a Workday employer (part of item 3a). A Workday link can now be
watched in Settings. A Workday site holds thousands of jobs, so a sweep
searches it for each job title in the profile (up to three), instead of
reading it whole. A job two searches both find is kept once. Because a
search never reads the whole site, a Workday job missing from it is never
marked gone; the board's "Are these still open?" still checks Workday cards
one by one. The name shown comes from the site name with words like
"External Site" taken off. The sweep itself was not run, because the cloud
cannot reach Workday; the sites were checked live in a browser. The file ships inside Job Crush as a resource; change the file, not
the code.

SmartRecruiters (part of item 19). Followed companies, pasted links, the
company-and-title hunt, and "Are these still open?" all read SmartRecruiters
now, through its official public API with no key. Checked live on 2026-10-16
against BoschGroup through a browser, and the traps are written at the top of
`SmartRecruitersBoardSource.h`: the list has no description or link, a limit
over 100 is cut to 100, an unknown company looks the same as one with no jobs,
and search matches loosely. A whole-board read stops after 1,000 jobs and says
so, so a job past the stop is never marked gone. A test reads a real answer.

The capabilities file (item 7). Job Crush writes capabilities.txt into the
soul folder on every launch: every page, the Active Jobs folder, AI Buddy
Mode, and what Moonlight cannot do from the chat. It goes last in what the
brain is told, so it wins over an older soul.txt that still talks about
phases. The user's soul.txt and primeDirectives.txt are never touched. New
installs get a soul.txt without the old "rolling out in phases" paragraph.
Keep capabilities.txt current: when a feature ships, it goes in
AiBrainSoul.cpp.

Trace On glow (item 9). GlowHalo.qml draws fading rings around a lit shape,
using glowStrength, so only the two Trace On themes change. Lit for now: the
board's column stripes, a column while a card is dragged over it, the
sidebar's current-page bar, and the active tab on Discoveries. Plain
rectangles, no shader effects, so it builds on every Qt 6 version. Ultra to
judge the look; more shapes can take a GlowHalo in one line each.

Discoveries rows say when a posting is gone (the rest of item 18). A sweep of
"Companies you follow" reads each company's whole board. A job from a company
that answered, which its board no longer lists, gets an amber outline and the
line "Gone from the company's own board since 15 Oct. It may be filled." A job
that comes back loses the mark. A company that did not answer this sweep
changes nothing. Search sites (Remotive, Arbeitnow) are never used for this,
because they only show a slice of a company's jobs. Checked on screen with a
marked job; the sweep itself was not run, because the cloud cannot reach job
boards.

The rest of the brains (item 5). OpenAI with an API key, and Ollama. OpenAI
uses gpt-5-mini, and gpt-5-search-api when a search is asked for (OpenAI's
chat API only searches with that model; checked in OpenAI's guide on
2026-10-16). Not run against OpenAI itself: the cloud machine cannot reach it.
Ollama runs on the user's own computer at localhost:11434, needs no key, and
uses the first model the user has downloaded. Settings hides the key boxes on
the ollama tab and says how to get started. Ollama is never picked for a user
who has not chosen it, because it would fail for anyone who never installed
it. Checked against a stand-in Ollama server: the chat streams. OpenRouter,
OpenAI and Ollama now share one reader for the streamed answer
(OpenAiStyleStream).

A Windows installer (item 25), not yet run on Windows. `cmake --install` now
gathers Job Crush, the AI app bridge, and every Qt file they need (Qt's
windeployqt does the gathering), plus the Microsoft C++ runtime for a Visual
Studio build. `tools/installer/JobCrush.iss` is the Inno Setup script: a
per-user install with no administrator password, a Start menu entry, an
optional desktop icon, and an uninstaller that never touches the user's jobs.
Two ways to make it: the "Windows installer" GitHub workflow (by hand, or by
pushing a v tag, which also attaches it to a GitHub Release), or
`tools/installer/build-installer.ps1` on a Windows computer with Inno Setup.

The Active Jobs folder (item 22). Every job on the board has its own folder in
Documents/Job Crush/Active Jobs, holding a JobQuest.txt file. The file is plain
"Name: value" lines; only Company and Title are required. Any outside AI that
can write a text file can add a job or change one. Job Crush watches the
folder: a new or changed file moves the board one second after the last
change, and a card moved, dated or noted on the board is written back into its
file. Lines Job Crush does not know are kept. A file with a problem is listed
in Settings with a sentence saying how to fix it. Every card gets a folder,
including jobs Job Crush found itself, and the open card has an "open this
job's folder" link. A folder the user deletes is not made again.
"Import a folder" in Settings copies job folders in from anywhere, so the
originals are never changed. It reads the older layout too (folder named
"Company - Title" with a HOW TO APPLY.txt): those go in Applied, dated by the
folder's last change. A job already on the board keeps its column. Only the
folders directly inside the one picked are read, so a "1 - Send These" folder
inside it is left alone. AI Buddy Mode in Settings has the blurb, a Copy
instructions button, and the exact text that gets copied.
Settled while building: a packet (and its brain questions) is only started for
a folder job that lands in Crushed, so importing sent jobs spends no AI
credits. A file with no Notes line or no Applied on line leaves the card's
notes and date alone, so an AI that rewrites a file and forgets a line does
not wipe them.

AI apps work Job Crush directly (item 23). Claude desktop, and any AI app that
speaks MCP, can list the board, add a job, mark one applied, move one, add a
note, and ask what is waiting on the user. The AI app starts a small bridge
program, jobcrush-ai-bridge.exe, that sits next to jobcrush.exe. The bridge
passes each action to the running Job Crush through a local connection that
only the same Windows user can open. Job Crush stays the only program that
writes its database, through the same Logic the screens use. If Job Crush is
closed, the AI is told to ask the user to open it. Settings has a box to turn
it off and the exact text to paste into Claude desktop. ChatGPT's and Gemini's
desktop apps are not set up yet; their settings differ and were not checked.

Column meanings on hover (item 24). Resting the mouse on a column title on Job
Pipelines shows one line saying exactly when a card belongs there.

Converting a document (most of item 14). ProDocs has "Save a copy as" Word,
PDF or plain text, using DocumentConverter, with no brain and nothing leaving
the machine. A PDF with some picture pages says how many. Still to do from
item 14: the two Moonlight offers (lay it out properly; read a scanned page).

Tests. `tests/` holds QtTest programs for the job scorer, the search profile,
the date reader and the Workday reader: 4 programs, about 50 checks. They build
the Logic classes on their own and need no window, database file or network.
CI runs them after the build on Windows and Linux. The README says how to run
them. Settings are written to a temporary INI file, never the real registry.

"Outside your area" is its own tab on Discoveries. It sits after Manual Add,
shows how many jobs it holds, and only exists while you have named a place.
It lists every held-back job from every site, ranked like Top Prospects. The
bar's "Show me" button goes to it and "Back to my area" leaves it. The site
tabs now always show only jobs inside your area.

Cards say when a posting is gone. "Are these still open?" on Job Pipelines
asks each employer's board about every open card (not Closed) that has a
Greenhouse, Lever, Ashby or Workday link. A gone job gets an amber edge, a
line saying so, and on the open card the date it went missing and a "search
the web for it" link. Nothing is deleted. A job that comes back loses the
flag. It only runs when pressed, never on its own.

The applied date can be edited. Open a card on Job Pipelines and type the day
you sent it in "Applied on": 10/8, 8 Oct, Oct 8 2026, 2026-10-08, yesterday,
or 3 days ago. A date with no year means the most recent one. A date in the
future is refused and the old date stays, with a sentence saying why. Clearing
the box clears the date, for a card dragged to Applied by mistake. The Stats
page follows the new date. Dragging a card to Applied still stamps today, and
the sentence now says you can change it.

Pasted Workday links work. Most defense, aerospace and medical-device
employers post on Workday, so this is the board that matters most for a
C++/Qt job search in those fields. Job Crush reads the job from Workday's own
service and saves the employer's full posting. Workday sends an internal
office name where the company name should be ("0090 CORP-Corporate Office"),
so the company name comes from what you typed or from the site name in the
link. The calls and five traps are written at the top of
`WorkdayBoardSource.h`. What is left is item 3a.

Transcripts are read as schooling. A document filed as a transcript goes to
its own reader, which does not need an EDUCATION heading. It reads the school,
the credential, the subject, and the first and last term, and skips the course
lines on purpose.

The Manual Add tab. Jobs you add by hand get their own tab on Discoveries,
newest added first, and the location filter never touches it. Each row says
what happened: the board Job Crush found the posting on, or an amber outline
and a plain sentence saying no board had it and the rest is yours to fill in.
The sentence in the add panel now comes with a Show me button that takes you
straight to the row and marks it. Adding a job by hand still does NOT put it on
the board — the hunt for the real posting stays in the middle, on purpose.

Two source fields instead of one. What the code called `discoverySource` always
held the BOARD a posting was read off, never who found it, so it is now
`postingSource`, and a new `scoutSource` says who brought the job in. A job is
often both at once — you added it AND Job Crush found it on Ashby — and that is
exactly what the Manual Add tab shows. Databases written before this are
migrated on the next launch and keep every row.

Several posting sources per job. A job can live on more than one board. Every
route found is kept, with its board, its id there and its link, and the card on
Job Pipelines lists them all: "see this one on Ashby", "see this one on Lever".
Paste a Lever link for a company that is also on Ashby and both are saved,
because which door to walk through is the user's call, not the app's. This
swallowed item 13.

Adding a job by hand. Discoveries → "Add a job" takes a link, or a company and
a title typed off an alert email, and saves what the user gave it when no board
match comes back. Pasting a whole description in and having it read is item 12.

The Job Pipelines board (what used to be Phase 4): five columns, cards dragged
between them, notes on each card, and CRUSH on every Discoveries row to put a
job up there. Reaching Applied stamps the date, once, and dragging back out
does not erase it.

## Talk about first

**Editing the soul inside the app.** Today Settings shows the folder and opens
it. Editing in place would save a trip into a messy PC, and on a phone there
is no trip to make.

Settled already: **the AI never writes to `soul.txt` or the prime directives.**
Access is the user's, through the app, and nothing else touches them.

Open questions worth an argument first: are prime directives editable at all,
or shown read-only with an explanation? What stops a half-finished edit from
bricking the brain? And does an in-app editor undercut the "plain text files,
any editor, no recompiling" line that programmers reading this repo tend to
like?

---

## Back burner — not until the core is vetted

**A first-launch tutorial.** Very fast. Reachable later from Settings.
Dismissible, with an unchecked box reading exactly "Never Show Me This Again".
Building it before the features settle means writing it twice.

**Forwarding jobs in by email.** Give each person a virtual address when they
sign in — verified against their real one — so they can forward a LinkedIn job
alert straight into Job Crush. A LinkedIn alert carries only the first few
lines of a posting, but that is enough to go looking for the SAME job on the
sites Job Crush can actually read, and hand back the full description from
there. It turns the one board we will not scrape into a lead instead of a dead
end.

Cloudflare Email Routing is the candidate: inbound routing is free, catch-all
is supported, and an Email Worker can take the whole domain. Checked 23 Aug
2026 — the caps are **200 routing rules per domain** and **200 verified
destination addresses per account**, which rules out one rule per user; the
shape has to be a catch-all into an Email Worker that reads the address and
does the routing itself. Inbound messages are capped at 25 MiB.

Ultra has details to work out first. Not started until he says so.

**Google Calendar.** An interview earns a place in the calendar the user
actually looks at. Requested 2026-08-23.

Researched: **build the free version and it may be the only version needed.**
An "Add to calendar" button on a job reaching Interview, offering a proper .ics
file and a Google Calendar deep link the user confirms themselves. No Cloud
project, no client secret, no verification, no tokens, no privacy policy — and
it works with Outlook and Apple Calendar too, which matters when somebody's
real calendar is their day job's. The full API version needs each user to
bring their own Google credentials, because Google's own terms say developer
credentials "may not be embedded in open source projects" and a shared one caps
the whole app at 100 lifetime users.

Upcoming interviews get listed from Job Crush's own database. We already have
the data; no calendar API is needed to show our own interviews back to us.

**Paid and subscription job sources.** Not v1, not alpha. Later.

**Scraping.** Avoided. Revisited only where it is clearly legal, and only if
the free APIs turn out not to be enough.
