# Job Crush — Backlog

This file lists the work that is queued, the work that is parked, and the work
that is done. It is written to be read. Every item says what the feature is,
what works today, and what is left to build.

Each item keeps its number for good. Numbers are not reused and are not
reordered, so "item 14" means the same thing in six months.

**Where the research is.** `BACKLOG_RESEARCH.md` sits in the "QtQuick App"
planning folder. It holds one section per item below: the web addresses, the
field names, the license problems and the decisions already made. Read the
first section of that file before starting any item here. Five of its findings
change the plan for an item.

**How to read an item.** An item has three parts, and any part can be missing:

- The first paragraph says what the feature is, in plain words.
- **Works now** says what is already built and running.
- **Left to do** says what is not built yet.

---

## Needs a decision from Ultra

These items cannot be built until Ultra answers a question. The question is
written out under each one. Nobody should guess at these.

**2. Read jobs from more job sites.** Job Crush finds jobs by reading job
sites. It reads two of them today: Remotive and Arbeitnow. The code that a new
job site plugs into is finished. Adding one more site means writing one new
class and adding one line to a list. No site is read by scraping its web pages. Job Crush
only reads a site's official data service or its published feed.

*The question:* which sites? This item has never named one. It cannot be built
until it does. Pick the sites and this becomes a day of work each.

**13. Ask the brain for a company's hiring-system name.** When Job Crush looks
for a job on the employer's own board, it has to guess the employer's account
name on that board. A person reads "The Home Depot" and knows to try
"homedepot". Job Crush cannot. A connected brain could answer that in one
question.

*The question:* this spends AI credits every time a job is crushed. Is that
worth it, or should it only run when the user presses something? Ultra decides.

**4. Sign in with ChatGPT (OAuth for OpenAI).** OpenAI opened this on 2 August
2026 as a closed beta with six partners and no way to sign up. Job Crush can
build the sign-in flow against OpenAI's published shape and put it behind a
screen that says "waiting on OpenAI to open this program", instead of a button
that does nothing.

*The question:* build it now against a program we cannot join, or wait? There
is no date for when OpenAI opens it.

---

## Next up

**29. Let the user pick where Job Crush saves files.** Job Crush saves document
copies and application packets in Documents\Job Crush Packets. When OneDrive
backs up the Documents folder, Windows reports the OneDrive copy as Documents,
so the files land in OneDrive. Many people want that, so it stays the default.
Settings needs a picker that says "Where Job Crush saves files", built the same
way as the Active Jobs folder picker.

**Works now:** AppPreferences already stores the folder path in
`exportFolderPath`.

**Left to do:** the control in Settings. Nothing else.

**28. Filter jobs by home country and home city.** Companies the user follows
post jobs in every country. Today any job marked remote gets through the
location filter, including "Remote - India".

**Left to do:** all of it. The design is settled:

- The user picks a home country, for example United States.
- The user picks a home city as a metro area, for example the Pittsburgh area.
  There is no separate state step.
- Settings asks "Look for jobs in other countries?" The answer starts at no.
- Answer no: an on-site or hybrid job in another country is filtered out.
- A remote job tied to another country, such as "Remote - India", goes in its
  own tab called "Remote, listed in another country". Companies do not use
  that wording the same way, so Job Crush does not decide for them. These jobs
  are not hidden and not thrown away. They are kept out of the main list so
  they do not swamp it. A plain "Remote" job, or a remote job in the home
  country, stays in the main list.
- Answer yes: jobs in other countries come in. The user is ready to move.
  This is rare. It is still allowed. Do not make these people leave Job Crush.
- A hybrid job counts as on-site. A hybrid job in Jakarta means being in
  Jakarta.

**3a. Finish Workday and Taleo.** Workday and Taleo are the two hiring systems
most defense, aerospace and medical-device employers use. Job Crush reads both
now.

**Works now:** a pasted Taleo link is read. A pasted Workday link is read, in
both of Workday's two link shapes. A Taleo or Workday company can be followed.
Both can be checked for "gone". A typed company name that is on the shipped
company list is searched on its recorded Workday site.

**Left to do:**

- *Run it from an installed copy.* The Workday and Taleo calls were checked by
  hand in a browser. They have never run from Job Crush itself, because the
  cloud machine that wrote the code cannot reach those sites. Workday or Taleo
  may refuse a request that does not come from a browser. If either does, the
  first pasted link will say so and show the status code.
- *Handle a typed company that is not on the list.* A Workday or Taleo
  employer that is not in the shipped company list still needs a pasted link,
  or its site has to be added to the list.

**11. Read scanned paper documents (OCR).** People scan paper and the scan is
a picture, not text. The transcript that started this item was a photo of a
page. Job Crush could only tell the user it could not read it.

**Left to do:** all of it. Two rules are settled. Reading the page on the
user's own computer must not spend the user's AI credits, and nothing calls
the brain on its own. Handing the page to Moonlight is a separate choice the
user makes, and on hard pages, such as handwriting or an odd layout, Moonlight
reads it better. See item 14.

