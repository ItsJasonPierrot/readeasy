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
│   ├── input.c     Reads the file or piped input into a buffer
│   ├── reflow.c    Re-flows text and splits it into sentences
│   ├── speech.c    Synthesizes (say) and plays (afplay) sentence audio
│   └── ui.c        Terminal interface, keyboard controls, playback (ncurses)
├── include/        Header files (.h) for each source module
├── docs/           Documentation
├── tests/          Sample text files used for manual testing
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

On **Linux**, install the wide-character ncurses package:

```bash
sudo apt install build-essential libncursesw5-dev
```

Note: speech will not work on Linux until the code is changed to use a Linux
speech tool (see *Adding Linux speech support* below).

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

To remove the executable and the compiled object files:

```bash
make clean
```

---

## Testing

`readeasy` is tested by hand using the sample files in `tests/`. After making a
change, rebuild and try a few cases:

```bash
make
./readeasy tests/longer_text.txt     # a normal file
./readeasy tests/empty.txt           # should say "File is empty"
./readeasy tests/pdf.txt             # accented characters and page breaks
echo "hello world" | ./readeasy      # piped input
./readeasy tests/a tests/b           # two args: should show usage error
```

Check that:

- The text fills the window width, including accented/non-ASCII characters
  (see `tests/pdf.txt`) — no garbled symbols, no narrow column left over from
  a hard-wrapped file.
- `↑` / `↓` move the highlighted cursor sentence, and `Space` starts reading
  from the cursor sentence, not always from the beginning.
- While reading, the highlight advances sentence by sentence and the audio
  moves between sentences without a long gap.
- Pausing with `Space` leaves the cursor on the current sentence; pressing
  `Space` again resumes from that sentence (unless the cursor was moved).
- Resizing the terminal re-flows the text to the new width.
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

## Adding Linux speech support

Speech currently uses the macOS-only `say` and `afplay` commands in
`src/speech.c`: `synth_to_file()` writes a sentence to an audio file with
`say -o`, and `play_file()` plays it with `afplay`:

```c
execlp("say", "say", "-o", path, text, (char *)NULL);   /* synth_to_file */
execlp("afplay", "afplay", path, (char *)NULL);          /* play_file */
```

To support Linux, you would detect the platform (or add a build option) and use
Linux tools instead — for example synthesizing with `espeak -w file.wav` and
playing with `aplay`, or having a single tool both synthesize and play. Keep
the synth/play split so the next-sentence prefetch in `ui.c` still works.

If you take this on, please keep macOS working and document any new dependency.

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
