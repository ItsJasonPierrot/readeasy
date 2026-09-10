# readeasy — Usage Reference

This is the full reference for `readeasy`. For a quick start, see the
[README](../README.md).

---

## Name

**readeasy** — read a text file aloud in the terminal.

---

## Synopsis

```
readeasy [options] <filename>
command | readeasy [options]
```

---

## Description

`readeasy` loads a text file into a terminal window and can read it aloud using
the system text-to-speech command (`say`), playing the audio with `afplay`. It
takes input in one of two ways:

1. **A filename** passed as an argument.
2. **Piped input** from another command, when no filename is given.

The text is re-flowed to the window width and shown on screen. You then control
the speech with the keyboard, one sentence at a time.

---

## Arguments

| Argument | Required? | Description |
| --- | --- | --- |
| `<filename>` | Optional | Path to the text file to read. If omitted, `readeasy` reads from piped input instead. |

`readeasy` accepts **at most one** filename. Passing two or more filenames is an
error.

---

## Options

| Option | Description |
| --- | --- |
| `-r`, `--rate N` | Starting speaking speed in words per minute (clamped to 80–400; default 180). Can also be changed live with `+` / `-`. |
| `-w`, `--width N` | Wrap text to N columns and centre it as a reading column (minimum 20). Off by default (full width). Adjust live with `[` / `]`. |
| `--voice NAME` | Text-to-speech voice, passed to `say -v NAME`. List the available voices with `say -v '?'`. |
| `--focus` | Start in focus mode (dim everything except the current sentence). Toggle at any time with `f`. |
| `--color` | Apply the blue/cream color theme. Off by default — the terminal's own colors are used, with the current sentence and status bar shown in reverse video. (`--no-color` is accepted too and is the default.) |
| `-v`, `--version` | Print the version and exit. |
| `-h`, `--help` | Print a usage summary and exit. |

Options may appear before or after the filename. An unknown option, or a
missing value for `--rate`/`--voice`, prints the usage summary and exits with
status `1`.

---

## Controls

While a file is open, these keys work:

| Key | Action |
| --- | --- |
| `↑` | Move the cursor up one sentence. |
| `↓` | Move the cursor down one sentence. |
| `PgUp` / `PgDn` | Move the cursor up or down about one screenful. |
| `Home` / `g` | Jump to the first sentence. |
| `End` / `G` | Jump to the last sentence. |
| `Space` | Start reading aloud from the cursor sentence; press again to pause. |
| `+` | Speak faster (increase words per minute). |
| `-` | Speak slower (decrease words per minute). |
| `f` | Toggle focus mode (dim all but the current sentence). |
| `[` | Narrow the centred reading column. |
| `]` | Widen the reading column (past full width turns it off). |
| `Ctrl-L` | Redraw the screen (see *Recovering the display* below). |
| `q` | Quit `readeasy`. If speech is playing, it stops first. |

### The cursor and sentence tracking

`readeasy` reads one **sentence** at a time. One sentence is always
highlighted — this is the **cursor**. It marks where reading will begin and
shows which sentence you are on.

- Move the cursor with `↑` / `↓`, jump about a screenful with `PgUp` /
  `PgDn`, or go to the very start or end with `Home` / `End` (or `g` / `G`).
- Press `Space` to start reading from the cursor sentence. As each sentence
  finishes, the highlight advances to the next one, so the cursor always
  shows the sentence currently being read.
- Press `Space` again to **pause**. The cursor stays on the sentence that
  was being read.
- Press `Space` once more to **resume** from that same sentence — unless you
  moved the cursor while paused, in which case reading resumes from the new
  cursor sentence. (Moving the cursor with `↑` / `↓` while reading pauses
  playback.)
- Reading stops on its own at the end of the text.

### The status bar

The bottom line of the screen is a status bar. On the left it shows the file
name (or `(stdin)` for piped input), your position as `sentence/total`, the
percent through the text, the total word count, whether it is `playing` or
`paused`, and the current speaking speed in words per minute (`wpm`). On the
right, when the window is wide enough, it lists the main keys. It updates as
you move the cursor, change the speed, and as playback advances.

### Speaking speed

`+` speeds the voice up and `-` slows it down, in steps of 20 words per minute
(from 80 up to 400; the default is 180). A change applies to sentences read
from then on, so it can take a sentence to fully take effect while playing.

### Focus mode

