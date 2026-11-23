# Stories for the desktop

A Qt Quick audiobook player for [storiesd](https://github.com/carnager/storiesd).
Positions and bookmarks stay in step with the Stories app on your phone.

The first start asks where your audiobooks live:

- **On this computer** sets up storiesd as a `systemctl --user` service. An
  existing storiesd config or unit is used and never rewritten.
- **On a server** connects to a storiesd that's already running.

## Build

Needs Qt 6.5+ (Quick, Quick Controls, Network, DBus, SVG), libmpv, QtKeychain
and libqrencode. On Arch:

```sh
pacman -S --needed qt6-base qt6-declarative qt6-svg mpv qtkeychain-qt6 qrencode
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

"On this computer" needs `storiesd` on your `PATH`.

## Keys

| Key | Action |
|---|---|
| Space | Play or pause |
| Left, Right | Back 15 s, forward 30 s |
| Ctrl+Left, Ctrl+Right | Previous, next chapter |
| B | Bookmark |
| Ctrl+F | Search |
| Esc | Back |

Media keys and `playerctl` work through MPRIS.

The token is kept in your keyring. Without one, it's in a file only your user
can read.
