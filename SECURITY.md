# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 3.6.0   | :white_check_mark: |
| 3.5.x   | :x:                |
| < 3.5   | :x:                |

## Reporting a Vulnerability

You can report potential security vulnerabilities by sending an email to Daniel Kondor: kondor.dani@gmail.com. Please encrypt your message with the public key available at the end of this document, or from e.g. Ubuntu's key server:
```
gpg --keyserver keyserver.ubuntu.com --recv DCE94A82DD0C62FD
```

To receive an encrypted answer, please include your public key, and sign your message with the corresponding private key. Normally, you can expect a reply with at least an acknowledgment within 1-2 days, and a plan to address the issue within 1-2 weeks. In any case, please wait at least 30 days before posting about the issue publicly.


## General notes

To perform its function as a launcher and taskbar, Cairo-Dock needs to have broad permissions, including running arbitrary commands, managing other running apps (via the X11 protocol or privileged Wayland protocol extensions), taking snapshots of other apps (on X11, if showing thumbnails of minimized apps is enabled). This should be taken into account if running Cairo-Dock in a security-critical environment. Running Cairo-Dock in a sandboxed mode is currently not supported; while it might be possible, implementing it will be challenging due to the above requirements.

### Filesystem access

Currently, Cairo-Dock needs access to at least the following locations:
 - read / write access to its configuration under `$XDG_CONFIG_HOME/cairo-dock`
 - read access to GTK configuration
 - read access to its own installation (under `{prefix}/share/cairo-dock` and `{prefix}/lib/cairo-dock`)
 - read access to `.desktop` files corresponding to installed apps (under `$XDG_DATA_DIRS/applications`)
 - read access to icons associated with installed apps (under `$XDG_DATA_DIRS/icons`), also system fonts, etc.
 - read / execute access to any app it launches
 - read / execute access to some standard unix utilities (`cp`, `mv`, `rm` -- these are used when changing themes)

In theory, restricting access outside of these (e.g. to other files in `$HOME`) should be possible. However, care should be taken that apps started by Cairo-Dock do not inherit such restrictions.

### Internet access

Cairo-Dock accesses the Internet for the following:
 - downloading themes, currently from https://github.com/Cairo-Dock/glxdock-repository -- this will happen anytime the global "Themes" configuration is opened, or the configuration of any plugin is opened that allows changing its appearance via themes (e.g. clock, system-monitor)
 - many individual plug-ins that allow access to specific services e.g. weather (open-meteo.com), musicPlayer (downloading album art), Drop to share (various services), etc.

None of these are essential, and other functionality will keep working if Cairo-Dock is denied Internet access. However, again, apps launched by Cairo-Dock might inherit any such restriction.

### Integration with containers

Starting from version 3.6.0, Cairo-Dock will try to avoid directly spawning child processes for apps whenever possible. Specifically, it will use:
 - [D-Bus activation](https://specifications.freedesktop.org/desktop-entry-spec/latest/dbus.html) for apps that support this. Such apps will be spawned as the child process of the D-Bus service and / or systemd.
 - The `StartTransientUnit` interface of [systemd](https://systemd.io/CONTROL_GROUP_INTERFACE/) to put the newly launched app in its own cgroup / slice.

Of course, using these require access to the respective D-Bus interfaces.

## Public key to use for reporting

```
-----BEGIN PGP PUBLIC KEY BLOCK-----

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
G3bGJF3rt7EEB40aPiLMfA==
=UxiW
-----END PGP PUBLIC KEY BLOCK-----
```

