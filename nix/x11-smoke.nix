# Starts fcitx5 with the addon on Xvfb and switches a D-Bus input context to it, which makes the
# engine dlopen libxcb. Fails if that load fails, as it does on NixOS without the module's runpath.
{
  runCommand,
  writeText,
  dbus,
  fcitx5-with-addons,
  python3,
  xvfb,
}:

let
  profile = writeText "profile" ''
    [Groups/0]
    Name=Default
    Default Layout=us
    DefaultIM=lotus

    [Groups/0/Items/0]
    Name=keyboard-us
    Layout=

    [Groups/0/Items/1]
    Name=lotus
    Layout=

    [GroupOrder]
    0=Default
  '';

  client = writeText "client.py" ''
    import time
    import dbus

    bus = dbus.SessionBus()
    for _ in range(100):
        if bus.name_has_owner("org.fcitx.Fcitx5"):
            break
        time.sleep(0.1)
    im = bus.get_object("org.fcitx.Fcitx5", "/org/freedesktop/portal/inputmethod")
    path, _ = im.CreateInputContext([("program", "smoke")], dbus_interface="org.fcitx.Fcitx.InputMethod1")
    bus.get_object("org.fcitx.Fcitx5", path).FocusIn(dbus_interface="org.fcitx.Fcitx.InputContext1")
    ctl = bus.get_object("org.fcitx.Fcitx5", "/controller")
    ctl.SetCurrentIM("lotus", dbus_interface="org.fcitx.Fcitx.Controller1")
  '';
in
runCommand "fcitx5-ngosen-x11-smoke"
  {
    nativeBuildInputs = [
      dbus
      fcitx5-with-addons
      (python3.withPackages (ps: [ ps.dbus-python ]))
      xvfb
    ];
  }
  ''
    export HOME=$TMPDIR XDG_CONFIG_HOME=$TMPDIR/config
    mkdir -p $XDG_CONFIG_HOME/fcitx5
    cp ${profile} $XDG_CONFIG_HOME/fcitx5/profile
    # Let Xvfb pick a free display: without the sandbox, builds share /tmp/.X11-unix.
    Xvfb -displayfd 3 3>display 2>/dev/null &
    for _ in $(seq 50); do
      [ -s display ] && break
      sleep 0.1
    done
    export DISPLAY=:$(cat display)
    dbus-run-session --config-file=${dbus}/share/dbus-1/session.conf -- sh -c '
      fcitx5 --verbose="*=4" >fcitx5.log 2>&1 &
      python3 ${client}
      for _ in $(seq 50); do
        grep -q XInput2 fcitx5.log && break
        sleep 0.2
      done
      kill %1
    '
    if ! grep -q "Watching mouse clicks through XInput2" fcitx5.log; then
      cat fcitx5.log
      exit 1
    fi
    touch $out
  ''
