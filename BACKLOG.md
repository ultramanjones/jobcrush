# Job Crush — Backlog

What is queued, what is deliberately parked, and why. Kept current as work
lands: an item moves out of this file when it ships, and anything new goes in
the moment it is asked for rather than the moment somebody gets round to it.

A backlog nobody updates is a lie with a filename.

---

## Next up

**1. Job source coverage.** The source interface and the roster are done;
Remotive and Arbeitnow are wired. Every other free board is a class that
implements `JobSourceProvider` and a row in the roster. No scraping — APIs and
published feeds only.

**2. OAuth for OpenAI (Sign in with ChatGPT).** Launched 2 August 2026 as a
closed beta: six partners, no open enrollment. The flow gets built against the
published shape and sits behind an honest "waiting on OpenAI program access"
state rather than a button that does nothing.

**3. The rest of the brains.** An OpenAI API-key provider (an OpenRouter clone
pointed elsewhere) and Ollama. Ollama needs `providerIsSelectable` to stop
demanding a credential — it runs on the user's own machine and has no key.

**4. Brain levels.** Per-provider model choice, so "which brain" and "how much
brain" are separate questions. Replaces the hardcoded model constant in each
provider. The naming is the hard part: model version numbers mean nothing to
somebody who just wants their cover letter to sound like them.

**5. "Moonlight clean this up!"** The button on ProDocs that hands a messy
parse to the connected brain. Label above it reads "If you have connected an
AI brain you can have…", and with no brain connected it greys out *and says
why*.

**6. An app-owned soul capabilities file** that refreshes on every launch, so
what Job Crush can DO stays current while the user's own edits to `soul.txt`
stay theirs. Today a stale soul file means a brain that describes an app that
no longer exists.

**7. Job Pipelines (Phase 4).** The board. Drag and drop between stages.

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

**Paid and subscription job sources.** Not v1, not alpha. Later.

**Scraping.** Avoided. Revisited only where it is clearly legal, and only if
the free APIs turn out not to be enough.
