# readeasy — Task List

Last refreshed: 2026-09-13.

Mission: make reading easier for **non-technical, neurodivergent users**
(dyslexia, ADHD, ASD). Priorities are weighted toward accessibility UX and
getting installed without a terminal fight, then reach, hygiene, and big rocks.

Priority: ★ = do next. Effort: **S** ~hours · **M** ~a day or two · **L** ~a
week+.

---

## V2 roadmap

Started 2026-09-13, after the v1.2.0 release. Bigger features that make
readeasy usable without ever learning a flag or editing a file, and widen
where it runs.

- [x] ★ **In-app settings menu (`,`).** (done 2026-09-13) A modal menu changes
      theme, speed, width, focus, word highlight, and voice — the first four
      apply live behind the menu; Voice opens a scrollable picker of every
      installed voice. `s` saves to the config file, so a non-technical user
      never edits a file or memorizes a flag. New `config.c` (load + save) and
      `speech.c` voice listing back it.
- [x] ★ **Better reflow (join sentences, clean man/PDF input).** (done
      2026-09-14) Three fixes, all mission-critical to reading quality:
  1. **Rejoin by sentence, not line length.** `build_sentences` (reflow.c) no
     longer flushes on a `maxw * 3/5` line-length threshold (which a single long
     line — a URL, a wide row — inflated, splitting ordinary lines mid-sentence).
     It now joins hard-wrapped lines and lets sentence punctuation split them,
     de-hyphenates words broken across a line end (`inter-\nnational`), and keeps
     ALL-CAPS lines (man section headers) as their own units.
  2. **Man-page overstrike** (`input.c` `collapse_overstrike`). `c\bc`/`_\bc`
     backspace overstrike is collapsed, so `man x | readeasy` reads cleanly with
     no `col -b` and no "N NA AM ME E".
  3. **PDF page furniture** (`input.c` `strip_pdf_furniture`). Uses `pdftotext`'s
     form-feed page breaks (before the sanitiser flattens them) to drop running
     headers/footers that recur on most pages and bare page-number lines.

      Covered by 10 new unit tests (43 total, pass under ASan/UBSan); verified
      `collapse_overstrike` matches `col -b` and furniture-stripping on a real
      3-page PDF.
- [x] **Navigation.** (done 2026-09-14)
  - [x] **Search (`/`).** Type a phrase in the status bar; `Enter` jumps the
        cursor to the next sentence containing it (case-insensitive, wraps);
        `n` / `N` repeat forward / backward; `Esc` cancels.
  - [x] **Outline / jump by heading (`o`).** Pops a scrollable list of the
        document's headings (ALL-CAPS lines and short non-sentence lines);
        `Enter` jumps to that section. Reuses `list_picker`.
  - [x] **Help overlay (`?`).** A centred, bordered card lists every key; any
        key closes it. Reuses the modal-window pattern.
- [x] **Linux support.** (done 2026-09-14) `speech.c` now selects a backend at
      build time (`#ifdef __APPLE__`): Linux synthesizes with `espeak-ng -w`,
      plays with `aplay`, reads clip duration by parsing the WAV header (no
      external tool), and lists voices from `espeak-ng --voices`; `AUDIO_EXT`
      (`speech.h`) picks the file extension. A Linux CI job builds with `-Werror`
      and runs the unit tests (incl. ASan/UBSan) on `ubuntu-latest`. *Compile +
      unit tests are green in CI; live speech on a real Linux box still wants a
      hands-on check.* Windows (SAPI/PowerShell) is a future `#elif`.

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
- [x] **Pull modal widgets out of `ui.c`.** (done 2026-09-14) The overlay
      widgets (settings menu draw, list picker, help card, search prompt) moved
      to `src/widgets.c`, taking `color_pair` as a parameter so they hold no
      shared state. `ui.c` 1238 → ~1070 lines.
- [ ] **Consider splitting `run_ui`.** It is still ~700 lines (the main loop +
      every key handler). Splitting it would mean threading ~15 locals through a
      context struct — real regression risk on the hot path, so deferred until it
      actually gets in the way. **M**

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
