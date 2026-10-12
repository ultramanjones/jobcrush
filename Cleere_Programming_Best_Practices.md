# Cleere Programming Best Practices

House law for any project, in any language, on any framework. It comes
from the Author's real professional best-practices work, not from
improvisation. Every piece of code and every review answers to this
document.

----

## 1. The architecture: V-VM-L-M

**View → ViewModel → Logic → Model.**

The program is built in four layers, stacked top to bottom. The View is
the top layer, closest to human eyeballs. The Model is the bottom layer,
close to the metal. Every dependency points downward: each layer uses
the layer below it, and no layer knows anything about the layers above
it.

V-VM-L-M is the rightful successor to MVVM. It is MVVM with the missing
layer named and given a home. MVVM tells you where the view goes, where
the viewmodel goes, and where the data goes — but it says nothing about
where the rest of the program lives. That gap has a name here: the Logic layer.

The stack reads top-down, but it is easiest to learn bottom-up, because
each layer exists to serve the one above it. So this tour starts at the
bottom.

### Model (data core)

The model layer owns the stored data, and it owns everything about HOW
that data is stored. The database connection, the table schema, and
every single query live in this layer, and nowhere else. The layers
above ask this layer for data. They never learn how the data is kept.

Three kinds of thing live here: the database class, data records, and
repositories.

**The database class** opens the connection and creates the tables. It
is the one place that knows the database as a whole.

**A data record** is a plain data type that describes one thing the
program cares about. A job posting record has one field for each fact
about a posting: the company name, the position title, the link. A data
record is only the shape of the data. It has no functions, no queries,
and no knowledge of anything above it.

Data records are the cargo of the whole program. They are created down
here, and they travel upward through every layer — a viewmodel holding a
list of posting records is normal and correct. What must never travel
upward is the machinery: the queries, the connection, the table names.
Those stay locked in this layer.

**A repository** is a class that saves and loads one kind of data
record. The posting repository is the only place in the whole program
that runs queries against the postings table. A layer above calls a
method like `loadAllDiscoveredJobPostings()` and gets back a plain list
of posting records, ready to use. The repository's interface is declared
here in the model. The layers above hold a reference to a repository and
call its methods.

This is what "the database is never the model itself" means. The model
is the data records plus the repositories. The database is a storage
detail hidden inside the repositories. Swap the local database for a
cloud service, and only the repository internals change — every caller
keeps working, unchanged. The full price of this design is small: one
plain data type and one row-to-record converter function per table.

### Logic layer — the whole back end

The Logic layer sits directly on top of the model, and it is the whole
back end of the program. It calls repository methods and receives data
records. It never sees a query, and it never learns which database
answered.

Here is the dividing line between these two layers, and it is worth
memorizing: **the model knows HOW the data is stored. The Logic layer
knows WHAT the data MEANS.** Storing is model work: the connection, the
tables, the queries. Meaning is Logic layer work: scoring a posting,
spotting a duplicate, deciding what has gone stale.

**The Logic layer is NOT just a service layer.** The word "service" describes
maybe a fifth of it, and it implies things that go OUT. Most of this
layer never leaves the machine. The Logic layer holds all of the following:

1. It wraps the model below it, as described above. Nothing above this
   layer ever learns which database is in use.
2. It holds the calculation logic: ranking, scoring, duplicate
   detection, totals — anything that computes an answer rather than
   displays one.
3. It orchestrates. When one operation needs several repositories or
   several outside calls, this layer composes them into one operation.
4. It holds policy and rules: which route is tried first, which
   credential wins, what counts as a duplicate.
5. It holds the outward integrations: HTTP clients and third-party
   APIs. This is the one part that really is a service layer.
6. It translates formats. Vendor JSON comes in, and the program's own
   types go out. No wire format ever leaks upward.
7. It handles caching and freshness. It decides what is still true and
   when to ask again.
8. It keeps long-lived domain state: a running session, a credential
   roster, a user profile — state that outlives any one screen.

Why the name matters: calling this a "service layer" causes a
specific, predictable failure. If the layer is "for services," then a calculation is not a
service call, so it has nowhere to go — and it drifts up into the
viewmodel, because that is the nearest place with a pulse. Three months
of that and the viewmodels are god objects — bloated classes that do
everything — and the layering is decorative. Naming the layer for 
everything it holds leaves the drift nowhere to start.

