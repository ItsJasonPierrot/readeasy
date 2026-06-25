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
│   ├── speech.c    Starts the text-to-speech process (say)
│   └── ui.c        Terminal interface and keyboard controls (ncurses)
├── include/        Header files (.h) for each source module
├── docs/           Documentation
├── tests/          Sample text files used for manual testing
├── Makefile        Build instructions
└── README.md       Overview and quick start
```

Each `.c` file in `src/` has a matching `.h` file in `include/` that declares
its public functions.

---

## Setting up

You need:

- A C compiler — `cc`, `gcc`, or `clang`.
- The **ncurses** development library.
- The **say** command for speech (built into macOS).

On **macOS**, everything is already installed.

On **Linux**, install ncurses first:

```bash
sudo apt install build-essential libncurses-dev
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
`readeasy` executable.

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
echo "hello world" | ./readeasy      # piped input
./readeasy tests/a tests/b           # two args: should show usage error
```

Check that:

- The text appears correctly on screen.
- `Space` starts and stops the voice.
- `q` quits cleanly and returns you to the shell.
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

Speech currently uses the macOS-only `say` command, called in `src/speech.c`:

```c
execlp("say", "say", text, NULL);
```

To support Linux, you would detect the platform (or add a build option) and call
a Linux speech tool such as `espeak` instead:

```c
execlp("espeak", "espeak", text, NULL);
```

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
