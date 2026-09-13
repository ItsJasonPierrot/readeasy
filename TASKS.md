# readeasy — Task List

Last refreshed: 2026-09-10.

Mission: make reading easier for **non-technical, neurodivergent users**
(dyslexia, ADHD, ASD). Priorities are weighted toward accessibility UX and
getting installed without a terminal fight, then reach, hygiene, and big rocks.

Priority: ★ = do next. Effort: **S** ~hours · **M** ~a day or two · **L** ~a
week+.

---

## Done

**Foundation**
- Modular C (`input` / `reflow` / `speech` / `ui`), clean under `-Wall -Wextra`.
- Robustness: terminal restore + child/temp cleanup on signals; re-flow on
  resize; dynamic input buffer (no size cap); piped input reopens `/dev/tty`.
- Gapless sentence-by-sentence playback (`say -o` → `afplay`, ping-pong files).
- Word-boundary wrap (UTF-8 aware); exact pad height (no clipping).
- `make install` / `uninstall` (PREFIX); binary untracked.

**Reading UX**
- Cursor highlight, pause/resume-from-sentence.
- Navigation: arrows, PageUp/PageDown, Home/End, `g`/`G`.
- **Current sentence stays centred while reading** (text scrolls under it).
- Focus/dim mode (`f`, `--focus`).
- Reading column (`-w/--width`, live `[` / `]`) — centred, blank line between
  sentences.
- Live speed `+`/`-` (80–400 wpm).
- Status bar: file, position, %, **word count**, state, speed, key hints.

**CLI & correctness**
- `-h/--help`, `-v/--version`, `-r/--rate`, `-w/--width`, `--voice`,
  `--focus`, `--color` (theme off by default; `--no-color` accepted).
- Validated `--rate` / `--width` with friendly errors.
- UTF-8 fix: `setlocale` before `initscr` (curly quotes, dashes, etc. render).

**Docs**: README + `docs/USAGE.md` + `docs/CONTRIBUTING.md` current.

---

## Next up (★)

- [x] ★ **Theme presets (`--theme NAME`).** (done 2026-09-10) Palettes: none,
      blue, cream (dyslexia dark-on-cream), contrast (white-on-black), dark.
      `--theme NAME`, `t` cycles, `--color` = `--theme blue`.
- [x] ★ **Config file** (`~/.config/readeasy/config`). (done 2026-09-10)
      `key value` lines for rate, width, voice, theme, focus; CLI flags
      override; invalid lines ignored.
- [x] ★ **Unit tests for `reflow.c`.** (done 2026-09-10) `tests/test_reflow.c`
      covers wrapping, sentence splitting, and re-flow; run with `make test`
      (24 cases).

## Accessibility (rest of the mission)

- [x] **Word-level highlight (karaoke).** (done 2026-09-13) Each word lights
      up as it is spoken. `say` exposes no word timings, so the timing is
      distributed across the sentence's words by length, anchored to the true
      clip duration from `afinfo`, and re-synced every sentence. On by default;
      toggle with `w`, or start it off with `--no-word-highlight` /
      `word_highlight off` in the config. Pure layout in `reflow.c`
      (`wrap_words`, unit tested); overlay + timing in `ui.c`.

## Distribution & reach

- [x] ★ **Homebrew tap** (`brew install itsjasonpierrot/tap/readeasy`).
      (done 2026-09-13) Formula lives in the `homebrew-tap` repo, builds from
      the tagged release, and pulls in ncurses automatically — the one command
      a non-technical Mac user can follow.
- [x] **Native `readeasy file.pdf`.** (done 2026-09-13) A `.pdf` argument (any
      case) is converted with `pdftotext file.pdf -`; missing `pdftotext`,
      non-PDF files, and text-less scans each get a clear message.
- [ ] **Linux speech backend.** Abstract `speech.c` to also use `espeak-ng` /
      `spd-say` / Piper. ~doubles the audience. **L**

## Dev hygiene

- [x] **CI (GitHub Actions).** (done 2026-09-13) `.github/workflows/ci.yml`
      builds on macOS with `-Werror` (via `make EXTRA_CFLAGS`) and runs
      `make test` both normally and under `-fsanitize=address,undefined`.
- [x] **Version from git tags.** (done 2026-09-10) `make` stamps the version
      from `git describe --tags --always --dirty` via `-DREADEASY_VERSION` (on
      `src/main.o` only); falls back to the built-in default outside git.

## Big rocks (later)

- [x] **Word-level highlight** (done 2026-09-13, see Accessibility above).

## Not planned

- **In-app paste/read window.** (declined 2026-09-13) An interactive
  paste-and-read mode was considered and dropped — `pbpaste | readeasy` already
  covers it, and a built-in paste UI isn't worth the surface area.

---

## Known caveats (design notes, not bugs)

The codebase is intentionally comment-free; record these here rather than
inline.

- **Signal handler calls `endwin()`** — not strictly async-signal-safe, but the
  common pragmatic tradeoff for restoring the terminal on exit. Deliberate.
- **`wrap_sentence` counts codepoints, not display columns** — double-width CJK
  characters throw the wrap off by a column. Edge case for this audience.
- **Word count treats a lone em-dash between spaces as a token**, so counts are
  approximate on punctuation-heavy text.
- **The word highlight is time-estimated, not from real word boundaries.** `say`
  exposes no word timings, so each word's turn is its share of the sentence's
  audio duration (from `afinfo`), weighted by length. It re-syncs every
  sentence, so any drift is bounded to one sentence; long words spoken slowly
  can still lead or lag slightly. True per-word sync would need a synthesis-API
  helper (`AVSpeechSynthesizer`), which is deliberately out of scope for a small
  C tool.