**14. Turn a document into another format.** Somebody asks for a Word copy and
the user only has a PDF. Today that means searching the web for a converter,
landing on whatever ad-funded site paid for the top result, and uploading a
document that holds a home address and a whole work history to a stranger.
Job Crush does this itself, on the user's own computer, with no AI and no
internet.

**Works now:** ProDocs has "Save a copy as" for Word, PDF or plain text. A PDF
with picture pages says how many pages are pictures. ProDocs also has "Word,
laid out by Moonlight", which asks the brain to lay the words out as headings,
dates and bullets, and writes a new Word file.

**Left to do:** one thing. A scanned PDF holds no words at all. Job Crush
should say exactly that, say how many pages are affected, and put a button
right there that reads "Moonlight can read this. Do it now?" The brain looks
at the picture and reads it back as text. This needs a brain that can read
images. The message must say it spends AI credits before it runs.

Two rules for this one. Typing the page in by hand stays on screen as the
other choice, because somebody with no brain connected still needs a way
forward. Neither offer appears when no brain is connected. There the message
says what is missing and how to connect one, the same as item 6.

**16. Finish the company list.** Job Crush ships a list of 52 companies that
hire C++ and Qt people. Each entry holds the company's sector, its hiring
system, a link to its job feed, and the day that link was checked.

**Works now:** the list and the "Add the C++ and Qt starter list" button in
Settings. 23 of the 52 companies have a feed Job Crush can read.

**Left to do:** 29 companies have no feed recorded. For each one, find its
Workday site or other feed, check it live in a browser, and add the link to
the file. Two guesses already failed on 2026-10-16:
bostonscientific.wd1/BSCCareers and garmin.wd1/Garmin. Also note the
Greenhouse board named "ics" belongs to a cooling company, not to ICS the Qt
consultancy. One medical company from the starter list in
`docs/job-crush-handoff.md` was left out of the shipped file on purpose. The
reason is in the handoff questions.

**17. Finish career presets.** A career preset is a ready-made set of job
titles and skills for one line of work. The user applies it instead of typing
everything in.

**Works now:** the C++ and Qt preset, and the security-clearance flag on
Discoveries rows.

**Left to do:** two things.

- When a brain is attached, Moonlight reads the user's documents, works out
  what line of work they are in, and builds a preset and a company list for
  it.
- The job scorer counts every skill the same. The preset file already keeps
  "strongSkills" and "goodSkills" in separate lists. Scoring a strong skill
  higher is a change to ProspectScorer and nothing else.

**19. Read jobs off a company's own careers page.** Many employer careers
pages hold `JobPosting` data for Google to read. `PostedJobPageReader` already
reads that data from a pasted page. So a company with no hiring-system feed
can still be read from its careers page.

**Works now:** SmartRecruiters, shipped 2026-10-16.

**Left to do:** use `PostedJobPageReader` for a followed company that has no
feed. Workday is item 3a.

**20. A web version of Job Crush.** Ultra asked for this on 2026-10-11. It is
a portfolio piece that shows web work on top of desktop work. It is built on
Qt 6's server tools, Qt HTTP Server and Qt WebChannel. It is not built on Qt
for WebAssembly. Users pay a small subscription for the job gathering. It
shows only jobs that are publicly listed.

The web version and the desktop version follow different rules. One flag in
the code says which one is running. On the web, Job Crush never shows a full
copied job description. It shows key facts, or a short summary in our own
words, plus a link to the employer's posting. The desktop app keeps its full
behavior. The web version tells users the desktop app does more and offers it
to them.

Both versions read from one shared job collector, so the collecting code is
written once. The full plan is in `docs/job-crush-handoff.md`. It covers how
the layers split between the browser and the server, one database with a user
ID on every row, logins through an existing login service, and the risks.

**Left to do:** all of it, and in this order.

1. Test Qt HTTP Server first. It is new and basic. Build a small server that
   answers one job search as JSON through the real Logic and Model layers.
   Load-test it. If it is not good enough, keep Logic and Model in C++ and put
   a common web server in front of them. The HTML front end does not change
   either way.
2. Write thin web ViewModels. The desktop ViewModels hold state and send
   signals to QML. A web ViewModel turns one request into one JSON answer and
   keeps nothing afterward. Logic and Model are shared as they are, but Logic
   has to work when two users ask at the same moment: one database connection
   per request, and every row filtered by user ID.
3. Get one hour with a lawyer about the terms of use before charging anyone.

**21. Many users, one login.** Ultra asked for this on 2026-10-11. Job Crush
is a multi-user program from here on, on the desktop as well. Each person logs
in and the app knows who they are. Use a standard login service. Start with
"Log in with Google". The desktop app opens the same web login in the browser
and gets the user back. Job Crush needs the internet to find jobs anyway, so
an online login costs nothing.

**Left to do:** all of it. Three rules are settled.

- Every new Logic function takes the user ID as an input. A user's data lives
  in the database under their user ID. It never lives in a Logic object's
  memory. On the web, one running program serves many people at the same
  moment. A Logic object that remembers "the current user" will hand one
  person's search to another person.
