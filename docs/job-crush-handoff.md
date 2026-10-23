# Job Crush Handoff — Direct Employer Feeds, Niche Defaults, and a Website

From a Make Money Now chat, Oct 11, 2026. Matt wants the Job Crush project to do all of this.

## Why this change

Job Crush found plenty of general jobs but almost nothing in Matt's niche (C++/Qt, medical device, defense, simulation). General job boards bury those jobs. Many boards also lock listings behind paywalls or block automated access.

Employers do the opposite. They want their jobs found. Most companies post jobs through a hiring system (Greenhouse, Lever, Workday, and others), and many of those systems publish the jobs in a public, machine-readable feed.

New approach: keep a list of companies that hire Qt and C++ people, and check each company's own job feed directly. Skip the middleman boards.

## Decisions Matt made

1. Job Crush gets a website version in addition to the desktop app.
2. Default focus: tech and medical tech jobs, with C++/Qt as a built-in preset.
3. Users can change the settings to search any kind of job. The niche is a default, not a limit.
4. The website stays free and links to the original employer posting. No paywall on job listings.

## Where the jobs come from

### Use these (public feeds meant for this)

| Hiring system | How to get a company's jobs | Notes |
|---|---|---|
| Greenhouse | `https://boards-api.greenhouse.io/v1/boards/{company}/jobs?content=true` | Official public API. `{company}` is the name in their careers page link. |
| Lever | `https://api.lever.co/v0/postings/{company}?mode=json` | Official public API. |
| Ashby | `https://api.ashbyhq.com/posting-api/job-board/{company}` | Official public API. Common with newer startups. |
| SmartRecruiters | `https://api.smartrecruiters.com/v1/companies/{company}/postings` | Official public API. |
| Workday | Each company's `*.myworkdayjobs.com` careers site | No official public API. The careers pages load jobs from a JSON request the browser makes. Usable, but it can change without warning. Check each site's terms. Many big medical and defense companies use Workday. |
| Company careers pages | Look for `JobPosting` structured data in the page | Many employers tag jobs this way so Google can read them. |

### Do NOT collect from these

LinkedIn, Indeed, Glassdoor, ZipRecruiter, and paywall boards like FlexJobs and RemoteJobs.io. Their terms ban automated collection. LinkedIn won a breach-of-contract claim against hiQ over scraping, even though the court said scraping public pages was not hacking.

## Rules for showing jobs

- Job title, company, location, and pay are facts. Show them freely.
- Do not copy the full job description. Write a short summary in our own words, or show only key facts (skills, years, clearance, remote or onsite).
- Always link to the original employer posting. That is where users apply.
- Record the date we found the job. Re-check feeds often and drop jobs that disappear.
- Matt is not a lawyer. Before charging money for anything, get one hour with a lawyer on the terms-of-use question.

## Company starter list

These are companies known to hire C++ and Qt developers. This list is a starting point from memory, not verified. For each one, the Job Crush project should find which hiring system it uses and the feed link, and confirm it still hires for C++/Qt.

**Medical devices and surgical robotics**
Stryker, Intuitive Surgical, Medtronic, GE HealthCare, Philips (including Philips Respironics, Pittsburgh), Siemens Healthineers, Boston Scientific, Zimmer Biomet, Smith+Nephew, Abbott, Baxter

**Defense, aerospace, and simulation**
L3Harris, CAE, Lockheed Martin, Northrop Grumman, General Dynamics Mission Systems, Collins Aerospace, BAE Systems, Leidos, Textron Systems, Kratos, AeroVironment, Shield AI, Anduril

**Automotive and in-car screens**
GM, Ford, Rivian, Lucid, Harman, Bosch, Continental, Visteon, Garmin

**Test and measurement**
Keysight, NI (Emerson), Tektronix, Rohde & Schwarz

**Qt specialists and software with Qt front ends**
The Qt Company, KDAB, ICS (Integrated Computer Solutions), Autodesk, Ansys (Canonsburg, PA), Bentley Systems (Exton, PA)

**Pittsburgh-area robotics and autonomy**
Aurora Innovation, Motional, Carnegie Robotics, Near Earth Autonomy, Sarcos

Store the list as data (a file or table), not in code, so Matt or a user can add and remove companies. Each entry: company name, sector, hiring system, feed link, last checked date.

## Matching the niche

Feeds return every job a company has. Filter for the niche with keywords in the title and description:

- Strong match: Qt, QML, Qt Quick, C++ with GUI, embedded UI, HMI (human-machine interface)
- Good match: C++, real-time, embedded Linux, simulation, telemetry, medical device software, IEC 62304 (the medical device software standard)
- Flag but don't hide: security clearance required

Let users edit the keyword lists. Ship the C++/Qt preset and a general tech preset as defaults.

## The website

Two ways to build it:

