## Summary

<!-- Explain the user-visible or developer-visible outcome. Keep the pull request focused on one change. -->

## Why

<!-- Link the Issue or Discussion when one exists, and describe the concrete scenario this solves. -->

## Validation

- Build:
- Automated tests:
- Visual or interaction review:
- Not tested locally:

## Contract checklist

- [ ] The change is focused and does not include unrelated generated or formatting changes.
- [ ] Public API state, ownership, signals, compatibility, accessibility, and
      no-op behavior are documented or not applicable.
- [ ] Gallery examples, source snippets, installed headers, catalogs, and
      documentation remain aligned or are not applicable.
- [ ] PySide6 support is included in this slice or an intentional C++-only boundary is documented.
- [ ] I ran `python3 tools/quality/check_cpp_format.py --changed-from origin/main`
      when the pull request changes C++ files.
- [ ] I ran `git diff --check` and removed credentials, customer data, and private paths from the change and evidence.

## Screenshots or recordings

<!-- Required for visible UI changes. Include Light/Dark and relevant interaction states when applicable. -->
