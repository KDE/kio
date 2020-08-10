/* This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 René J.V. Bertin

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#import <CoreFoundation/CoreFoundation.h>
#import <AppKit/AppKit.h>

// Set the LaunchServices LSUIElement property programmatically in unbundled
// ("nongui") executables, instead of in the app bundle's Info.plist file.
// This function has to be called as early as possible in main(), and before
// creating the QApplication instance.
void makeAgentApplication()
{
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
        // get the application's Info Dictionary. For app bundles this would live in the bundle's Info.plist,
        // for regular executables it is obtained in another way.
        CFMutableDictionaryRef infoDict = (CFMutableDictionaryRef) CFBundleGetInfoDictionary(mainBundle);
        if (infoDict) {
            // Add or set the "LSUIElement" key with/to value "1". This can simply be a CFString.
            CFDictionarySetValue(infoDict, CFSTR("LSUIElement"), CFSTR("1"));
            // That's it. We're now considered as an "agent" by the window server, and thus will have
            // neither menubar nor presence in the Dock or App Switcher.
        }
    }
}

void setAgentActivationPolicy()
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
