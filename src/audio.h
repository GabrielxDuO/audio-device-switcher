#pragma once

#include <string>
#include <vector>
#include <mmdeviceapi.h>

struct AudioDevice {
    std::wstring id;
    std::wstring name;
};

// Returns all active devices for the given flow direction.
std::vector<AudioDevice> EnumerateDevices(EDataFlow flow);

// Returns the endpoint ID string of the current default device, or empty string on failure.
std::wstring GetDefaultDeviceId(EDataFlow flow, ERole role);

// Sets the default endpoint for the given role. Returns true on success.
bool SetDefaultDevice(const std::wstring& deviceId, ERole role);
