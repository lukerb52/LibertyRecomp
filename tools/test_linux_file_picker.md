# Linux native file-picker regression

This opt-in executable links the vendored **actual NFD GTK backend**. It does
not link LibertyRecomp, load game assets, or use the application PCH. GTK timers
select files inside NFD's real modal event loop; no desktop automation utility
is required. `NFD_Init`, dialog calls, and timer callbacks all run on main.

Dependencies: CMake >= 3.29, C++17 compiler, pkg-config, GTK3 development files,
Xvfb, xauth, and timeout (coreutils). On Debian/Ubuntu the relevant packages are
`build-essential cmake pkg-config libgtk-3-dev xvfb xauth coreutils`.

From the repository root:

```sh
cmake -S . -B out/build/picker-regression \
  -DLIBERTY_RECOMP_FILE_PICKER_TEST_ONLY=ON -DNFD_PORTAL=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/picker-regression --target liberty_test_linux_file_picker -j2
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
timeout 40s xvfb-run -a env -u WAYLAND_DISPLAY \
  GDK_BACKEND=x11 HOME="$scratch" XDG_CACHE_HOME="$scratch/cache" \
  GSETTINGS_BACKEND=memory LIBERTY_PICKER_UNDER_XVFB=1 \
  out/build/picker-regression/liberty_test_linux_file_picker
```

Only set `LIBERTY_PICKER_UNDER_XVFB=1` **inside xvfb-run**, never on a real
user display. The opt-in is a safety interlock, not automatic verification of
the X server implementation. The dedicated HOME/cache and in-memory settings
avoid writing the user's recent files, settings, or caches.

Expected output (exit 0):

```text
PASS: single .iso with spaces (NFD result 1)
PASS: multiple .iso including Unicode and spaces (NFD result 1)
PASS: cancel returns without paths (NFD result 2)
```

Each dialog has a 10-second automation deadline; the outer timeout also bounds
startup/shutdown. Selection is checked by exact returned paths and count, not
just by the response code. Every NFD pathset conversion result is checked.
Fixtures are empty temporary `.iso` files, removed on normal completion.

Some newer distributions use glycin/bubblewrap for even GTK icon loading.
If running within another restricted sandbox prevents the icon-loader child
from starting, GTK can abort before selection occurs. That is an environment
failure, not a passing test: run this same Xvfb command outside that sandbox.
Do not replace the real NFD backend with a mock to work around it.
