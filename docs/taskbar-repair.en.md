[简体中文](taskbar-repair.md) | [English](taskbar-repair.en.md) | [日本語](taskbar-repair.ja.md)

# Repairing the taskbar icon after an update

The launcher can repair a Start menu shortcut before launching ChatGPT when it confirms that the shortcut still points to an old app version. This avoids having to repair it manually after each update.

Only `ChatGPT.lnk` in the current user's Programs folder is checked. Its app ID must be `com.openai.codex`. Its target must be the current launcher itself, or `app\ChatGPT.exe` under the same installation root with the official package identity. The target, icon and working directory are changed to the current launcher. Existing arguments and the app ID are preserved. The repair uses the Windows Shell Link API; it does not parse the shortcut's binary format directly.

A shortcut that is already correct is only read, not saved again. Missing or unverified shortcuts are not created or taken over. `--launcher-check` skips repair. Temporary arguments passed to the launcher do not replace the shortcut's saved arguments; the caller still supplies the proxy arguments. The original shortcut is backed up once to `ChatGPTProxyLauncher/TaskbarShortcut.original.lnk` under the current user's local app data folder. Later repairs do not overwrite this backup.

Before saving, the launcher writes one temporary file and reads it back for verification. It then atomically replaces the shortcut and sends a Shell update notification for that file only. It does not clear the global icon cache or restart Explorer. A read lock is held during preparation, but must be released for the final rename, leaving a small window for concurrent edits. A failed repair does not block normal launching. No background service or log is added.

Limits: the launcher does not scan pinned taskbar items, change Windows startup entries or modify the official app package. It skips paths beyond the classic Shell path limit, Programs folders redirected to a network location, other app IDs, custom targets, and shortcuts that no longer point to the current launcher after it has been moved. Changes to the official app's filename, identity or icon handling may require an update to this repair. Keep the launcher at its configured path.

Verification: `tests/test-build.cmd` runs isolated shortcut tests. They cover old package paths, repeated calls, identity filtering, preserved arguments and properties, the first backup, failed backup or replacement, files in use, damaged files and path limits. All test shortcuts are under `work/`, and Shell notifications are mocked. Entry tests verify that check mode skips repair and normal launch preserves arguments. Actual Store updates and the visible taskbar result still need verification during real use.