- Store each job once and share it. A separate small table says which user saw
  it, saved it, or put it on their board. Index the user ID column from the
  first day.
- Delete jobs that have been gone for months and sit on nobody's board.

**25. Test the Windows installer on Windows.** The installer was built on
2026-10-16 and has never run on a Windows machine.

**Left to do:** run the "Windows installer" GitHub workflow once. Install the
result on a computer with no Qt on it, or on a fresh Windows user account.
Then check five things: Job Crush starts, the database opens, a PDF drops into
ProDocs, the AI app bridge answers Claude desktop, and the uninstaller leaves
the user's jobs alone.

Ultra asked on 2026-10-23 to be reminded about this. Once the next few
features and the code cleanup are done, and he has read the code, he wants a
new installer put on GitHub as a Release so employers can download it.

**26. A website with short videos.** Ultra asked for this on 2026-10-16. Do it
once the app is feature ready. Make one very fast video that shows how to set
Job Crush up and get started. Then make a few short optional ones: AI Buddy
Mode, and writing resumes and cover letters. Keep every one of them short.

**30. Say why a job was closed.** Ultra asked for this on 2026-10-19. When a
card moves to Closed, Job Crush offers a reason. The reason is optional and the
user can skip it. The choices are:

- They said no.
- Not a good fit.
- The posting came down and they never made contact.
- I withdrew.
- I turned down an offer.
- Other, with a short note.

**Left to do:** all of it. Store the reason with the card. Show it on the card.
Let the user change it later. Add a "Closed because:" line to JobQuest.txt and
to the AI Buddy Mode instructions, so an outside AI can fill it in too.

This also answers an open question from 2026-09-15. A job dropped after a
closer look and a job rejected after an application both land in Closed, and
the Stats page cannot tell them apart. A stored reason lets it.

**34. Let the user pick more than one Discoveries tab.** Ultra asked for this
on 2026-10-25. The tabs show one list at a time. He wants to see several at
once, for example Manual Add together with Companies you follow.

The tab row mixes two different kinds of tab. Some are ways of looking at
every job: Top Prospects, Manual Add, Outside your area. The rest are the site
a job came from. A redesign should make that difference obvious.

One smaller thing in the same place: the "Show the job" button for a job just
added sits inside the Add form, so it disappears when the form closes.

**35. Archive of applied jobs.** Ultra asked for this on 2026-10-26.

Today "take it off the board" runs `DELETE FROM jobApplication`. The row is
gone for good, and with it the date the job was crushed and the date it was
applied to. The Stats page counts only the cards still on the board, so the
effort disappears the moment the user tidies up.

**Left to do:** all of it. The design is settled:

- A job that reached Applied is kept forever, even after the user takes it off
  the board. Call this the archive.
- An archived job keeps its JobQuest data.
- A job that never reached Applied is deleted as it is today. Ultra does not
  want a record of how fast postings vanish.
- Everything in Stats is placed in time by the applied date. One date, no
  guessing.
- Interview and offer are tags on a job, not dates. "This one got an
  interview" is enough. The database stores no date for reaching Interviewing
  or Offer, and never has, so those dates cannot be recovered for past jobs
  either.
- Notes stay free text for anything the user wants to add.

**36. A date range on the Stats page.** Ultra asked for this on 2026-10-26.
The Stats page has no date range at all. The charts are fixed to the last 26
weeks and the totals are all-time.

**Left to do:** all of it.

- Ready-made ranges: this month, last 90 days, 6 months, a year back, all
  time.
- A custom pair of dates.
- A running total of every job that ever crossed the board inside the chosen
  range, whether or not the job is still on it. This needs item 35 first.

---

## 33. Bugs found while reading every file

Found on 2026-10-25, while every source file was read for its comments. None
of these were fixed then, because that pass was not allowed to change what the
program does.

Fixed in that same pass: a possible crash when a chat is cleared while the
answer is still coming in; the education "edited by you" flag was never saved
when a row was inserted; a scorer test that never ran; two links that opened
without the http and https check; an empty USAJOBS apply link.

Still open, grouped by what they break.

**Wrong numbers.**

- JobSearchProfileViewModel.cpp reads "$120,000.50" as 12000050. A number too
  big for an int becomes 0.
- ProfessionalDocumentListViewModel.cpp shows a file size 0.1 MB too low. It
  takes a tenth of a megabyte as 1024 times 105 bytes.
- ProspectScorer reads hourly pay such as "$45/hour" as $45,000 a year.
- ProspectScorer gives "Engineer, Software" 8 points instead of 35 when the
  target title is made only of common role words such as "Software Engineer".
- The "Outside your area (N)" count counts the same job twice when two sites
  found it, but the tab shows each job once. N can be higher than the number
  of rows.

**Wrong matches.**

- ProspectScorer treats one matching word as a matching place, so "New York"
  matches "New Jersey", and "Pittsburgh, PA" matches everywhere in
  Pennsylvania.
- ResumeInsightParser.cpp matches a word inside a longer word. "lead" matches
  inside "Leadership".
