/* This file is part of the KDE libraries
    Copyright (C) 2017 René J.V. Bertin

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or (at
    your option) version 3 or, at the discretion of KDE e.V. (which shall
    act as a proxy as in section 14 of the GPLv3), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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
