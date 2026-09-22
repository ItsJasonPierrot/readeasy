# Contributing to readeasy

Thank you for your interest in improving `readeasy`. This guide explains how to
set up the project, build it, follow the code style, and submit changes.

By taking part, you agree to follow our
[Code of Conduct](CODE_OF_CONDUCT.md).

---

## Project layout

```
readeasy/
├── src/            C source files
│   ├── main.c      Program entry point and argument handling
│   ├── config.c    Reads and writes the ~/.config/readeasy/config file
│   ├── input.c     Reads file/PDF/pipe; de-overstrikes, strips PDF furniture
│   ├── reflow.c    Joins hard-wrapped lines into sentences; wraps/maps words
│   ├── speech.c    Speech backends: macOS say / Linux espeak-ng; voices
│   ├── ui.c        Terminal interface, keyboard controls, playback loop
│   ├── widgets.c   Modal overlays: settings menu, list picker, help, prompts
│   └── places.c    Saves reading position + bookmarks per file (the places file)
├── include/        Header files (.h) for each source module
├── docs/           Documentation
├── tests/          Unit tests (test_reflow.c), run with `make test`
├── Makefile        Build instructions
└── README.md       Overview and quick start
```

Each `.c` file in `src/` has a matching `.h` file in `include/` that declares
its public functions.

`ui.c` keeps the next sentence's audio prepared while the current one plays:
`speech.c` writes each sentence to a temporary file with `say -o` and plays it
with `afplay`, so playback moves between sentences without a gap.

---

## Setting up

You need:

- A C compiler — `cc`, `gcc`, or `clang`.
- The **wide-character ncurses** development library (`ncursesw`), not the
  narrow/plain build. The narrow build renders multi-byte UTF-8 text (e.g.
  accented characters pulled from a PDF) as garbage instead of the correct
  character.
- The **say** and **afplay** commands for speech and audio playback (both
  built into macOS).

On **macOS**, `say` and `afplay` are built in, but the system `ncurses` is the
narrow build. Install the wide-character version first:

```bash
brew install ncurses
```

On **Linux**, install ncurses plus the speech tools `readeasy` uses there
(`espeak-ng` to synthesize, `aplay` from `alsa-utils` to play):

```bash
sudo apt install build-essential libncursesw5-dev espeak-ng alsa-utils
```

`espeak-ng` and `aplay` are only needed at run time; the program builds with
just a compiler and `ncursesw`.

---

## Building

Build the program with:

```bash
make
```