- DroppedFileClassifier.cpp picks the first kind on a tie, so a file named
  "Reference letter" is filed as a cover letter.
- AcademicTranscriptReader.cpp sorts winter last in the year.
- UrlCanonicalizer.h strips gh_jid and ashby_jid as tracking parameters. They
  are job ids, so two different jobs can end up with the same link.
- FollowedEmployerRoster.cpp counts a link to a board Job Crush cannot read as
  "already on your list".
- ActiveJobsFolder.cpp refuses a folder named "Active Jobs Old", because the
  check also matches a sibling folder.

**Data that gets lost or left behind.**

- EducationListViewModel.cpp and WorkExperienceListViewModel.cpp leave a
  failed edit on screen. It looks saved and is not.
- JobQuestFile.cpp can leave old note text behind after a rewrite, when there
  is a blank line inside Notes.
- TaskInstructionWriter.cpp drops the user's notes on a job when there is no
  history and no documents. An early return skips them.
- DiscoveryWorker::saveFindings counts a batch of 200 jobs as saved and new
  even when the batch fails at COMMIT.
- JobPostingRepository::markJobPostingAsHandAdded changes only scoutSource. A
  job a sweep found earlier, then pasted in by the user, still sorts on the
  Manual Add tab by the sweep time, so it may not be the top row.
- removePacketForApplication is never called. A deleted campaign leaves its
  packet rows in the database.
- WordDocumentReader.cpp decodes "&amp;" first, so "&amp;lt;" becomes "<". It
  never decodes numeric escapes.
- FormattedDocumentWriter.cpp only checks that the PDF file exists, so an old
  file passes the check.

**Dead code.**

- StagingWorkbench.cpp has a "Checklist" title branch that can never run.
- DiscoveredJobListViewModel returns the same value for
  SourceDisplayNameRole and PostingSourceDisplayNameRole.

**Screen bugs.**

- InlineEditField.qml: showStoredText() breaks the binding to fieldText.
- TokenEntryField.qml: Tab cannot leave the field.
- AppPreferences::loadFromSettings never sends
  moonlightTipsTurnedOnChanged.

**Messages that end with no next step.** The UI rule says every message gives
the user something to do. These end with "try again" or with nothing:
StagingWorkbench, StagingPacketViewModel, JobPipelines,
PostingStillListedChecker, TaskReplyReader, ProDocsIntake,
MoonlightDocumentCleanup, DocumentTextExtractor, the Arbeitnow and Remotive
sources, and BrainChatSession, which never shows the next step the reply
suggested.

**Logic sitting in the wrong layer.** These belong in the Logic layer and sit
in a view or a viewmodel instead: AiCredentialRosterViewModel decides which
provider a key belongs to; JobSearchProfileViewModel merges presets;
StagingPacketViewModel builds the export folder and file name;
SettingsPage.qml holds the provider list and the a-or-an rule;
ProDocsPage.qml picks the next document kind.

These viewmodels call a repository directly instead of going through Logic:
Education, WorkExperience, ProfessionalDocumentList, StagedJobList and
StagingPacket.

---

## Talk about first

**Editing the soul file inside the app.** The soul file tells the brain who it
is. Today Settings shows the folder and opens it. Editing it inside the app
would save a trip into a messy PC, and on a phone there is no trip to make.

Settled already: the AI never writes to `soul.txt` or to the prime directives.
Only the user changes them, through the app. Nothing else touches them.

Three questions to settle before any code is written:

1. Can the prime directives be edited at all, or are they shown read-only with
   an explanation?
2. What stops a half-finished edit from breaking the brain?
3. Does an editor inside the app go against the "plain text files, any
   editor, no recompiling" idea that programmers reading this repo tend to
   like?

---

## Back burner — not until the core is tested

**A first-launch tutorial.** Keep it very fast. Reachable later from Settings.
Dismissible, with an unchecked box reading exactly "Never Show Me This Again".
Building it before the features settle means writing it twice.

**Forward a job in by email.** Give each person their own email address when
they sign in, checked against their real one, so they can forward a LinkedIn
job alert straight into Job Crush. A LinkedIn alert holds only the first few
lines of a posting. Those few lines are enough to go looking for the same job
on a site Job Crush can read, and read the full description from there.

Cloudflare Email Routing is the candidate. Inbound routing is free, a
catch-all address is supported, and an Email Worker can take the whole domain.
Checked on 23 August 2026: the limits are 200 routing rules per domain and 200
verified destination addresses per account. That rules out one rule per user.
The shape has to be a catch-all into an Email Worker that reads the address
and does the routing itself. Inbound messages are capped at 25 MiB.

Ultra has details to work out first. Do not start until he says so.

**Google Calendar.** An interview belongs in the calendar the user actually
looks at. Requested 2026-08-23.

Researched, and the free version may be the only version needed. Put an "Add
to calendar" button on a job that reaches Interview. It offers a proper .ics
file and a Google Calendar link the user confirms themselves. That needs no
Cloud project, no client secret, no verification, no tokens and no privacy
policy, and it works with Outlook and Apple Calendar too. That matters when
somebody's real calendar belongs to their day job.