Press `f` (or start with `--focus`) to dim every sentence except the current
one, so only the sentence you are on stands out. It reduces visual clutter for
easier reading. The dimming follows the cursor as you move and as playback
advances; press `f` again to turn it off. (Dimming uses the terminal's faint
attribute, so on a terminal that doesn't support faint text it simply has no
visible effect.)

### Reading column

By default the text fills the whole terminal width. `--width N` (or the `[` /
`]` keys) wraps it to a narrower column of N characters and centres it, with a
blank line between sentences. A shorter line length is easier for many readers
to follow than full-width text. Widening past the terminal width with `]` turns
the column off and returns to full width.

While a sentence is playing, `readeasy` synthesizes the next one in the
background so playback moves from one sentence to the next without a
noticeable gap.

### Resizing and recovering the display

Resize the terminal and `readeasy` re-flows the text to the new width
automatically, keeping the current sentence in view. Some terminals also
leave the screen stale after you switch away to another tab or window and
back — press `Ctrl-L` to force a full redraw.

### Quitting

Press `q` for a normal quit. Interrupting with `Ctrl-C` (or the program
receiving `SIGTERM`/`SIGHUP`) also exits cleanly: it stops any speech,
restores your terminal, and removes its temporary files.

---

## How input is chosen

`readeasy` decides where to read text from using these rules, in order:

1. If you give a filename, it reads that file.
2. If you give no filename **and** pipe text in, it reads the piped text.
3. If you give no filename **and** there is no piped text, it stops with an
   error (see *No input provided* below).

---

## Examples

Read a file aloud:

```bash
./readeasy notes.txt
```

Read the output of another command:

```bash
ls -l | ./readeasy
```

Read a man page's plain text:

```bash
man ls | col -b | ./readeasy
```

Read a PDF (via poppler's `pdftotext`) or the macOS clipboard:

```bash
pdftotext paper.pdf - | ./readeasy
pbpaste | ./readeasy
```

If `readeasy` is installed globally (`make install`), drop the `./` and run
`readeasy` from anywhere.

---

## Limits

| Limit | Value | Notes |
| --- | --- | --- |
| Maximum text size | Available memory | The whole file is loaded; the buffer grows as needed, so there is no fixed size limit. |
| Formatting | Plain text only | Markdown, HTML, and PDF symbols are read literally. |
| Re-flow | Fills window width | Hard-wrapped lines are joined and re-wrapped to the terminal at word boundaries (words are not split); short lines (e.g. headings) are kept separate. |
| Sentence splitting | On `.` `!` `?` | Boundaries are detected heuristically, so unusual punctuation may split a little early or late. |
| Control characters | Converted to spaces | Stray control bytes (e.g. form-feed page breaks from PDF-to-text extraction) are replaced with spaces so they don't render as garbled symbols. |
| Unicode display | Requires `ncursesw` | Multi-byte UTF-8 characters (accented letters, etc.) only render correctly if `readeasy` was built against wide-character ncurses. See [CONTRIBUTING.md](CONTRIBUTING.md). |
| Speech engine | macOS `say` + `afplay` | Speech is unavailable on systems without both commands. |

---

## Exit status

| Code | Meaning |
| --- | --- |
| `0` | Success. |
| `1` | An error occurred (see *Error messages* below). |

---

## Error messages

All errors print to standard error and exit with status `1`.

| Message | Cause | How to fix |
| --- | --- | --- |
| `Usage: readeasy [options] [file]` … | More than one filename, an unknown option, or a missing option value. | Pass at most one filename and check the options (`readeasy --help`). |
| `No input provided.` | No filename and no piped input. | Give a filename or pipe text in. |
| `File not found.` | The filename could not be opened. | Check the path and spelling. |
| `Error reading file.` | The file could not be read after opening. | Check file permissions and that it is readable. |
| `File is empty` | The file or piped input contained no text. | Use a file that has content. |
| `Allocation failed.` | The program could not reserve memory. | Free up memory and try again. |
| `No terminal available for controls.` | Text was piped in but there is no terminal to read key presses from. | Run `readeasy` in a terminal (piped input still needs a `/dev/tty` for the controls). |
| `fork failed` | The system could not start the speech process. | Try again; check system limits. |

---

## See also

- [README.md](../README.md) — overview and installation.
- [CONTRIBUTING.md](CONTRIBUTING.md) — building and changing the code.
