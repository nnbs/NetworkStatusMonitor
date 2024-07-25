
#include "CWALNController.h"
#include <Wlanapi.h>


void WlanNotificationCallback(
    PWLAN_NOTIFICATION_DATA unnamedParam1,
    PVOID unnamedParam2
) {
    CWALNController* pControl = (CWALNController*)unnamedParam2;
    printf("WLAN notify: %d\n", unnamedParam1->NotificationCode);
    if (wlan_notification_acm_connection_complete == unnamedParam1->NotificationCode) {
        printf("Connect complete!!\n");
        pControl->SetConnected();
    }
}

CWALNController::CWALNController() {
    DWORD status;
    m_NegotiatedVersion = 2;
    status = WlanOpenHandle(m_NegotiatedVersion, 0, &m_NegotiatedVersion, &m_hClientHandle);
    if (status != ERROR_SUCCESS) {
        printf("WlanOpenHandle fail: 0x%08x\n", status);
        return;
    }

    status = WlanRegisterNotification(m_hClientHandle,
        WLAN_NOTIFICATION_SOURCE_ALL,
        TRUE,
        WlanNotificationCallback,
        this,
        NULL,
        &m_dwPrevNotifSource
    );
    if (status != ERROR_SUCCESS) {
        printf("WlanRegisterNotification fail: 0x%08x\n", status);
        return;
    }

    m_hConnected = CreateEvent(NULL, TRUE, FALSE, NULL);

}

CWALNController::~CWALNController() {
    if (m_hClientHandle) {
        WlanRegisterNotification(m_hClientHandle,
            WLAN_NOTIFICATION_SOURCE_NONE,
            TRUE,
            NULL,
            this,
            NULL,
            &m_dwPrevNotifSource);

        WlanCloseHandle(m_hClientHandle, 0);
        m_hClientHandle = NULL;
    }
}

std::shared_ptr<CWALNController> CWALNController::GetInstance() {
    if (m_instance == NULL) {
        m_instance = std::make_shared< CWALNController>();
    }
    return m_instance;
}

void CWALNController::ReleaseInstance() {
    m_instance = NULL;
}

void CWALNController::SetConnected() {
    SetEvent(m_hConnected);
}

bool CWALNController::DoConnect(const GUID* pInterfaceGuid, std::wstring& profile) {
    WLAN_CONNECTION_PARAMETERS ConnectProfile = {};
    ConnectProfile.wlanConnectionMode = wlan_connection_mode_profile;
    ConnectProfile.strProfile = profile.c_str();
    ConnectProfile.dwFlags = 0;
    ConnectProfile.dot11BssType = dot11_BSS_type_infrastructure;
    printf("Try to connect to %ws\n", profile.c_str());
    ResetEvent(m_hConnected);

    DWORD status = WlanConnect(m_hClientHandle, pInterfaceGuid, &ConnectProfile, NULL);
    if (status != ERROR_SUCCESS) {
        printf("WlanConnect fail: 0x%08x\n", status);
        return false;
    }
    return true;
}

void CWALNController::DoDisconnect(const GUID* pInterfaceGuid) {
    WlanDisconnect(m_hClientHandle, pInterfaceGuid, NULL);
}

bool CWALNController::WaitConnectComplete() {
    if (WaitForSingleObject(m_hConnected, 10000) == WAIT_OBJECT_0) {
        return true;
    }
    return false;
}

