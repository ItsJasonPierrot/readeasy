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

- [ ] **Word-level highlight (karaoke).** Highlight the spoken word, not the
      whole sentence — major dyslexia aid, but `say` doesn't expose word
      timings easily (needs `[[slnc]]`/callbacks or another engine). Big
      rock. **L**

## Distribution & reach

- [ ] ★ **Homebrew tap** (`brew install itsjasonpierrot/tap/readeasy`).
      Handles ncurses automatically — the one command a non-technical Mac user
      can follow. Biggest reach-multiplier. **M**
- [ ] **Native `readeasy file.pdf`.** Auto-run `pdftotext file.pdf -` for a
      `.pdf` argument. (Spun-off task exists.) **S**
- [ ] **`--list-voices`.** Wrap `say -v '?'`. **S**
- [ ] **Linux speech backend.** Abstract `speech.c` to also use `espeak-ng` /
      `spd-say` / Piper. ~doubles the audience. **L**

## Dev hygiene

- [ ] **CI (GitHub Actions).** Build on macOS with `-Werror`; a `debug` target
      with `-fsanitize=address,undefined`. **M**
- [x] **Version from git tags.** (done 2026-09-10) `make` stamps the version
      from `git describe --tags --always --dirty` via `-DREADEASY_VERSION` (on
      `src/main.o` only); falls back to the built-in default outside git.

## Big rocks (later)

- [ ] **Word-level highlight** (above).
- [ ] **In-app paste/read window** — interactive paste-and-read (vs. today's
      `pbpaste | readeasy`). **M–L**

---

## Known caveats (design notes, not bugs)

The codebase is intentionally comment-free; record these here rather than
inline.

- **Signal handler calls `endwin()`** — not strictly async-signal-safe, but the
  common pragmatic tradeoff for restoring the terminal on exit. Deliberate.
- **`wrap_sentence` counts codepoints, not display columns** — double-width CJK
  characters throw the wrap off by a column. Edge case for this audience.
- **Speed changes apply to the next sentence** (one-sentence prefetch), not the
  one currently playing. A `+`/`-` that re-synthesizes the current sentence is
  a possible polish.
- **Word count treats a lone em-dash between spaces as a token**, so counts are
  approximate on punctuation-heavy text.