This compiles every file in `src/` and links them with ncurses to produce the
`readeasy` executable. The `Makefile` looks for `ncursesw6-config` (checking
Homebrew's keg-only install path on macOS too) and uses it to link against
the wide-character library; if it can't find that tool it falls back to
plain `-lncursesw`.

The compiled `readeasy` binary is **not** committed to the repository (it is
in `.gitignore`) — always build it from source with `make`.

To install it on your `PATH` (`/usr/local/bin` by default), or remove it:

```bash
sudo make install                    # or: make install PREFIX="$HOME/.local"
sudo make uninstall
```

To remove the executable and the compiled object files:

```bash
make clean
```

The version reported by `readeasy --version` is taken from
`git describe --tags` at build time, so tag a release (e.g. `v1.1.0`) and
run `make clean && make` to stamp it in. Outside a git checkout it falls back
to the built-in default.

---

## Testing

The pure text logic in `reflow.c` (`build_sentences`, `wrap_sentence`,
`wrap_words`) has unit tests. Run them with:

```bash
make test
```

They cover word-boundary wrapping, sentence splitting, re-flowing of
hard-wrapped text, and the per-word layout (`wrap_words`) used by the word
highlight. Add a case there when you change that logic.

The rest of `readeasy` is tested by hand. After making a change, rebuild and
try a few cases (use any plain-text file, ideally one exported from a PDF so
it has
accented characters and hard-wrapped lines):

```bash
make
./readeasy somefile.txt              # a normal file
./readeasy somefile.pdf              # a PDF (converted with pdftotext)
printf '' > empty.txt; ./readeasy empty.txt   # should say "File is empty"
./readeasy /no/such/file             # should say "File not found."
echo "hello world" | ./readeasy      # piped input
./readeasy a b                       # two args: should show usage error
```

Check that:

- The text fills the window width, including accented/non-ASCII characters —
  no garbled symbols, no narrow column left over from a hard-wrapped file, and
  no word split across two lines.
- A very large file (well over 64 KB) loads completely — the end of the text
  is reachable, not truncated.
- Piped input (`… | ./readeasy`) is still controllable — `Space`/`q` work,
  because the controls fall back to `/dev/tty`.
- A `.pdf` file (any case) is converted with `pdftotext` and reads like a text
  file. With `pdftotext` off the `PATH`, `readeasy file.pdf` prints a short
  "install poppler" note; a `.pdf` that isn't really a PDF prints "could not
  read PDF"; a valid PDF with no text layer (a scan) says so.
- Sentences read whole, not cut in half: hard-wrapped lines (from a PDF or an
  email) rejoin, words hyphenated across a line break rejoin, and a multi-page
  PDF's repeated running headers/footers and page numbers are dropped.
- `man ls | ./readeasy` reads cleanly without `col -b` — bold/underline
  overstrike (`c\bc`, `_\bc`) is collapsed, not read as doubled letters.
- `↑` / `↓` move the highlighted cursor sentence; `PgUp` / `PgDn` move about a
  screenful; `Home`/`End` (or `g`/`G`) jump to the first/last sentence. `Space`
  starts reading from the cursor sentence, not always from the beginning.
- While reading, the highlight advances sentence by sentence and the audio
  moves between sentences without a long gap.
- While reading, each word lights up in turn (word highlight), roughly in time
  with the voice, re-syncing each sentence; `w` toggles it off and on, and
  pausing restores the whole-sentence highlight.
- Pausing with `Space` leaves the cursor on the current sentence; pressing
  `Space` again resumes from that sentence (unless the cursor was moved).
- The bottom status bar shows the file name, position, percent, word count,
  state, and speed, and updates as the cursor moves and playback advances.
- Typographic characters (curly quotes, em dashes, ellipses, etc.) display
  correctly, not as mojibake — `setlocale` runs before `initscr`.
- The color theme is off by default; `--theme NAME` selects one (`none`,
  `blue`, `cream`, `contrast`, `dark`), `t` cycles them, and `--color` is
  shorthand for `--theme blue`.
- `f` (or `--focus`) dims all but the current sentence, and the dimming
  follows the cursor and playback; toggling it off restores full brightness.
- `--width N` (or `[` / `]`) wraps the text to a centred column of N columns
  with a blank line between sentences; it recentres on resize and `]` past
  full width returns to full-width layout.
- `+` / `-` change the speaking speed (shown as `wpm` in the status bar),
  clamped between 80 and 400. Changing speed while reading restarts the current
  sentence at the new speed (you hear it again from the start).
- `w` toggles the word highlight off and on while reading; `--no-word-highlight`
  (or `word_highlight off` in the config) starts with it off.
- `r` replays the current sentence from the start (works whether playing or
  paused). The status bar shows an estimated `~M:SS left`.
- The settings menu's **Pause** row (or `pause N` in the config, 0–2000 ms) adds
  a silent gap between sentences; at `0` playback stays gapless.
- `b` (or `--bionic` / `bionic on` in the config) bolds the first half of each
  word, and the bold combines with the cursor highlight, focus dim, and word
  highlight rather than replacing them.
- Quitting a file with the cursor moved, then reopening it by the same path,
  resumes on that sentence (with a brief "Resumed where you left off" note);
  `m` bookmarks the current sentence (naming it in the status bar), `m` again
  removes it, and `'` lists this file's bookmarks and jumps to one. Positions
  and bookmarks survive a restart (they live in `~/.config/readeasy/places`),
  are keyed by the file's path, and piped input is not remembered.
- `,` opens the settings menu: Up/Down and Left/Right (or `h`/`j`/`k`/`l`) move
  and change theme, speed, width, focus, word highlight, and voice; theme/width/
  focus apply live behind the menu; Enter on Voice opens a scrollable picker;
  `s` writes the config file; Esc closes. Relaunching picks up the saved values.
- Resizing the terminal re-flows the text to the new width (status bar stays
  at the bottom).
- `q` quits cleanly, and `Ctrl-C` while playing also exits cleanly —
  either way the terminal is restored and no `readeasy.*` directories are
  left in your temp folder.
- Each error case prints the expected message.

---

## Code style

Keep the style consistent with the existing code:

- **Language:** C, built with `-Wall -Wextra`. Fix all warnings before
  submitting.
- **Indentation:** 2 spaces, no tabs.
- **Braces:** opening brace on the same line as the statement.
- **One job per file:** keep input, speech, and UI logic in their own modules.
- **Headers:** if you add a public function, declare it in the matching `.h`
  file and use an include guard.
- **Error handling:** print a short message to standard error and return a
  non-zero value, as the current code does.
- **Memory:** free anything you allocate, and close any file you open.

---

## Speech backends (macOS and Linux)

All speech lives in `src/speech.c`, run with `fork` + `exec` (never a shell) and
selected at build time with `#ifdef __APPLE__`:

| | macOS | Linux |
| --- | --- | --- |
| `synth_to_file()` | `say -r RATE [-v VOICE] -o PATH -- TEXT` | `espeak-ng -s RATE [-v VOICE] -w PATH -- TEXT` |
| `play_file()` | `afplay PATH` | `aplay -q PATH` |
| `audio_duration()` | `afinfo` output | parses the WAV header (no external tool) |
| `list_voices()` | `say -v '?'` | `espeak-ng --voices` |

The audio file extension follows the platform (`AUDIO_EXT` in `speech.h`:
`aiff` on macOS, `wav` on Linux). The `--` before the text marks the end of
options so a sentence starting with `-` is never read as a flag — keep that if
you add a backend.

To add another platform (e.g. **Windows** via SAPI / PowerShell), add an
`#elif`/`#else` branch to each of the four functions and an `AUDIO_EXT`, keeping
the synth/play split so the next-sentence prefetch in `ui.c` still works. Please
keep the other platforms building and document any new dependency.

---

## Submitting changes

1. **Fork** the repository on GitHub.
2. **Create a branch** for your change:
   ```bash
   git checkout -b my-change
   ```
3. **Make your change** and rebuild with `make` (no warnings).
4. **Test** using the steps above.
5. **Commit** with a clear message describing what and why:
   ```bash
   git commit -m "Fix crash when file is empty"
   ```
6. **Push** your branch and **open a pull request** against the `main` branch of
   [ItsJasonPierrot/readeasy](https://github.com/ItsJasonPierrot/readeasy).

In your pull request, please describe what you changed, why, and how you tested
it.

---

## Reporting bugs and ideas

If you find a bug or have an idea, please
[open an issue](https://github.com/ItsJasonPierrot/readeasy/issues). Helpful
bug reports include:

- What you did (the command you ran).
- What you expected to happen.
- What actually happened (including any error message).
- Your operating system.
