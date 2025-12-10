# LKJ Web IDE

LKJ is a minimal POSIX socket server and browser IDE combo so you can browse, edit, and execute commands inside a Linux workspace.

## Build & Run

```bash
# optional: export LKJ_HOST, LKJ_PORT, LKJ_WORKSPACE, LKJ_STATIC
make clean && make
./lkj
```

By default the server binds to `0.0.0.0:8080` and serves static assets from `assets/`. The workspace root defaults to the current directory.

## Desktop GUI window

A GTK+WebKit desktop window is bundled so you can edit and preview sites/apps without leaving your Linux desktop:

- GTK 3 and WebKit2GTK 4.1 headers are required (`libgtk-3-dev libwebkit2gtk-4.1-dev` on Ubuntu).
- When a graphical display is available, `lkj` automatically starts the server in the background and opens a native window that loads the IDE at `http://127.0.0.1:<port>`.
- If no GUI is detected (e.g., running headless), the server runs normally and you can open the IDE in your browser at `http://<host>:<port>`.

## API surface

- `GET /api/files?path=<path>` – list directory contents.
- `GET /api/file?path=<path>` – read a file.
- `POST /api/file?path=<path>` – write file content (request body is raw file bytes).
- `POST /api/exec` – execute a shell command (body contains the command line).

## Static assets

The `assets/index.html` file implements a single-page web IDE with explorer, editor, and command console panes.
