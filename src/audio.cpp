#include "audio.h"
#include "PolicyConfig.h"

#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

std::vector<AudioDevice> EnumerateDevices(EDataFlow flow)
{
    std::vector<AudioDevice> result;

    ComPtr<IMMDeviceEnumerator> pEnum;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(pEnum.GetAddressOf()));
    if (FAILED(hr)) return result;

    ComPtr<IMMDeviceCollection> pCollection;
    hr = pEnum->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, pCollection.GetAddressOf());
    if (FAILED(hr)) return result;

    UINT count = 0;
    pCollection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> pDevice;
        if (FAILED(pCollection->Item(i, pDevice.GetAddressOf()))) continue;

        // Get endpoint ID
        LPWSTR pwszId = nullptr;
        if (FAILED(pDevice->GetId(&pwszId))) continue;
        std::wstring id(pwszId);
        CoTaskMemFree(pwszId);

        // Get friendly name from property store
        ComPtr<IPropertyStore> pProps;
        if (FAILED(pDevice->OpenPropertyStore(STGM_READ, pProps.GetAddressOf()))) continue;

        PROPVARIANT varName;
        PropVariantInit(&varName);
        std::wstring name;
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) &&
            varName.vt == VT_LPWSTR && varName.pwszVal) {
            name = varName.pwszVal;
        }
        PropVariantClear(&varName);

        if (!name.empty()) {
            result.push_back({ std::move(id), std::move(name) });
        }
    }

    return result;
}

std::wstring GetDefaultDeviceId(EDataFlow flow, ERole role)
{
    ComPtr<IMMDeviceEnumerator> pEnum;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(pEnum.GetAddressOf()));
    if (FAILED(hr)) return {};

    ComPtr<IMMDevice> pDevice;
    hr = pEnum->GetDefaultAudioEndpoint(flow, role, pDevice.GetAddressOf());
    if (FAILED(hr)) return {};

    LPWSTR pwszId = nullptr;
    if (FAILED(pDevice->GetId(&pwszId))) return {};

    std::wstring id(pwszId);
    CoTaskMemFree(pwszId);
    return id;
}

bool SetDefaultDevice(const std::wstring& deviceId, ERole role)
{
    ComPtr<IPolicyConfig> pConfig;
    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigClient), nullptr,
                                  CLSCTX_ALL, __uuidof(IPolicyConfig),
                                  reinterpret_cast<void**>(pConfig.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = pConfig->SetDefaultEndpoint(deviceId.c_str(), role);
    return SUCCEEDED(hr);
}
