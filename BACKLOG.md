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

**1. Transcripts produce nothing.** A resume has an EDUCATION heading; a
transcript does not. All three transcripts in testing were stored, read, and
yielded zero entries, while the resume yielded everything — so the schools a
transcript proves are the ones Job Crush cannot see. A document already
classified as a transcript should be read as schooling whether or not it
announces itself with a heading.

**2. Job source coverage.** The source interface and the roster are done;
Remotive and Arbeitnow are wired. Every other free board is a class that
implements `JobSourceProvider` and a row in the roster. No scraping — APIs and
published feeds only.

**3. OAuth for OpenAI (Sign in with ChatGPT).** Launched 2 August 2026 as a
closed beta: six partners, no open enrollment. The flow gets built against the
published shape and sits behind an honest "waiting on OpenAI program access"
state rather than a button that does nothing.

**4. The rest of the brains.** An OpenAI API-key provider (an OpenRouter clone
pointed elsewhere) and Ollama. Ollama needs `providerIsSelectable` to stop
demanding a credential — it runs on the user's own machine and has no key.

**5. Brain levels.** Per-provider model choice, so "which brain" and "how much
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
a page, and Job Crush could only tell them so. Must not spend the user's AI
credits to do it.

---

## Shipped

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

**Adding a job by hand, or by pasting the description in.** For the sites no
API and no automation can reach. Somebody who found a job anywhere at all
should be able to put it on their board and track it like any other — an app
that only tracks the jobs IT found is an app that quietly tells people their
real leads do not count.

**Paid and subscription job sources.** Not v1, not alpha. Later.

**Scraping.** Avoided. Revisited only where it is clearly legal, and only if
the free APIs turn out not to be enough.
