# AGENTS.md

hmw-workbench: a lightweight Windows tool for porting content to HMW (H1 / MWR). It loads zonetool dumps, shows
weapons on viewhands, plays animations, mounts HMW attachments, edits tags so attachments fit, previews camos.
The full plan, facts and milestones are in [docs/PLAN.md](docs/PLAN.md). Read it before doing anything.

## Workflow
- **Claude** plans, designs, reviews and orchestrates. It runs in a Linux VM, cannot build with MSVC or run the exe.
- **opencode (you)** implements, builds and tests on Windows.
- Work **one milestone at a time**. At the end of a milestone:
  1. build clean with `tools\build\build.bat`,
  2. run the review commands of that milestone (`-screenshot`, `-info`) into `review\M<n>\`,
  3. write `docs/milestones/M<n>.md`: what was done, how it was tested, exact commands, open issues, deviations,
  4. commit `M<n>: <summary>`,
  5. **stop and wait**. Don't start the next milestone until told.
- If something in the plan is wrong or blocks you, stop and report it in the milestone file. Don't silently work around it.

## Paths
- Repo: `C:\Games\cod-hwm\hmw-workbench`
- `GAME` = `C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered`
- Crossbow port dump: `GAME\zonetool\mod`; HMW dumps: `C:\Games\cod-hwm\refs\dumps\*`
- H1 struct reference (read-only): `C:\Games\cod-hwm\cod-mwx\src\client\game\database.hpp`
- zonetool: `GAME\zonetool.exe`, build in `C:\Games\cod-hwm\refs\tools\x64-zt\release`

## Rules
- Write only inside `C:\Games\cod-hwm\hmw-workbench`. Everything else is read-only.
- **Never modify `C:\Games\cod-hwm\cod-mwx`**: other agents work there. Don't build it, don't run its scripts.
- Never change game files, `.ff`, `.pak`, zones or existing dumps. Exceptions: a new dump folder under `GAME\dump\`
  (M2), and `.xmb` saves in M5 with a `.bak` copy first.
- Keep at least 20 GB free on C:. Never copy or extract outside `C:\Games\cod-hwm`.
- Dependencies only as git submodules from their official repositories.
- C++20, MSVC warnings level 4, no warnings in our code. Keep asset readers free of D3D code (`src/assets`).
