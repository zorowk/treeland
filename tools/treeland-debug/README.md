# treeland-debug

`treeland-debug` is an extensible C++ command-line inspector for Treeland
debug Remote Objects. Its initial inspector reads `WindowTreeRemote` through
the static Replica generated from `src/modules/resource/treelandwindowtree.rep`
and prints the result as JSON.

## Build and install

The client requires CMake, Qt6 Core, Qt6 RemoteObjects, and the Qt6 `repc` and
`moc` tools. Build it from the Treeland repository root:

```bash
cmake -S . -B build -DBUILD_TREELAND_DEBUG=ON
cmake --build build --target treeland-debug
sudo cmake --install build --component treeland-debug
```

The executable is installed to `/usr/local/bin/treeland-debug` by default.
Set `CMAKE_INSTALL_PREFIX` during configuration to use another prefix.

## DDE mode and access control

Treeland runs as the `dde` user in global mode. Its `WindowTree` Remote Object
uses group-only local socket access, with `treeland-debug` as the service's
primary group. Members of that group can run this client directly; `sudo -u dde`
is not required.

Create the group once, then add only authorised users. Replace `uos` with the
account that will run the client:

```bash
sudo groupadd --system treeland-debug
sudo usermod -aG treeland-debug uos
```

After installing a Treeland build that includes the updated systemd unit, reload
systemd and restart Treeland:

```bash
sudo systemctl daemon-reload
sudo systemctl restart treeland.service
```

Restarting Treeland interrupts the active graphical session. The client user
must also sign out and back in before its new group membership is available.

Verify the membership before using the client:

```bash
id -nG uos
```

The `treeland-debug` group grants access to the complete `WindowTree` Remote
Object, including window titles, application IDs, geometry, workspaces, and
cursor position. Do not add untrusted users to it.

Before starting Treeland, enable the `debugSource` DConfig option as the `dde`
user; otherwise the `WindowTree` Remote Object source is absent:

```bash
sudo -u dde -- dde-dconfig set \
  -a org.deepin.dde.treeland \
  -r org.deepin.dde.treeland \
  -k debugSource \
  -v true
```

Restart Treeland after changing this option.

## Usage

Print the complete layout tree:

```bash
/usr/local/bin/treeland-debug --tree
```

`--tree` is the default when neither `--tree` nor `--cursor` is specified.

Print the cursor position:

```bash
/usr/local/bin/treeland-debug --cursor
```

Connection options:

```bash
treeland-debug \
  --url local:org.deepin.dde.treeland.debug \
  --name WindowTree \
  --timeout-ms 30000
```

Default values match Treeland:

- URL: `local:org.deepin.dde.treeland.debug`
- Replica name: `WindowTree`

The layout JSON contains layers, workspaces, windows, geometry, visibility,
activation state, and the current Treeland mode.

## License

treeland is licensed under Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
