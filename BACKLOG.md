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

**2. Job source coverage.** The source interface and the roster are done;
Remotive and Arbeitnow are wired. Every other free board is a class that
implements `JobSourceProvider` and a row in the roster. No scraping — APIs and
published feeds only.

**3a. Taleo, and the rest of Workday.** Reading a pasted Workday link shipped
on 2026-10-11 (see Shipped). What is left:

- **Check it from a real install.** The Workday calls were checked live in a
  browser against Northrop Grumman's site. They were NOT run from the app
  itself, because the cloud machine that wrote the code cannot reach Workday.
  If Workday refuses requests that do not come from a browser, the first pasted
  link will say so in the message, with the status code.
- **Following a Workday employer** in Settings. Workday sites list thousands of
  jobs, so following one needs a search, not a full read. Settings says
  "Job Crush can't read Workday yet" for now, which is still true for this part.
- **Company and title only, no link.** Greenhouse, Lever and Ashby account
  names can be guessed from a company name. Workday's cannot: they need the
  server and the site name too. Item 16's company list is the fix. It records
  each company's Workday site once.
- **Workday's other link shape**, `wd3.myworkdaysite.com/recruiting/{tenant}/{site}/...`,
  is not read yet. No example has turned up to check it against.
- **Taleo** has no common JSON answer across companies. Cost it separately.

**4. OAuth for OpenAI (Sign in with ChatGPT).** Launched 2 August 2026 as a
closed beta: six partners, no open enrollment. The flow gets built against the
published shape and sits behind an honest "waiting on OpenAI program access"
state rather than a button that does nothing.

**5. The rest of the brains.** An OpenAI API-key provider (an OpenRouter clone
pointed elsewhere) and Ollama. Ollama needs `providerIsSelectable` to stop
demanding a credential — it runs on the user's own machine and has no key.

**6. Brain levels.** Per-provider model choice, so "which brain" and "how much
brain" are separate questions. Replaces the hardcoded model constant in each
provider. The naming is the hard part: model version numbers mean nothing to
somebody who just wants their cover letter to sound like them.

**6. "Moonlight clean this up!"** The button on ProDocs that hands a messy
parse to the connected brain. Label above it reads "If you have connected an
AI brain you can have…", and with no brain connected it greys out *and says
why*.

**7. An app-owned soul capabilities file** that refreshes on every launch, so
what Job Crush can DO stays current while the user's own edits to `soul.txt`
stay theirs. Today a stale soul file means a brain that describes an app that
no longer exists.

**8. Tests.** `ProspectScorer` and `JobSearchProfile` under QTest, wired into
CI. The README says the core is testable without a window; nothing currently
proves it, and that is the sort of claim a reader checks first.

**9. Trace On glow.** `glowStrength` exists as a theme token, sits at 1.0 for
the two Trace On palettes and 0.0 everywhere else, and nothing reads it yet.

**10. "Outside your search area" as its own tab.** `SearchAreaScope` already
carries the idea; the Discoveries bar already flips between held-back and
shown. The tab is the tidier home for it.

**11. OCR for scanned documents.** People scan real-world paper and find it
useless in the digital world — the transcript that started this was a photo of
a page, and Job Crush could only tell them so. Local OCR must not spend the
user's AI credits: nothing reaches for the brain on its own. Handing the page
to Moonlight instead is a separate, offered choice the user makes knowingly,
and on hard pages — handwriting, odd layouts — it is the better reader. See
item 14.

**12. Paste any text and let Moonlight fill the form in.** On "Add a job", a
box where the user pastes an email, a job alert, a recruiter message or a
posting copied off a page no API can reach. With a brain connected, Moonlight
reads it and fills in company, title, location, salary, remote or not, the
link, the posting date and the description. Anything it cannot find in the text
is left empty and marked as still needing the user — it never guesses a field.
Once the fields are filled, item 13 goes looking for the employer's own posting
of the same job.
Two things have to come first: "Add a job" today has only a link box and
company/title boxes, so the full set of editable fields has to exist before
anything can fill them; and with no brain connected the paste box greys out and
says why, same rule as item 6.

**13. Every job knows where it can be applied to, and Staging hands them
over.** A job can be reachable in more than one place at once: the aggregator
that found it (USAJOBS, Remotive, Jobicy) and the employer's own posting. Both
are worth keeping. The employer's own board is the source of truth — it is
current, it is not a rewrite, and it is where a lot of people would rather
apply — but the aggregator link is the one that was actually verified to exist,
and some people prefer applying there. Job Crush offers both and the user
picks.

Three parts:

*Finding them.* `CanonicalPostingResolver` already guesses the employer's
Greenhouse, Lever or Ashby account from the company name and matches by title.
It runs today only at the moment a job is added by hand, and it REPLACES the
lead with what it finds. Both of those change: it runs for any job, and it adds
what it finds instead of overwriting. Where the account guess fails and a brain
is connected, Moonlight gets asked — a person reads "The Home Depot" and knows
to try `homedepot`; a string rule does not. The same hunt widens to the sources
that take search terms (Remotive, Jobicy, and USAJOBS by keyword and
organization; Arbeitnow has no search).

*Storing them.* `JobPosting.sourceUrl` is one link and this is a list, so
apply routes become their own table: the job, the link, which site it is, and
whether it is the employer's own. A route already known is not added twice.
Finding nothing extra is a normal answer and leaves the job as it was.

*Using them.* An **Apply:** block in Staging, next to "I've sent it" — a button
per route, each naming where it goes ("Apply on USAJOBS", "Apply on Acme's own
site"). That is the whole point of the step: Job Crush hands the user the cover
letter written for this job AND the places to send it, in one screen, and the
user chooses. A "search for this job elsewhere" button, on the job itself, runs
the hunt again on demand for anything added before this existed or added
without a brain connected.

*When it runs.* On CRUSH, not on discovery. Settled 2026-08-28. A sweep turns
up a lot of jobs that are not a real match — the scorer tries, but it is not
perfect — and looking up the employer's own posting for every one of them
spends bandwidth, and the user's AI credits, on jobs they were never going to
open. Interest is the signal: the moment a job goes on the board, Job Crush
goes and finds out more about it. Jobs sitting in Discoveries are left alone.
The manual "search for this job elsewhere" button covers anything else.

Paid and subscription sites become routes the day they are added; nothing about
the block changes when they are.

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

**15. A hints strip on the Moonlight chat page.** A small line near the bottom
naming one thing Moonlight or Job Crush can do that the user has not found —
"Drop a PDF here and ask for a Word copy", "Paste a job email into Add a job
and I will fill the form in". Rotates, and dismissible. Written last, once the
features it advertises exist, because a hint pointing at something that is not
there is worse than no hint.

**16. A list of companies, kept as data.** Ultra, 2026-10-11. Job Crush checks
the companies a person wants to work for, instead of waiting for their jobs to
show up on a board. Employers want their jobs found. Most post through a hiring
system with a public feed, so the feed is the place to look.

The list lives in a data file or table, never in code, so a user can add and
remove companies. Each entry holds: company name, sector, hiring system, feed
link, and the date it was last checked.

The first list is C++/Qt in med tech and tech. It is a default, not a limit.
The starter companies are in `docs/job-crush-handoff.md` (2026-10-11). That list was
written from memory. For each company, find its hiring system and feed link,
and confirm it still hires C++/Qt people.

**17. Career presets, built from what the user gives us.** The C++/Qt list is
just the first preset. Other careers get their own. When the user puts in their
documents, Moonlight reads them, works out the line of work, and goes and finds
companies in that line. This only happens when a brain is attached. Without
one, the user picks a preset or builds a list by hand.

Each preset also carries keywords for matching jobs. For C++/Qt:
strong match is Qt, QML, Qt Quick, C++ with a GUI, embedded UI, HMI. Good match
is C++, real-time, embedded Linux, simulation, telemetry, medical device
software, IEC 62304. "Security clearance required" is flagged, never hidden.
The user can edit every list.

**18. Warn when a posting disappears.** When a job drops out of the employer's
feed, do NOT delete it. Flag it and say so plainly: "This posting seems to be
gone. Check whether it is still listed somewhere else." Then offer to look.
Jobs on the board get this warning too. The user decides what to do with it.

**19. More feeds.** SmartRecruiters has an official public API. Many employer
careers pages carry `JobPosting` data for Google; `PostedJobPageReader` already
reads it from a pasted page, so a company with no hiring system feed can still
be checked from its careers page. Workday is item 3a.

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


**22. Bring in the applications already sent.** Ultra, 2026-10-12. His
"Make Money Now" project kept one folder per application, and the sent ones sit
in `Make Money Now\Job Applications\Application Complete` (14 jobs as of
today). Do NOT read `1 - Send These`: those are not sent.

Each folder name is "Company - Title". Its `HOW TO APPLY.txt` has the link
(the line starting `HOW:`), the pay (`PAY:`) and the place (`WHERE:`). Each job
goes on the board in Applied. The applied date is the folder's date modified.
Ultra's call: close enough, and he can fix any one with the Applied on box.
Shield AI (R5189 Staff Engineer GCS) goes in Closed: two interviews, then
turned down.

Open question: a one-time load into Ultra's board, or an "Import a folder"
button any user can use?

---

## Shipped

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