The full API version needs each user to bring their own Google credentials.
Google's terms say developer credentials "may not be embedded in open source
projects", and one shared credential caps the whole app at 100 users for its
lifetime.

Upcoming interviews can be listed from Job Crush's own database. The data is
already there. No calendar service is needed to show our own interviews back
to us.

**Paid and subscription job sources.** Not in the first release. Not in the
alpha. Later.

**Scraping.** Avoided. Look at it again only where it is clearly legal, and
only if the free data services turn out not to be enough.

---

## Shipped

Newest first. Each entry says what the feature does and what was never tested.

**Every ending of "look for this job elsewhere" says something** (2026-10-26).
The button on a job card used to end five different ways and say nothing on
three of them. Now every ending puts a sentence in the strip at the top of the
page: it found the job on another board, it searched and found nothing, the
card's link already points at the employer's own posting, the card has no
company name or job title, or the job is not in the database. When it does
find the job on a live board, the gone mark comes off, because Job Crush just
read the job off a working board. The strip itself used to cut the sentence
off at one line. It wraps now and the box grows to fit.

**Job Crush no longer says "I"** (2026-10-27). Job Crush talks about itself in
the third person and talks to the user as "you". Every button is a verb. Nine
labels changed: "It's still open, I checked" became "Mark this as still open",
"I've sent it" became "Mark it as sent", "Tailor my resume" became "Tailor the
resume", "Read my documents again" became "Read the documents again", "AI apps
may work my board" became "Let AI apps work your board", "Back to my area"
became "Back to your area", "Show me" became "Show them" on Discoveries and
"Show the job" in the add panel, and "My own note" became "Your own note".

**The two actions on a gone card are buttons** (2026-10-27). "Search the web
for it" and "Mark this as still open" were underlined amber text, the same
color as the warning above them, so they did not read as something to click.
They are now small buttons, each as wide as its own words.

**A card says when Job Crush cannot check it** (2026-10-27). Job Crush can
only ask a job board it can read. A card whose links go to LinkedIn, to
Indeed, to a company's own careers page, or to a whole job board instead of
one job cannot be checked at all. The open card now says so. Before this, such
a card looked exactly like a card that was checked and found still listed.

**Taleo** (part of item 3a, 2026-10-23). A pasted Taleo link is read, a Taleo
company can be followed, and a Taleo job can be checked for "gone". Taleo has
no public data service, so Job Crush asks the same JSON search the careers
page uses. That needs a "portal" number, read once from the site's search
page. Searching for a job's number finds exactly that job, which is how one
job is read and checked. The description comes from the job's own page. A
followed Taleo company is searched by the profile's job titles, the same as
Workday. Checked live in a browser on 2026-10-23 against hdr.taleo.net and
cu.taleo.net, with no cookies.

**Workday's second link shape** (part of item 3a, 2026-10-23). Links shaped
like wd5.myworkdaysite.com/recruiting/uw/UWHires/job/... are read, followed
and checked. The JSON service there is the same one as on myworkdayjobs.com.
Checked live in a browser on 2026-10-23 against the University of Washington
site.

**The window no longer freezes during a sweep** (item 27, 2026-10-23). Found
jobs are saved on a background thread, one company at a time, as each company
answers, and Discoveries fills in while the sweep is still running. The rows
for each tab are loaded and scored on that same thread. The database runs in
WAL mode, so the screen can read while the background thread writes. Two more
causes were found and fixed: the "have we seen this job?" lookup read the
whole table for every job saved and now uses its index, and the held-back
count was worked out three times per screen update. Tested in the cloud with a
fake sweep of 7,000 jobs. The window never paused for more than about a tenth
of a second. A real sweep has not been run.

**Staging leaves out closed jobs** (item 31, 2026-10-23). A job in Closed is
not listed. If it was the one showing, the packet panel clears.

**A gone job needs two answers that agree** (item 32, 2026-10-23). Ultra asked
for this after three jobs were marked gone that were still listed. Job Crush
marks a job gone only when the job's own page on the board says "not found"
AND the company's list of jobs, or a search of that list for the title, does
not have it by id or by title. If either answer is missing, the job counts as
"couldn't be checked for sure". The job's own link is checked first, then
every other place the job was found. Any link that still lists the job wins.
The sweep of followed companies uses the same two-answer check. A company list
cut short at the page limit counts as "couldn't be checked". On Workday the
board is also searched for the job's requisition number, in case the employer
changed the title. A card marked gone has "Mark this as still open": the mark
comes off, and Job Crush never marks that job gone again, even if a check was
already running when the button was pressed. Tested with a fake board. Never
run against a live board.

