#pragma once

// Returns true if the app is registered in HKCU Run key.
bool IsStartupEnabled();

// Adds or removes the app from HKCU Run key.
// Returns true on success.
bool SetStartup(bool enable);