1. **Qt for WebAssembly.** This is the "weird layer" Matt used once to put a Qt desktop app online. It compiles the Qt app with a tool called Emscripten into a `.wasm` file that runs inside the browser. It works, but it supports fewer Qt modules than desktop, the first load is large, and search engines can't read the page content because it draws everything into one canvas.
2. **A normal web page front end** that reads from the same job data as the desktop app.

**Recommendation:** use a normal web front end for the public site. A job site needs Google to find its pages, needs to load fast on phones, and needs plain links. Keep the Qt WebAssembly build as an optional "try the desktop app in your browser" demo. That demo is a good portfolio piece in itself.

Both the desktop app and the website should read from one shared job collector, so the collection work is written once.

## Web architecture using Matt's VVMLM layers

Matt's layers: **View, ViewModel, Logic, Model.**

- View: the screen.
- ViewModel: a translator only. Arranges data for the View. Keeps nothing.
- Logic: all calculations and every outside connection. The only layer that talks to the Model.
- Model: raw data. No calculations.

Because the ViewModel keeps nothing, the web version does not need a live copy of the app per visitor. Each request goes through the layers and is done.

**Where each layer runs:**

- Browser: the View, written in plain HTML and JavaScript. It keeps track of what the user is looking at right now (filters, current page).
- Server, in C++/Qt: ViewModel, Logic, and Model. Reuse the desktop app's C++ layers where possible.
- The browser sends a request. The server's ViewModel takes it, Logic does the work, Model holds the data. The answer goes back as JSON.

**Two ways the browser talks to the server:**

- Plain requests (Qt HTTP Server, in Qt 6.4 and later) for almost everything: searching jobs, loading saved jobs, saving a change. The server answers and forgets.
- Qt WebChannel only where something must update live while the user watches, such as a new job appearing. WebChannel exposes Qt objects (properties, signals, slots) to JavaScript over an open connection. Each open connection costs server memory, so use it sparingly.

## Users, saved data, and handling many users

**Each user's data:** one shared database. Every saved search, tracked job, and pipeline entry carries the owner's user ID. The Logic layer only ever reads or writes rows for the user making the request. This is the web version of the Model layer.

**Database:** start with SQLite while building. Move to PostgreSQL when real users arrive. Qt SQL supports both.

**Knowing who is asking:** when a user logs in, the server gives the browser a pass (a long random token). The browser sends it with every request. Logic checks it and uses it to find the user ID.

**More users:** the server keeps nothing between requests, so you can run several identical copies of it. A load balancer (a traffic director in front of the copies) sends each request to any free copy. All copies share the one database. One database handles thousands of users fine.

**Not one stack per user.** One program and one database serve everyone.

**Accounts and logins:** do not build password storage by hand. Use an existing login service (for example Auth0, Clerk, Supabase Auth, or log in with Google or GitHub). A mistake there leaks people's data.

## Risks and open questions

Matt has built desktop and air-gapped defense software, not public web servers. These are the places most likely to cause trouble:

1. **"Qt on the web" means two different things here.** The plan above runs Qt/C++ on the server. The page in the browser is ordinary HTML. Qt for WebAssembly, which runs Qt inside the browser, is the separate demo idea.
2. **Qt HTTP Server is young and basic** compared with common web server tools. Test it early under load before building a lot on it. If it falls short, keep the Logic and Model layers in C++ and put a common web server in front.
3. **Licenses.** Check the license of each Qt module used on the server (Qt HTTP Server, Qt WebChannel, Qt SQL) against the MIT license of the Job Crush repo, and check Qt for WebAssembly's terms for the demo.
4. **Thread safety.** A web server answers several requests at the same time. Matt's one-ViewModel-per-instance rule avoids shared state in the ViewModel, but Logic and the database connection must be safe when two requests arrive together. Give each request its own database connection.
5. **Security.** A public server gets attacked. Check every input from the browser. Use parameterized database queries, never text glued into SQL. Serve only over HTTPS.
6. **Hosting.** A C++ server needs a Linux host or container. Plan how it is built, deployed, and restarted if it crashes.
7. **Backups.** User data needs regular database backups.

## Suggested order of work

1. Build the company list as a data file with the fields above.
2. Write a collector for Greenhouse and Lever first. They are the easiest and fully official.
3. Add Ashby and SmartRecruiters.
4. Add the keyword filter with the C++/Qt and general tech presets.
5. Hook the collector into the desktop app.
6. Look up the hiring system for each company on the starter list. Add Workday companies last.
7. Build a small test server with Qt HTTP Server that answers one job search as JSON through the VVMLM layers. Load-test it before going further.
8. Add the database with user IDs, then logins through an existing login service.
9. Build the public website front end (plain HTML and JavaScript) on that server.
10. Optional: a Qt WebAssembly build of the desktop app as a browser demo.
