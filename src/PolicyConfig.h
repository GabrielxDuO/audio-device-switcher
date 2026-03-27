#pragma once

// IPolicyConfig -- undocumented COM interface used to set default audio endpoints.
// Stable from Windows Vista through Windows 11.
// Adapted from community reverse-engineering (stackoverflow, manyrootsofallevilrants.blogspot.com).

#include <mmdeviceapi.h>

MIDL_INTERFACE("f8679f50-850a-41cf-9c72-430f290290c8")
IPolicyConfig : public IUnknown
{
public:
    virtual HRESULT GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT GetDeviceFormat(PCWSTR, BOOL, WAVEFORMATEX**) = 0;
    virtual HRESULT ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT GetProcessingPeriod(PCWSTR, BOOL, PINT64, PINT64) = 0;
    virtual HRESULT SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT GetShareMode(PCWSTR, struct DeviceSharing*) = 0;
    virtual HRESULT SetShareMode(PCWSTR, struct DeviceSharing*) = 0;
    virtual HRESULT GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT SetDefaultEndpoint(PCWSTR deviceId, ERole role) = 0;
    virtual HRESULT SetEndpointVisibility(PCWSTR, BOOL) = 0;
};

// {870af99c-171d-4f9e-af0d-e63df40c2bc9}
class DECLSPEC_UUID("870af99c-171d-4f9e-af0d-e63df40c2bc9") CPolicyConfigClient;
