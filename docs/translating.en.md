[简体中文](translating.md) | [English](translating.en.md) | [日本語](translating.ja.md)

> This document may include machine-translated content. In case of discrepancies, the [Chinese original](translating.md) takes precedence.

# Community translation guide

Chinese is the source language. English can be used as a reference. UI text, help, errors, launcher notifications and PowerShell download messages live in `locales/`; no C++ edits are needed. Translations ship with the program, so users do not need separate language packs.

## Improve an existing translation

1. Open `zh-CN.json`, `en-US.json` or `ja-JP.json`.
2. Edit only the values on the right inside `strings`. Keys on the left are used by the code and must not be renamed.
3. Save as UTF-8. Use `\n` for a line break, `\n\n` between paragraphs, `\"` for a double quote and `\\` for a backslash.
4. Keep named placeholders such as `{path}` and `{version}`. You may move them, but do not translate or remove them. Values are inserted literally and are not interpreted as further translation text.
5. Run the following commands from the project root. Include the catalog changes and the regenerated `setup.ps1` in your pull request.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/locales.ps1 -Generate
powershell -NoProfile -ExecutionPolicy Bypass -File tools/locales.ps1
```

Validation does not require Visual Studio. `-Generate` updates embedded text; running without it only checks files. Do not manually edit `work/LocalizationData.h` or the `BEGIN GENERATED LOCALES` block in `setup.ps1`.

## Add a language

1. Copy `locales/zh-CN.json`, for example to `fr-FR.json`.
2. Set `locale` to `fr-FR` and `nativeName` to the language's own name, such as `Français`. Keep it short: this name appears in the menu.
3. Set `fontFamily` to a Windows font suitable for the language. `Segoe UI` works for Latin scripts. The font name can contain at most 31 characters. Only left-to-right layouts are currently supported.
4. Append `{"tag":"fr-FR","complete":false}` to the `languages` array in `locales/languages.json`. Keep Chinese first. The filename, `locale` and `tag` must match.
5. Translate `strings`. For unfinished entries, remove the key or leave its value empty; the program will use the Chinese text. Once complete, set `complete` to `true` so missing translations fail the build.
6. Run the commands above. The language is automatically included in the menu, both EXEs, PowerShell text and source archive. No C++ or release manifest edits are required for a UI language.

The three built-in languages must be complete. New languages may start with partial translations. Validation reports the number of missing entries. Editors can use `locales/catalog.schema.json` for format hints. The project script also checks duplicate and unknown keys, placeholders, characters and size limits.

## Writing rules

- Keep UI text concise. Buttons name actions. Errors explain what went wrong and what to do next.
- `setup.*` is for the configurator; `help.*` for question-mark help; `error.*` for deployment and input errors; `proxy.*` for port detection; `startup.*` for startup reports; `chain.*` for companion notifications; `download.*` for PowerShell.
- Keep technical names such as ChatGPT, Codex, `.env`, HTTP, SOCKS5 and PowerShell unchanged.
- Do not translate flags such as `--proxy-server`, JSON fields, machine error codes, filenames, environment variable names or ownership markers. Programs and scripts depend on these fixed values.
- Use named placeholders in complete sentences. Avoid fragments that depend on concatenation. Catalog text is plain text, without HTML or Markdown; line breaks in help text are displayed as written.
- Windows dialog buttons and system error details may still use the Windows language. Third-party names and errors are outside this project's translations.

## Check the actual display

After installing the Visual Studio desktop development tools for C++, run from the project root:

```powershell
cmd /c tests\test-build.cmd
cmd /c tests\analyze.cmd
```

Open `outputs/ChatGPTLauncherSetup.exe` and choose the new language at the top right. Check the main page, advanced page, language menu, question-mark help, and both themes. Reduce the window size and check 100%, 125%, 150%, 175% and 200% display scaling. Buttons, switches and help icons must not overlap. Check empty-field hints, long errors and combined status messages for clipping.

Automated tests use isolated folders and mocks. They do not deploy into the real startup folder or close ChatGPT. Screenshots stay in `work/`; do not commit screenshots containing personal paths.

For new features, update Chinese first, then English, Japanese and the calling code. Keep an existing key if its meaning has not changed. If its meaning changes, review all translations. Remove unused keys from every catalog. Increase `schemaVersion` only for incompatible changes to the catalog structure.

## Translate user documentation

Chinese keeps the original filename. English and Japanese use `.en.md` and `.ja.md`. Current groups are the root `README`, `docs/translating`, `docs/taskbar-repair`, `docs/release-upload` and `assets/README`. Every document starts with links to all three languages.

Keep steps, limitations, button names and links in sync. Use the corresponding catalog for button names. Keep commands, flags, environment variables and filenames unchanged. Prefer links to documents in the same language; mark untranslated developer documents as Chinese. Release notes are copied into GitHub Release bodies, so their top language links must be full GitHub URLs. Keep versions and download URLs aligned too.

Run `powershell -NoProfile -ExecutionPolicy Bypass -File tests/documentation-tests.ps1` to check language links, local links and command examples. When adding document translations, update the document groups and language suffixes in that test and add the files to the source list in `release.ps1`. Adding a UI language does not require translating every document. Do not translate internal working records such as `AGENTS.md` or `HANDOFF.md`.
