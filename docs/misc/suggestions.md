Here are some project ideas when learning the language.

## Command line tool to evaluate math expressions
```bash
mexpr.exe "(1 + 3) * 3"
mexpr.exe "x = 5 / 2 - 1"
```

- Parse math syntax, addition, multiplication, parenthesis...
- Interactive mode like python where you can set variables.
- Saved state between processes. (save variables and their variables to a file `~/mexpr-state.txt`)
- Lazy evaluation
```bash
mexpr.exe "c = 5"
#-> 5
mexpr.exe "x = 3 * c"
#-> 15 (3*5)
mexpr.exe "c = 2"
mexpr.exe "x"
#-> 6 (3*2)
```

## Code project statistics
```bash
projstats.exe --root . --lines --file-ext cpp,h
```

- Count number of lines in files.
- Count spaces.
- Count blank lines.
- Filter file extensions, list line count per file extension.

## Chat server/client
A text box where you write text.

A text box where you see all messages from server/clients.

Commands for connecting or starting a server (`/connect 127.0.0.1:2000` or `/start 2000`).

**What you need to know**:
- Creating a window.
- Rendering text.
- User keyboard input (maybe mouse).
- Sockets, server and clients, sending messages.


## Game about jumping?
You have a player on the ground.

Platforms randomly spawn.

You jump on platforms to get higher.

If you fall, you lose.

To make the game fun you'll probably need moving platforms, powerups, and enemies (birds with pointy beak?).

**What you need to know**:
- Creating a window.
- User input.
- Rendering rectangles, images, text.