**Brain levels** (item 6, 2026-10-23). Each brain has Quick, Balanced and Best
in Settings, with the real model name shown in small text. Model names were
checked on 2026-10-23: Anthropic claude-haiku-5-5, claude-sonnet-5-5,
claude-opus-5-5; OpenAI gpt-5.6-luna, terra, sol; Gemini
gemini-3.5-flash-lite, gemini-3.5-flash, gemini-4-argon; OpenRouter's "latest"
Claude names; Ollama picks the smallest, middle or largest model the user has
downloaded. This also replaced three old defaults that are shutting down:
gemini-2.5-flash, closed to new projects; claude-sonnet-4-5, shuts
2026-11-30; gpt-5-mini, shuts 2026-12-11.

*Left to do:* ask each vendor for its model list while the app is running, so
a retired model name never breaks a brain. Until that is built, this list has
to be updated by hand.

**Word, laid out by Moonlight** (the layout half of item 14, 2026-10-16). On
ProDocs, next to "Save a copy as", shown only when a brain is connected.
Moonlight gets the document's words and sends them back as headings, dates and
bullets, with the words themselves unchanged. Job Crush writes that as a new
Word file where the other copies go, named like
Resume_Moonlight_202610162100.docx, so the copies sort by name. Built and
tested in the cloud. Never run against a real brain.

**Crushing a job looks for it elsewhere** (most of item 13, 2026-10-16). When
a job lands on the board, Job Crush looks for the same job on the employer's
own board and adds it as another place to apply. The job itself is never
replaced. A job already read off the employer's own board sends no requests.
Each card has "look for this job elsewhere" to run the same search by hand,
for jobs added before this existed. Never run against a real board from the
cloud.

**The Apply block in Staging** (part of item 13, 2026-10-16). Above "Download
as", one button per place the job can be applied to, such as "Apply on
Greenhouse" or "Apply on USAJOBS", plus the job's own link. The user picks.
Job Crush never applies for them. "Write it out as one file" saves the packet
into the job's own folder in Active Jobs when it has one, next to its
JobQuest.txt. "Open the folder" opens that same folder.

**Moonlight, clean this up!** (the second item 6, 2026-10-16). Each readable
document on ProDocs has this button. It hands the document's text to the
connected brain, which reads the jobs and schools again into a fixed JSON
shape and never guesses. The answer replaces Job Crush's own reading of that
one document. Anything the user typed is kept. With no brain the button is
greyed out and the line says to connect one in Settings. It spends AI credits,
so it only runs when pressed. Checked with a stand-in brain: two schools the
built-in reader had glued together came out as two.

*Known problem:* "Read the documents again" on the Experience tab still throws
away every untouched entry, Moonlight's included, and reads with the built-in
reader.

**A hints strip on the Moonlight chat page** (item 15, 2026-10-16). One tip at
a time above the message box, a new one every 20 seconds. Each tip names a
feature that exists today, listed in MoonlightHints.h. "hide tips" hides the
strip until Job Crush is closed. The tips come back the next time it opens.

**Paste any text and let Moonlight fill the form in** (item 12, 2026-10-16).
"Add a job" has a third box, "Or paste the text". With a brain connected,
"Have Moonlight fill it in" reads the text and fills in company, title, link,
place, pay, posted date and remote. It does not search the web, so it costs a
few cents. Anything the text did not say stays empty with an amber edge and
the hint "not in the text. Type it if you know it". Moonlight is told never to
guess, and a link that is not a web address is thrown away. "Add this job"
then runs the usual hunt for the employer's own posting. If no board has it,
the form is what gets saved, with the description. With no brain the button is
off and a line says why. Checked on screen with a stand-in brain.

*Fixed at the same time:* a pasted job that no board answered for used to say
"Job Crush found the posting on Greenhouse". It now says no board had it.
Ollama is never asked to search the web, because it cannot, and the message
says so.

**The C++ and Qt career preset** (part of item 17, 2026-10-16).
`data/career-presets/cpp-qt.json` holds six job titles and the strong and good
skills for that line of work. "Fill in the C++ and Qt preset" in Settings adds
them to the search profile. Nothing the user typed is removed. A Discoveries
row says "Asks for a security clearance" when the title or the description
says so: secret, top secret, TS/SCI, polygraph, or "obtain a clearance". It is
a flag on the row. It never filters anything out.

**A list of companies, kept as data** (item 16, 2026-10-16).
`data/company-lists/cpp-qt-medtech-tech.json` holds 52 companies, each with its
sector, hiring system, follow link and the day it was checked. 23 have a feed
Job Crush can read, and each of those was checked against the real feed on
2026-10-16 through a browser. On Greenhouse, Lever, Ashby and SmartRecruiters:
Anduril (24 C++ or Qt titles), Shield AI (8), Epirus, Saronic, Skydio, Bosch,
Continental, Lucid Motors, Motional, Zoox and Waymo. On Workday: Northrop
Grumman, Stryker, Medtronic, Leidos, GE HealthCare, RTX (Collins, Raytheon,
Pratt), Philips, Baxter, Abbott, Harman, GM and Autodesk. Settings has "Add
the C++ and Qt starter list", which follows all 23 at once. The file ships
inside Job Crush as a resource. Change the file, not the code.