**Which layer talks to the cloud?** Ask whose data it is. If the app's
OWN data lives in the cloud — the same records that sit in the local
database today — then connecting to that cloud is model work. The
repository keeps the same face, and only its insides change from local
queries to network calls. If the cloud belongs to someone else — an
outside service answering questions — that is not storage of our data.
That is the outside world, and talking to it is Logic layer work. The
pocket test: code whose job is "save and load our data records" belongs
in the model, no matter what wire it uses. Code whose job is "go ask
the world something" belongs in the Logic layer.

### ViewModel

The ViewModel sits above the Logic layer and directly below the view. Its
job is to take prepared data from the Logic layer and organize it into
exactly the form the view wants to display. That is the whole job:
translation and organization only. If a piece of code computes
something, it does not belong here — it belongs down in the Logic layer.

A viewmodel knows nothing about the UI framework. It is headless, which
means it can run and be unit-tested with no UI attached at all. A
viewmodel is named for the data it serves, never for a diagram word or
a screen name. `SessionJobListViewModel` is a good name.
`AboutPageViewModel` is not.

**Names are not entanglement. Bindings are.** If a viewmodel's wording
drifts toward the screen it serves, that is not the crime. The crime is
a reference pointing the wrong way. The view binds down to the
viewmodel — that is required, and it is the whole mechanism. The
viewmodel never binds, connects, or refers up to the view. Not a
reference, not a callback, not an event subscription. Keep that arrow
one-way and the view stays removable, and removable is the test that
matters.

**One page, several viewmodels.** A page usually has sections, and each
section gets its own viewmodel. That is the healthy arrangement. Keep
those viewmodels as separate classes, side by side, and let the page
bind each section to its own viewmodel. Never nest viewmodels inside
other viewmodels to bundle a page together. When one page's viewmodels
multiply, you may group them in a subfolder under the viewmodel folder
so they are easy to find — but do not name the viewmodel files to match
the view.

**The shortcut this architecture forbids.** Most frameworks will
happily let a viewmodel query the database directly, and it works. Do
not do it. The moment a viewmodel runs a query, it knows the database
exists, so swapping storage now means rewriting viewmodels. It also
needs a real database just to be tested. And the next calculation finds
a cozy home right next to that query, which is how the drift into god
objects begins. The layered version costs about twenty extra lines per
table. Pay it.

### View

The view is the top layer. It is everything the person sees and
touches, and it holds zero business logic — no rules, no calculations,
no decisions. The view only shows what it is given and passes along
what the person did. The view must be fully replaceable: you can rip the face off the
app and everything underneath still works. View objects never talk to
each other. When a view wants to navigate somewhere, it signals that
wish up to the top-level view, and the top-level view switches the page.
Views never reach sideways.

### Straight-line logic

Logic flows straight up and down. Calculations belong down in the
Logic layer and the data core. There are no sideways shortcuts at any
layer.

### Composition root

The application entry point builds everything by hand, in order: first
the database, then the repositories, then the Logic layer classes, then the
viewmodels, then the UI. This happens in exactly one place. There is no
dependency-injection framework and no container. A human wires the
constructors together, and that is the whole system.

### Project structure

The folder tree states the architecture. Each layer gets its own folder:

    view/
    viewmodel/
    logic/
    model/

A newcomer learns the architecture by looking at the directory, before
opening a single file. The invariants below are folder rules, so the
folders must exist for the invariants to be checkable.

View gets a folder too. It is the top layer and it can feel like the
project itself, but the face is supposed to be replaceable — and a face
that lives in one folder is a face you can actually rip off. The
composition root sits beside the four folders, inside none of them.

### The checkable invariants

Claims cost nothing. Verifiable claims are what convince engineers. Each
of these can be checked mechanically, and every review checks them:

- No database query exists above the model layer.
- No UI-framework import appears inside the viewmodel layer.
- Nothing in the model layer includes anything from the layers above it.
- The project has exactly one composition root.

---

## 2. Naming

Naming is one of the most important aspects of coding. All the rest is
plumbing.

Names are long, explicit, and descriptive — compound German words built
from plain English. Write `stagedApplicationPacket`, never `sap`. Names
are always camelCase, never snake_case. The one exception is Python: 
in Python, use snake_case for functions and variables, to match the 
language’s own style. (Python does not enforce snake_case, but some 
common code checkers, like Pylint, will flag camelCase. Having AI use 
snake_case in Python saves those headaches. I guess they figured that a
Python is a snake thus snake_case.)

