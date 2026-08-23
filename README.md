<p align="center">
  <img src="assets/jobcrush-logo.png" alt="Job Crush — AI resumes. Organized search. Better offers." width="720">
</p>

<p align="center">
  <a href="https://github.com/ultramanjones/jobcrush/actions/workflows/build.yml"><img src="https://github.com/ultramanjones/jobcrush/actions/workflows/build.yml/badge.svg" alt="Build status"></a>
</p>

<p align="center"><b>Organize. Personalize. Get Hired.</b></p>

A desktop command center for the job search, in **Qt Quick / C++**. Applications live on a kanban pipeline, documents are prepared and staged before any job portal is opened, and an interchangeable **AIBrain** helps parse postings and draft cover letters — while the human makes every call that matters.

## Why it exists

One night mid-job-search, a fifteen-minute task took three hours — and by the time the application was finally ready, the portal went into maintenance before the resume could be submitted.

Job Crush exists so that never happens again. Everything is prepared, reviewed and staged **first**, and the portal visit becomes the short mechanical last step instead of the whole evening.

## Architecture — V-VM-MV-M

Not MVVM. **V-VM-MV-M** is MVVM's rightful successor: the same idea with the missing layer named and given a home.

```
            View          QML only. Zero business logic. Rip the face off and
              │           everything still works. View objects never talk to
              ▼           each other — navigation is signalled up to Main.

          ViewModel       C++ that knows nothing about QML. Headless and
              │           testable. Translation and organization ONLY.
              ▼           Named for the DATA served, never for the screen.

          ModelView       The whole back end. Faces both directions: wraps
              │           the data core below, hands prepared data up. See
              ▼           below — this layer is the entire point.

            Model         Entities and repositories wrapping SQLite behind
                          interfaces. SQL is never the model itself.
```

The naming carries its own logic: **a ViewModel models the view; a ModelView views the model.** They are mirror images, and they sit next to each other for that reason.

### ModelView is not a "service layer"

This is the part people get wrong, so it is worth being blunt about.

Call this layer a *service layer* and you have described maybe a fifth of it. "Service" implies things that go **out** — network calls, external systems. Most of what lives here never leaves the machine. ModelView is:

- **the data wrapper** — everything above it asks for prepared data and never learns that SQLite exists (swap in a cloud back end and nothing above this layer changes)
- **calculation logic** — ranking, scoring, deduplication, aggregation, anything that computes an answer rather than displaying one
- **orchestration** — coordinating several repositories or several outward calls into one coherent operation
- **policy and rules** — routing order, which credential wins, what counts as the same job across two sources
- **outward integrations** — the HTTP clients, the AI vendors, the job boards (the part that actually *is* a service layer)
- **format translation** — vendor JSON in, domain types out, so no wire format ever leaks upward
- **caching and freshness** — what is still true, and when it is worth asking again
- **long-lived domain state** — a running conversation, a credential roster, a search profile

And here is why the misnomer matters, in practice rather than in theory: if you believe the layer is *for services*, then a calculation "isn't a service call," so it has nowhere to go — and it drifts up into the viewmodel because that is the nearest place with a pulse. Do that for three months and the viewmodels are god objects, the layering is decorative, and nobody can say when it happened.

Name the layer for everything it holds and that drift has nowhere to start. The ViewModel stays thin because there is somewhere better for the work to live.

### The rule is checkable

The dependency arrow points one way only, and the folders are the rule — which makes the architecture verifiable rather than aspirational. In thirty seconds you can confirm that:

- no SQL appears above `src/model/`
- no QML or Qt Quick header appears inside `src/viewmodel/`
- nothing in `src/model/` includes anything from a layer above it
- there is exactly one composition root, and it is `src/main.cpp`

```
src/
├── model/                  entities + repositories; the only place SQL lives
├── modelview/              the back end
│   ├── aibrain/            pluggable AI providers, credential roster, the soul
│   ├── brainchat/          the conversation itself
│   └── jobscout/           job-site clients, search profile, prospect scoring
├── viewmodel/              QAbstractListModel subclasses and small state
└── main.cpp                the composition root — everything wired by hand
*.qml                       the view; hand-written components, no Qt Widgets
```

Dependency injection is constructor injection, by hand, in that one composition root. No container, no framework.

## Reading it

Rather than browsing folders, follow one feature straight down through all four layers:

**Brain Chat** — `BrainChatPage.qml` → `BrainChatConversationViewModel` → `BrainChatSession` → `AiBrain` → `OpenRouterApiProvider`

Ten minutes, and the layering either convinces you or it doesn't.

Two other places worth a look:

- **`src/modelview/aibrain/AiBrainProvider.h`** and **`src/modelview/jobscout/JobSourceProvider.h`** — the same seam solved twice on purpose. Adding an AI vendor or a job board is one class and one catalog line; nothing above either interface changes.
- **`src/modelview/jobscout/ProspectScorer.cpp`** — job ranking as a self-contained deterministic algorithm. Its weights are named constants in a single block, so tuning it means editing numbers rather than hunting through arithmetic.

## What runs today

- **Brain Chat** — streaming responses over SSE, system prompt assembled from user-editable soul files, honest degraded mode with no key configured.
- **Brain selector** — provider tabs plus a checkbox that picks the active brain, verified by a real zero-token call to the vendor. A tick means connected and active and nothing weaker.
- **JobScout** — pluggable job-site clients (Remotive, Arbeitnow; more behind the same interface), per-site tabs, cross-source deduplication, and **Top Prospects** ranked by a local algorithm that costs no API calls and no AI tokens, showing its reasons on every row.
- **Data core** — SQLite behind repositories, with schema that grows by adding columns rather than discarding anyone's data.

Next: the Job Pipelines board, then ProDocs document intake.

## Conventions

Long explicit `camelCase` names — code is written for humans. In-house subsystems get identity names (`AIBrain`, `JobScout`, `ProDocs`, `ModelView`) so they read differently from framework API. Dense logic is commented heavily. No Qt Widgets anywhere: the QML components are hand-written and meant to be read.

## Building

Requires Qt 6.5+ and CMake 3.21+. CI builds on Windows and Linux against Qt 6.8.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or open `CMakeLists.txt` in Qt Creator and hit Run.

## License

MIT — see [LICENSE](LICENSE).