**Following a Workday employer** (part of item 3a, 2026-10-16). A Workday link
can be followed in Settings. A Workday site holds thousands of jobs, so a
sweep searches it for each job title in the profile, up to three, instead of
reading the whole site. A job that two searches both find is kept once.
Because a search never reads the whole site, a Workday job missing from it is
never marked gone. The board's "Are these still open?" still checks Workday
cards one at a time. The name shown comes from the site name with words like
"External Site" taken off. The sweep itself was never run, because the cloud
cannot reach Workday. The sites were checked live in a browser.

**SmartRecruiters** (part of item 19, 2026-10-16). Followed companies, pasted
links, the company-and-title hunt, and "Are these still open?" all read
SmartRecruiters, through its official public data service with no key needed.
Checked live on 2026-10-16 against BoschGroup through a browser. The traps are
written at the top of `SmartRecruitersBoardSource.h`: the list holds no
description and no link, a limit over 100 is cut to 100, an unknown company
looks the same as a company with no jobs, and the search matches loosely. A
whole-board read stops after 1,000 jobs and says so, so a job past the stop is
never marked gone. A test reads a real answer.

**The capabilities file** (item 7, 2026-10-16). Job Crush writes
capabilities.txt into the soul folder every time it starts. The file lists
every page, the Active Jobs folder, AI Buddy Mode, and what Moonlight cannot
do from the chat. It goes last in what the brain is told, so it wins over an
older soul.txt that still talks about phases. The user's own soul.txt and
primeDirectives.txt are never touched. A new install gets a soul.txt with no
"rolling out in phases" paragraph. Keep capabilities.txt current: when a
feature ships, add it to AiBrainSoul.cpp.

**Trace On glow** (item 9, 2026-10-16). GlowHalo.qml draws fading rings around
a lit shape, using glowStrength, so only the two Trace On themes change. Lit so
far: the board's column stripes, a column while a card is dragged over it, the
sidebar's current-page bar, and the active tab on Discoveries. They are plain
rectangles, with no shader effects, so this builds on every Qt 6 version. Ultra
has not judged the look yet. More shapes can take a GlowHalo in one line each.

**Discoveries rows say when a posting is gone** (the rest of item 18,
2026-10-16). A sweep of "Companies you follow" reads each company's whole
board. A job from a company that answered, which its board no longer lists,
gets an amber outline and the line "Gone from the company's own board since 15
Oct. It may be filled." A job that comes back loses the mark. A company that
did not answer this sweep changes nothing. Search sites such as Remotive and
Arbeitnow are never used for this, because they only show a slice of a
company's jobs. Checked on screen with a marked job. The sweep itself was
never run, because the cloud cannot reach job boards.

**OpenAI with a key, and Ollama** (item 5, 2026-10-16). OpenAI uses gpt-5-mini,
and gpt-5-search-api when a search is asked for, because OpenAI's chat service
only searches with that model. Checked in OpenAI's guide on 2026-10-16. Never
run against OpenAI itself, because the cloud machine cannot reach it. Ollama
runs on the user's own computer at localhost:11434, needs no key, and uses the
first model the user has downloaded. Settings hides the key boxes on the
Ollama tab and says how to get started. Ollama is never picked for a user who
has not chosen it, because it would fail for anyone who never installed it.
Checked against a stand-in Ollama server: the chat streams. OpenRouter, OpenAI
and Ollama share one reader for the streamed answer, OpenAiStyleStream.

**A Windows installer** (item 25, 2026-10-16). `cmake --install` gathers Job
Crush, the AI app bridge, and every Qt file they need, using Qt's windeployqt,
plus the Microsoft C++ runtime for a Visual Studio build.
`tools/installer/JobCrush.iss` is the Inno Setup script: a per-user install
with no administrator password, a Start menu entry, an optional desktop icon,
and an uninstaller that never touches the user's jobs. There are two ways to
build it: the "Windows installer" GitHub workflow, run by hand or by pushing a
v tag, which also attaches it to a GitHub Release; or
`tools/installer/build-installer.ps1` on a Windows computer with Inno Setup.
Never run on Windows. That is item 25 in Next up.

**The Active Jobs folder** (item 22, 2026-10-16). Every job on the board gets
its own folder in Documents/Job Crush/Active Jobs, holding a JobQuest.txt file.
The file is plain "Name: value" lines. Only Company and Title are required. Any
outside AI that can write a text file can add a job or change one. Job Crush
watches the folder: a new or changed file moves the board one second after the
last change, and a card moved, dated or noted on the board is written back into
its file. Lines Job Crush does not know are kept. A file with a problem is
listed in Settings with a sentence saying how to fix it. Every card gets a
folder, including jobs Job Crush found itself, and the open card has an "open
this job's folder" link. A folder the user deletes is not made again.

"Import a folder" in Settings copies job folders in from anywhere, so the
originals are never changed. It reads the older layout too, a folder named
"Company - Title" with a HOW TO APPLY.txt inside, and those jobs go in Applied,
dated by the folder's last change. A job already on the board keeps its column.
Only the folders directly inside the one picked are read, so a "1 - Send These"
folder inside it is left alone. AI Buddy Mode in Settings has the blurb, a Copy
instructions button, and the exact text that gets copied.

