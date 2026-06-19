# readeasy - A Simple Command Line Interface (CLI) for Reading text files using TTS in the terminal

## Description

readEasy is a command-line interface designed to simplify the process of reading
markdown files directly in your terminal. It provides an easy-to-use and intuitive
way to navigate through your markdown documents without having to open them in a
browser or text editor.

## Installation

To install readEasy, follow these steps:

1. Download the latest version of `readeasy` from our [GitHub
repository](https://github.com/itsjasonpierrot/readeasy) by cloning it to your
local machine using the following command in your terminal:

```bash
git clone https://github.com/itsjasonpierrot/readeasy
```

2. Navigate to the `readeasy` directory:

```bash
cd readeasy
```

3. Compile and build the program using your preferred C compiler (such as GCC):

```bash
gcc -o readeasy readeasy.c
```

Now you have a compiled executable named `readeasy` in the same directory.

## Usage

Once built, you can run readEasy by using the following command:

```bash
./readeasy <filename>
```

Replace `<filename>` with the name of the text file you want to read.

For example:

```bash
./read-easy README.md
```

## Customization

You can customize the appearance and behavior of readEasy by modifying the source
code in the `readEasy.c` file. For more details on available configurations, refer
to [the official
documentation](https://github.com/your-username/read-easy#configuration).

## Contributing

We welcome contributions and pull requests from the community! If you find any
issues or have ideas for new features, feel free to open an issue or submit a pull
request on our [GitHub repository](https://github.com/your-username/read-easy).

## License

readEasy is released under the MIT license. For more information about licensing,
visit the [LICENSE file](LICENSE) in this repository.
