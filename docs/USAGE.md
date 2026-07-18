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
the system text-to-speech command (`say`). It takes input in one of two ways:

1. **A filename** passed as an argument.
2. **Piped input** from another command, when no filename is given.

The text is shown on screen. You then control the speech with the keyboard.

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
| `Space` | Toggle speech. First press starts reading aloud from the top of the text currently visible on screen; next press stops it. |
| `↑` | Scroll up. |
| `↓` | Scroll down. |
| `q` | Quit `readeasy`. If speech is playing, it stops first. |

Speech starts from wherever you've scrolled to, not always from the
beginning of the file — scroll with `↑`/`↓` first if you want to jump ahead.
Speech also stops on its own when it reaches the end of the text. You can
then press `Space` to read from the top of the visible screen again.

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
| Control characters | Converted to spaces | Stray control bytes (e.g. form-feed page breaks from PDF-to-text extraction) are replaced with spaces so they don't render as garbled symbols. |
| Unicode display | Requires `ncursesw` | Multi-byte UTF-8 characters (accented letters, etc.) only render correctly if `readeasy` was built against wide-character ncurses. See [CONTRIBUTING.md](CONTRIBUTING.md). |
| Speech engine | macOS `say` | Speech is unavailable on systems without `say`. |

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
