# MIC Browser

MIC Browser is a Java-based Android web browser focused on expressive, high-touch navigation and a modern UX. It layers a multi-control surface on top of Material theming to deliver flexible browsing on mobile with desktop-style conveniences.

## Features
- Material-driven layout with low-contrast backgrounds, high-contrast actions, and compact spacing.
- Multi-tab support with swipe-friendly chips for selecting or closing tabs.
- Smart address bar that accepts URLs or search terms and provides copy/share shortcuts.
- Pull-to-refresh, visual loading indicator, and optimized WebView configuration for responsive browsing.
- Power tools for bookmarks, history recall, incognito switching, desktop user-agent toggling, and quick home access.
- Safety systems with opt-in safe mode warnings, third-party tracking shields, and one-tap session purging to protect users.

## Development
This project uses the Android Gradle plugin. To build locally, open the folder in Android Studio or run:

```bash
./gradlew assembleDebug
```

Android SDK paths are resolved from your local `local.properties`.
