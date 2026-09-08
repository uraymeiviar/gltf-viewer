# Gestaltor Resources
## QRC

A lot of the resources in this directory are added to Gestaltor with Qt QRC files that you can find in the project root. These QRC resources are added to the compiled binary by the Qt build tools (using the provided cmake functions). You can read more about QRC in the Qt documentation.

## Entitlements files

The files with the ending `.entitlements` are macOS entitlements configurations that configure the macOS app runtime. The different Gestaltor versions have different requirements so they each get their own entitlements file. 

The **Otoy** version requires some entitlements for the DRM tool that it uses to obfuscate an authentication key.

The **Mac App Store** version requires the [App Sandbox](https://developer.apple.com/documentation/security/app_sandbox) to be enabled and then requires a set of exceptions from that sandbox to be able to still interact with the filesystem and to access the network.

The **Steam** version requires [exceptions](https://partner.steamgames.com/doc/store/application/platforms#Entitlements) to be able to load the Steam platform libraries.

## Info.plist.in

This file is used as the [Info.plist](https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Introduction/Introduction.html#//apple_ref/doc/uid/TP40009248-SW1) file for the macOS app and contains variables set by [CMake `configure_file`](https://cmake.org/cmake/help/latest/command/configure_file.html).
The file contains information about the App's version, name, supported file types, etc.

## qt.conf

This file contains information [used by the Qt library](https://doc.qt.io/qt-6/qt-conf.html) about where to find resources and plugins in the application bundle.

## Assets.xcassets

This directory is a Xcode assets bundle containing e.g. the Gestaltor icon for macOS. The icon was generated from the Gestaltor AppIcon PNG file. You can create this with one of various tools, e.g. [Icon Set Creator](https://apps.apple.com/de/app/icon-set-creator/id939343785?l=en&mt=12)