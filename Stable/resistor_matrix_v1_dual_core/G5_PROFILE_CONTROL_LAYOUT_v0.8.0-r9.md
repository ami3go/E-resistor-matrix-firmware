# Gate 5 UI Layout Update v0.8.0-r9 — Profiles moved to Control

## Change summary

This revision consolidates profile operations into the main Control page.

## Implemented changes

- Removed the visible **Profiles** tab from the navigation bar.
- Kept **Profiles / Presets** controls on the **Control** page.
- Removed the old **Current state** section from the former Profiles page.
- Kept `/profiles` as a compatibility page that points users to the Control tab.
- Updated static asset cache-busting token to `ui=profilecontrol-r9`.

## Compatibility

Existing profile HTTP actions remain unchanged:

- `POST /profile_save`
- `POST /profile_apply`
- `POST /profile_delete`
- `POST /api/v1/control/profile`

Saved profile file format is unchanged.