void CWALNController::GetAvailableNetworkList(const GUID* pInterfaceGuid) {
    PWLAN_AVAILABLE_NETWORK_LIST pBssList = NULL;
    DWORD status = WlanGetAvailableNetworkList(m_hClientHandle,
        pInterfaceGuid,
        0,
        NULL,
        &pBssList);
    if (status != ERROR_SUCCESS) {
        printf("GetAvailableNetworkList fail: 0x%08x\n", status);
        return;
    }

    for (auto j = 0; j < pBssList->dwNumberOfItems; j++) {
        int iRSSI = 0;
        PWLAN_AVAILABLE_NETWORK pBssEntry =
            (WLAN_AVAILABLE_NETWORK*)&pBssList->Network[j];
        wprintf(L"  Profile Name[%u]:  %ws\n", j, pBssEntry->strProfileName);

        wprintf(L"  SSID[%u]:\t\t ", j);
        if (pBssEntry->dot11Ssid.uSSIDLength == 0)
            wprintf(L"\n");
        else {
            for (int k = 0; k < pBssEntry->dot11Ssid.uSSIDLength; k++) {
                wprintf(L"%c", (int)pBssEntry->dot11Ssid.ucSSID[k]);
            }
            wprintf(L"\n");
        }

        wprintf(L"  BSS Network type[%u]:\t ", j);
        switch (pBssEntry->dot11BssType) {
        case dot11_BSS_type_infrastructure:
            wprintf(L"Infrastructure (%u)\n", pBssEntry->dot11BssType);
            break;
        case dot11_BSS_type_independent:
            wprintf(L"Infrastructure (%u)\n", pBssEntry->dot11BssType);
            break;
        default:
            wprintf(L"Other (%lu)\n", pBssEntry->dot11BssType);
            break;
        }

        wprintf(L"  Number of BSSIDs[%u]:\t %u\n", j, pBssEntry->uNumberOfBssids);

        wprintf(L"  Connectable[%u]:\t ", j);
        if (pBssEntry->bNetworkConnectable)
            wprintf(L"Yes\n");
        else {
            wprintf(L"No\n");
            wprintf(L"  Not connectable WLAN_REASON_CODE value[%u]:\t %u\n", j,
                pBssEntry->wlanNotConnectableReason);
        }

        wprintf(L"  Number of PHY types supported[%u]:\t %u\n", j, pBssEntry->uNumberOfPhyTypes);

        if (pBssEntry->wlanSignalQuality == 0)
            iRSSI = -100;
        else if (pBssEntry->wlanSignalQuality == 100)
            iRSSI = -50;
        else
            iRSSI = -100 + (pBssEntry->wlanSignalQuality / 2);

        wprintf(L"  Signal Quality[%u]:\t %u (RSSI: %i dBm)\n", j,
            pBssEntry->wlanSignalQuality, iRSSI);

        wprintf(L"  Security Enabled[%u]:\t ", j);
        if (pBssEntry->bSecurityEnabled)
            wprintf(L"Yes\n");
        else
            wprintf(L"No\n");

        wprintf(L"  Default AuthAlgorithm[%u]: ", j);
        switch (pBssEntry->dot11DefaultAuthAlgorithm) {
        case DOT11_AUTH_ALGO_80211_OPEN:
            wprintf(L"802.11 Open (%u)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        case DOT11_AUTH_ALGO_80211_SHARED_KEY:
            wprintf(L"802.11 Shared (%u)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        case DOT11_AUTH_ALGO_WPA:
            wprintf(L"WPA (%u)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        case DOT11_AUTH_ALGO_WPA_PSK:
            wprintf(L"WPA-PSK (%u)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        case DOT11_AUTH_ALGO_WPA_NONE:
            wprintf(L"WPA-None (%u)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        case DOT11_AUTH_ALGO_RSNA:
            wprintf(L"RSNA (%u)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        case DOT11_AUTH_ALGO_RSNA_PSK:
            wprintf(L"RSNA with PSK(%u)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        default:
            wprintf(L"Other (%lu)\n", pBssEntry->dot11DefaultAuthAlgorithm);
            break;
        }

        wprintf(L"  Default CipherAlgorithm[%u]: ", j);
        switch (pBssEntry->dot11DefaultCipherAlgorithm) {
        case DOT11_CIPHER_ALGO_NONE:
            wprintf(L"None (0x%x)\n", pBssEntry->dot11DefaultCipherAlgorithm);
            break;
        case DOT11_CIPHER_ALGO_WEP40:
            wprintf(L"WEP-40 (0x%x)\n", pBssEntry->dot11DefaultCipherAlgorithm);
            break;
        case DOT11_CIPHER_ALGO_TKIP:
            wprintf(L"TKIP (0x%x)\n", pBssEntry->dot11DefaultCipherAlgorithm);
            break;
        case DOT11_CIPHER_ALGO_CCMP:
            wprintf(L"CCMP (0x%x)\n", pBssEntry->dot11DefaultCipherAlgorithm);
            break;
        case DOT11_CIPHER_ALGO_WEP104:
            wprintf(L"WEP-104 (0x%x)\n", pBssEntry->dot11DefaultCipherAlgorithm);
            break;
        case DOT11_CIPHER_ALGO_WEP:
            wprintf(L"WEP (0x%x)\n", pBssEntry->dot11DefaultCipherAlgorithm);
            break;
        default:
            wprintf(L"Other (0x%x)\n", pBssEntry->dot11DefaultCipherAlgorithm);
            break;
        }

        wprintf(L"  Flags[%u]:\t 0x%x", j, pBssEntry->dwFlags);
        if (pBssEntry->dwFlags) {
            if (pBssEntry->dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED)
                wprintf(L" - Currently connected");
            if (pBssEntry->dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE)
                wprintf(L" - Has profile");
        }
        wprintf(L"\n");

        wprintf(L"\n");
    }
}

