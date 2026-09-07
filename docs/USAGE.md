# readeasy — Usage Reference

This is the full reference for `readeasy`. For a quick start, see the
[README](../README.md).

---

## Name

**readeasy** — read a text file aloud in the terminal.

---

## Synopsis

```
readeasy <filename>
command | readeasy
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

`readeasy` accepts **at most one** filename. Passing two or more arguments is an
error.

---

## Controls

While a file is open, these keys work:

| Key | Action |
| --- | --- |
| `↑` | Move the cursor up one sentence. |
| `↓` | Move the cursor down one sentence. |
| `Space` | Start reading aloud from the cursor sentence; press again to pause. |
| `Ctrl-L` | Redraw the screen (see *Recovering the display* below). |
| `q` | Quit `readeasy`. If speech is playing, it stops first. |

### The cursor and sentence tracking

`readeasy` reads one **sentence** at a time. One sentence is always
highlighted — this is the **cursor**. It marks where reading will begin and
shows which sentence you are on.

- Move the cursor with `↑` / `↓`.
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

---

## Limits

| Limit | Value | Notes |
| --- | --- | --- |
| Maximum text size | ~64 KB | About 10,000 words. Text beyond this is not loaded. |
| Formatting | Plain text only | Markdown, HTML, and PDF symbols are read literally. |
| Re-flow | Fills window width | Hard-wrapped lines are joined and re-wrapped to the terminal; short lines (e.g. headings) are kept separate. |
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
| `Usage: readeasy <filename>` | More than one argument was given. | Pass at most one filename. |
| `No input provided.` | No filename and no piped input. | Give a filename or pipe text in. |
| `File not found.` | The filename could not be opened. | Check the path and spelling. |
| `Error reading file.` | The file could not be read after opening. | Check file permissions and that it is readable. |
| `File is empty` | The file or piped input contained no text. | Use a file that has content. |
| `Allocation failed.` | The program could not reserve memory. | Free up memory and try again. |
| `fork failed` | The system could not start the speech process. | Try again; check system limits. |

---

## See also

- [README.md](../README.md) — overview and installation.
- [CONTRIBUTING.md](CONTRIBUTING.md) — building and changing the code.