*Settled while building it:* a packet, and its brain questions, is only started
for a folder job that lands in Crushed, so importing sent jobs spends no AI
credits. A file with no Notes line, or no Applied on line, leaves the card's
notes and date alone, so an AI that rewrites a file and forgets a line does not
wipe them.

**AI apps work Job Crush directly** (item 23, 2026-10-16). Claude desktop, and
any AI app that speaks MCP, can list the board, add a job, mark one applied,
move one, add a note, and ask what is waiting on the user. The AI app starts a
small bridge program, jobcrush-ai-bridge.exe, that sits next to jobcrush.exe.
The bridge passes each action to the running Job Crush through a local
connection that only the same Windows user can open. Job Crush stays the only
program that writes its database, through the same Logic the screens use. If
Job Crush is closed, the AI is told to ask the user to open it. Settings has a
box to turn it off and the exact text to paste into Claude desktop. ChatGPT's
and Gemini's desktop apps are not set up. Their settings differ and were never
checked.

**Column meanings on hover** (item 24, 2026-10-16). Resting the mouse on a
column title on Job Pipelines shows one line saying exactly when a card
belongs in that column.

**Converting a document** (most of item 14, 2026-10-16). ProDocs has "Save a
copy as" Word, PDF or plain text, using DocumentConverter. It needs no brain
and nothing leaves the machine. A PDF with picture pages says how many.

**Tests** (2026-10-12). `tests/` holds QtTest programs for the job scorer, the
search profile, the date reader and the Workday reader: 4 programs and about
50 checks. They build the Logic classes on their own and need no window, no
database file and no network. CI runs them after the build on Windows and
Linux. The README says how to run them. Settings are written to a temporary
INI file, never to the real registry.

**"Outside your area" is its own tab** (2026-10-12). It sits after Manual Add,
shows how many jobs it holds, and only exists while the user has named a
place. It lists every held-back job from every site, ranked like Top
Prospects. The bar's button goes to it and back. The site tabs now always show
only jobs inside the user's area.

**Cards say when a posting is gone** (2026-10-12). "Are these still open?" on
Job Pipelines asks each employer's board about every open card that is not in
Closed and that has a link it can read. A gone job gets an amber edge, a line
saying so, and on the open card the date it went missing. Nothing is deleted.
A job that comes back loses the mark. It only runs when pressed.

**The applied date can be edited** (2026-10-11). Open a card on Job Pipelines
and type the day it was sent in "Applied on": 10/8, 8 Oct, Oct 8 2026,
2026-10-08, yesterday, or 3 days ago. A date with no year means the most
recent one. A date in the future is refused, the old date stays, and a
sentence says why. Clearing the box clears the date, for a card dragged to
Applied by mistake. The Stats page follows the new date. Dragging a card to
Applied still stamps today, and the sentence now says the date can be changed.

**Pasted Workday links work** (2026-10-11). Job Crush reads the job from
Workday's own service and saves the employer's full posting. Workday sends an
internal office name where the company name should be, such as "0090
CORP-Corporate Office", so the company name comes from what the user typed or
from the site name in the link. The calls and five traps are written at the
top of `WorkdayBoardSource.h`.

**Transcripts are read as schooling** (2026-10-11). A document filed as a
transcript goes to its own reader, which does not need an EDUCATION heading. It
reads the school, the credential, the subject, and the first and last term, and
skips the course lines on purpose.

**The Manual Add tab** (2026-09-15). Jobs the user adds by hand get their own
tab on Discoveries, newest added first, and the location filter never touches
it. Each row says what happened: the board Job Crush found the posting on, or
an amber outline and a plain sentence saying no board had it and the rest is
the user's to fill in. The sentence in the add panel comes with a button that
goes straight to the row and marks it. Adding a job by hand still does not put
it on the board. The hunt for the real posting stays in the middle, on
purpose.

**Two source fields instead of one** (2026-09-15). The field the code called
`discoverySource` always held the board a posting was read off, never who
found it. It is now `postingSource`, and a new field, `scoutSource`, says who
brought the job in. A job is often both at once: the user added it and Job
Crush found it on Ashby. That is what the Manual Add tab shows. A database
written before this change is migrated the next time the app starts, and keeps
every row.

**Several posting sources per job** (2026-09-15). A job can live on more than
one board. Every route found is kept, with its board, its id there and its
link. The card on Job Pipelines lists them all: "see this one on Ashby", "see
this one on Lever". Paste a Lever link for a company that is also on Ashby and
both are saved. The user picks which one to use.

**Adding a job by hand** (2026-09-15). Discoveries, then "Add a job", takes a
link, or a company and a title typed off an alert email. It saves what the
user gave it when no board match comes back.

**The Job Pipelines board** (what used to be called Phase 4). Five columns,
cards dragged between them, notes on each card, and CRUSH on every Discoveries
row to put a job up there. Reaching Applied stamps the date once. Dragging a
card back out does not erase it.