**The compound word is the method, not the goal. OBVIOUS is the goal.**
A long name can still say nothing:
`documentTextExtractionResultHandlerConfiguration` refuses to abbreviate
and is still unclear. The test is simple: a reader can say what the thing
IS after reading the name once. Length is what you get from refusing to
abbreviate. It is not a target to hit.

In-house creations get distinctive identity names that separate them from
framework and library APIs. A `WindowCommander` is unmistakably yours. A
`WindowManager` could be anyone's.

---

## 3. Readability

**Code is written for programmers, not for the machine. If we were
writing for the machine, we would write in binary. Always remember that
you are writing code for human eyes and human understanding.**

Readability is the top priority. Clarity outranks efficiency, always,
because without clarity there is no efficiency of communication. A
micro-optimization that hurts readability is rejected. Optimize only
proven hot spots.

If you absolutely must make a portion of code intricate and difficult to
understand at a glance, such as a complex mathematical calculation, a shader, or
something like them, then you MUST attach a comment section that clearly
describes every jot and tittle of the underlying algorithm. No exceptions.
If you edit a line of code you must check the above comment and make sure that
your changes are reflected in the comment.

Do not take clever shortcuts. Terse-clever code is pooping on your
project.

SOLID principles apply. Dependency injection is used sparingly: it means
constructor injection, wired by hand, in the one composition root, and
nothing more.

Do not write spaghettios code. Separation of concerns does not mean
confetti. Size each class to its responsibility instead of shattering
everything into tiny fragments.

---

## 4. Plain language in writing

Commit messages, code comments, specs, and reports get short, plain
English. Say what you mean. Write like Hemingway.

The writer thinks about a sentence once. Every reader after that pays the
cost again, in full — including the writer's future self, who is a
stranger to the code by then. A reader who fails twice does not give up.
They build a guess about what the sentence means and write new code on
top of the guess, and the confusion multiplies into everything
downstream.

Use short sentences, with one idea in each. Use the normal word: say
"fixed," not "healed." If a sentence sounds like writing, cut it and say
the plain version. Use a metaphor only when it makes something clearer
AND everyone already knows it.

---

## 5. Git workflow

**If it builds and runs, push it. Period.**

There are no qualifications. Not "once I like it." Not "after I finish
the screen." The only gate is that the program builds and runs after your
changes.

Always build and run BEFORE you commit and push. Never commit unverified
code. Push early and often. Every delivered increment should BE a
pushable green state.

---

## 6. UI feedback

**Never use a spinner. Ever.** A waiting state must say something true:
real progress with real numbers, skeleton content, streamed text, or an
animation that shows the actual work being done.

The app never looks frozen. Startup is near-instant. Settings apply
instantly, with no Apply/OK ceremony.

**Guardrails advise, never decide.** The app looks out for the user, but
it never makes choices for the user. Warn about a likely mistake, but
never block the action, and never pop up modal ceremony.

**Stupid-proofing means not making the user FEEL stupid.** You cannot
cure stupid, and that was never the real job of stupid-proofing.
When the app shows an error or a warning, the message states facts about 
the situation — "that looks like a Gemini key" — and never passes a verdict 
on the person. When the user makes a mistake, the app helps them recover 
from it and never punishes them for it. For instance, the app keeps what 
they typed instead of erasing it, points out what looks wrong, offers the 
fix, and lets them carry on.

**Never give a vague rejection.** Every message the app shows says
plainly what happened, says why when the why is actionable, and ends with
a next step the person can actually take — the specific thing that works,
not "try again."

**When all else fails, be helpful.** A limit is never announced without a
button that gets past it, when such a button exists.

---

## 7. Error handling and logging

**Error messages are clear and exact.** That rule covers the log as much
as the screen.

The code detects errors itself. Every operation that can fail is checked
right where it happens, and a failure is logged at that spot. A failure
is never swallowed, and never left to surface three layers later as a
mystery.

Every error log line answers three questions: what went wrong, where in
the code it happened, and which object ran into the trouble. This line
answers all three:

    SettingsFileReader::loadSettingsFile: could not open
    "preferences.ini" — the file does not exist

A log written this way is a map. The reader walks straight to the
failing method and the exact object that hit trouble. A log line that
only says something failed leaves the reader searching the whole
program.
