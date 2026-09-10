# readeasy — Task List

Last arranged: 2026-09-09.

Mission: make reading easier for **non-technical, neurodivergent users**
(dyslexia, ADHD, ASD). Priorities below are weighted toward accessibility UX
and getting the tool installed without a terminal fight, then reach, hygiene,
and big-rock features.

Priority: ★ = do first. Effort: **S** ~hours · **M** ~a day or two · **L** ~a
week+.

---

## Already done (baseline)

- Modular C: `input` / `reflow` / `speech` / `ui`, builds clean under
  `-Wall -Wextra`.
- Robustness: clean terminal restore + child/temp cleanup on
  SIGINT/SIGTERM/SIGHUP/SIGQUIT; re-flow on terminal resize; dynamic input
  buffer (no size cap); piped input works interactively (reopens `/dev/tty`).
- Reading UX: sentence-by-sentence playback with gapless prefetch
  (`say -o` → `afplay`, ping-pong `a.aiff`/`b.aiff`); word-boundary wrap
  (UTF-8 aware); status bar (file, position, %, state, speed, key hints);
  cursor highlight with pause/resume-from-sentence.
- Navigation: arrows, PageUp/PageDown, Home/End, `g`/`G`.
- Speed: live `+`/`-` (80–400 wpm).
- CLI: `-h/--help`, `-v/--version`, `-r/--rate`, `--voice`, `--no-color`.
- Install: `make install` / `make uninstall` (PREFIX). Binary untracked.
- Docs: README + `docs/USAGE.md` + `docs/CONTRIBUTING.md` current.

---

## 1. Quick fixes (bugs) — done 2026-09-10

- [x] ★ **Fix `.gitignore` inline comments.** Comments moved to their own lines,
      so `.vscode/`, `.idea/`, `*.swp`, `*.swo`, `*~`, `Thumbs.db` are now
      actually ignored.
- [x] ★ **Validate `--rate`.** `strtol` with range check; a non-number or
      out-of-range value prints "--rate must be a number between 80 and 400"
      and exits 1. Rate bounds now shared via `ui.h`.
- [x] **Fix `build_pad` height (was: guard).** Replaced the heuristic with an
      exact row count and switched rendering to explicit per-line positioning
      (`mvwaddnstr` per wrapped line, no layout `\n`). This removes the
      clipping risk entirely and also fixed a pre-existing bug: sentences whose
      wrapped line was exactly the terminal width left a stray blank row and,
      on long files, prevented scrolling to the end.

---

## 2. Accessibility — the mission (highest value)

- [x] ★ **Focus / dim mode.** (done 2026-09-10) Dim every sentence except the
      current one with `A_DIM`; toggle with `f`, or start with `--focus`. The
      dimming follows the cursor and playback and survives resize; the current
      sentence keeps its reverse highlight.
- [ ] ★ **Reading column (`--width`).** Cap line width (~60–70 cols) and centre
      the column, with a blank line between sentences. Full-width lines are
      hard for dyslexic readers (the eye loses the return). Big, cheap win. **M**
- [ ] **Theme presets (`--theme`).** Replace the hardcoded blue/cream with
      3–4 presets, including a cream / low-contrast dyslexia-friendly palette
      and a true high-contrast one. **M**
- [ ] **Config file** (`~/.config/readeasy/config`). Persist rate, voice,
      theme, width so non-technical users set it once instead of retyping
      flags. Core to "simple to use." **M**
- [ ] **Word-level highlight (karaoke).** Highlight the word being spoken, not
      the whole sentence — major dyslexia aid. Hard: `say` doesn't expose word
      timings easily (needs `[[slnc]]`/callback tricks or another engine).
      Big rock — see §5. **L**

---

## 3. Distribution & reach

- [ ] ★ **Homebrew tap** (`brew install itsjasonpierrot/tap/readeasy`).
      Handles the ncurses dependency automatically — the one instruction a
      non-technical Mac user can follow. Biggest reach-multiplier. **M**
- [ ] **Native `readeasy file.pdf`.** Auto-run `pdftotext file.pdf -` when the
      argument ends in `.pdf`, instead of requiring the manual pipe. (Already a
      spun-off task.) **S**
- [ ] **`--list-voices`.** Wrap `say -v '?'` so people discover voices without
      leaving the tool. **S**
- [ ] **Linux speech backend.** macOS-only today (`say`/`afplay`). Abstract the
      speech layer to also use `espeak-ng` / `spd-say` / Piper. `speech.c` is
      already isolated enough to make this clean. ~doubles the audience. **L**

---

## 4. Dev hygiene

- [ ] ★ **Unit tests for `reflow.c`.** `build_sentences` and `wrap_sentence`
      are pure functions with fiddly logic — exactly what breaks silently on a
      refactor. Small harness: feed input, assert output. **S**
- [ ] **CI (GitHub Actions).** Build on macOS with `-Werror`; add a `debug`
      make target with `-fsanitize=address,undefined`. **M**
- [ ] **Version from git tags.** Replace the hardcoded `"1.0.0"` (already behind
      `-DREADEASY_VERSION`) with the tag at build time, and cut tagged binary
      releases. **S**

---

## 5. Big rocks (later)

- [ ] **Word-level "karaoke" highlight** (from §2) — the feature people would
      talk about, but a real project. Do last. **L**
- [ ] **In-app paste/read window** — interactive "paste text and read" mode
      (vs. today's `pbpaste | readeasy`). **M–L**

---

## Known caveats (document, not necessarily fix)

The codebase is intentionally comment-free, so record these as design notes
(e.g. in `docs/CONTRIBUTING.md`) rather than inline comments:

- **Signal handler calls `endwin()`**, which is not strictly async-signal-safe
  — the common pragmatic tradeoff for restoring the terminal on exit. Note it
  is deliberate.
- **`wrap_sentence` counts codepoints, not display columns**, so double-width
  CJK characters throw the wrap math off by a column. Edge case for this
  audience.
- **Speed changes apply to the next sentence**, not the one currently playing,
  because of the one-sentence prefetch. A `+`/`-` that re-synthesizes the
  current sentence is a possible polish.
