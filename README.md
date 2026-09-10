# readeasy

**Read any text file out loud, right from your terminal.**

`readeasy` is a small command-line tool that opens a text file in a simple
terminal window and reads it aloud using text-to-speech. You stay in the
terminal — no browser, no GUI app. Press one key to start or stop the voice.

---

## Features

- Open a text file (or piped input) in a clean terminal view.
- Read the text aloud with a single key press.
- Start and stop the speech whenever you like.
- Tiny, fast, and written in C.

---

## Requirements

You need three things to build and run `readeasy`:

| Tool | What it is | Needed for |
| --- | --- | --- |
| A C compiler (`cc` / `gcc` / `clang`) | Turns the source code into a program | Building |
| `ncursesw` (wide-character ncurses) | A library for drawing terminal windows | Building & running |
| `say` | A text-to-speech command | Reading aloud |
| `afplay` | An audio-file player | Reading aloud |

`say` and `afplay` both ship with macOS. `readeasy` synthesizes each sentence
with `say` and plays it with `afplay`, preparing the next sentence while the
current one is still playing so the audio doesn't stutter between sentences.

`readeasy` needs the **wide-character** build of ncurses (`ncursesw`), not the
plain/narrow one. Without it, accented and non-ASCII characters — common in
text copied from PDFs — show up as garbled symbols instead of the correct
character.

### Platform support

- **macOS** — the `say` command ships with macOS, but the system's built-in
  `ncurses` is the narrow build. Install the wide-character version with
  Homebrew before building:
  ```bash
  brew install ncurses
  ```
  `make` detects it automatically and links against it.
- **Linux** — the terminal view works, but speech does **not** work out of the
  box. macOS has `say`; Linux does not. To get speech on Linux you would need
  to install a speech tool such as `espeak` and change the program to call it
  instead of `say` (see [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)). You also
  need the wide-character ncurses development package, e.g.
  `sudo apt install libncursesw5-dev`.
- **Windows** — not supported.

---

## Installation

**1. Clone the repository**

```bash
git clone https://github.com/ItsJasonPierrot/readeasy
```

**2. Go into the folder**

```bash
cd readeasy
```

**3. Build it**

```bash
make
```

This creates a program called `readeasy` in the current folder. Run it from
there with `./readeasy`, or install it globally (next step).

**4. (Optional) Install it globally**

To run `readeasy` from anywhere instead of `./readeasy` inside this folder:

```bash
sudo make install
```

This copies the program to `/usr/local/bin`. If that directory needs `sudo`
on your system, the command above handles it; to install somewhere on your
own `PATH` without `sudo`, set a prefix, e.g.:

```bash
make install PREFIX="$HOME/.local"
```

Remove it later with `sudo make uninstall` (or the matching `PREFIX`).

> **Note:** The old `gcc -o readeasy readeasy.c` command no longer works. The
> code is now split across several files, so always build with `make`.

---

## Usage

Run the program with the name of the file you want to read:

```bash
./readeasy [options] <filename>
```

For example:

```bash
./readeasy README.md
```

You can also **pipe** text into it instead of giving a filename:

```bash
cat notes.txt | ./readeasy
```

> If you installed it globally (step 4 above), drop the `./` and just run
> `readeasy <filename>` from any folder.

### Reading man pages, PDFs, and the clipboard

`readeasy` reads plain text, so anything you can turn into text on the command
line can be piped in:

```bash
man ls | col -b | readeasy            # a man page (col -b strips formatting)
pdftotext paper.pdf - | readeasy      # a PDF (needs poppler's pdftotext)
pbpaste | readeasy                    # whatever you've copied (macOS clipboard)
```

`pdftotext` comes from [poppler](https://poppler.freedesktop.org)
(`brew install poppler`). `col` and `pbpaste` are built into macOS.

### Options

| Option | What it does |
| --- | --- |
| `-r`, `--rate N` | Starting speed in words per minute (80–400, default 180). You can also change it live with `+` / `-`. |
| `--voice NAME` | Voice to use, passed to `say -v` (list voices with `say -v '?'`). |
| `--focus` | Start in focus mode (dim all but the current sentence; toggle with `f`). |
| `--no-color` | Skip the color theme and use your terminal's default colors. |
| `-v`, `--version` | Print the version and exit. |
| `-h`, `--help` | Print a usage summary and exit. |

For example, read a PDF slowly in a chosen voice:

```bash
pdftotext paper.pdf - | readeasy --rate 140 --voice Daniel
```

### Controls

Once the file is open, use these keys:

| Key | What it does |
| --- | --- |
| `↑` / `↓` | Move the highlighted cursor up or down one sentence. |
| `PgUp` / `PgDn` | Move up or down about one screenful. |
| `Home` / `End` (or `g` / `G`) | Jump to the first or last sentence. |
| `Space` | Start reading aloud from the cursor sentence. Press again to pause; press once more to resume from that same sentence. |
| `+` / `-` | Speak faster or slower (words per minute). Takes effect as reading continues. |
| `f` | Toggle **focus mode** — dim everything except the current sentence. |
| `Ctrl-L` | Redraw the screen (useful if it looks stale after switching terminal tabs). |
| `q` | Quit the program. |

`readeasy` re-flows the text to fill your terminal width and reads it one
**sentence** at a time. The highlighted sentence is the **cursor** — it marks
where reading starts and, while speech plays, it advances sentence by sentence
so you can see what is being read. If you pause and then press `Space` again
without moving the cursor, reading picks up from the same sentence.

A status bar along the bottom shows the file name, your position (sentence
number, total, and percent), whether it is playing or paused, the current
speaking speed in words per minute, and a reminder of the keys.

A full reference — including every error message and limit — is in
[docs/USAGE.md](docs/USAGE.md).

---

## Limits

- There is no fixed size limit — the whole file is loaded, growing the buffer
  as needed (bounded only by available memory).
- It reads plain text. It does not interpret Markdown, HTML, or PDF formatting —
  symbols like `#` or `*` are read aloud as written.
- Stray control characters (such as the page-break form feeds left behind by
  PDF-to-text extraction) are converted to spaces so they don't show up as
  garbled symbols on screen.
- Text is re-flowed to your window width, wrapping at word boundaries (words
  are never split across lines), so files that were hard-wrapped at a narrow
  column (common in text exported from PDFs) still fill the screen, and it
  re-flows again when you resize the terminal. Short lines such as headings
  are kept on their own; sentence boundaries are detected from `.`, `!`, `?`.

---

## Contributing

Contributions are welcome. Please read [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)
to learn how to set up, build, and submit changes, and
[docs/CODE_OF_CONDUCT.md](docs/CODE_OF_CONDUCT.md) for our community guidelines.

---

## License

`readeasy` is released under the MIT License. See the [LICENSE](LICENSE) file for
details.
