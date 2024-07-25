#pragma once


#include <string>
#include <Windows.h>
#include <memory>

class CWALNController {
public:
    CWALNController();
    virtual ~CWALNController();

    void SetConnected();

    static std::shared_ptr<CWALNController> GetInstance();
    static void ReleaseInstance();

    bool DoConnect(const GUID* pInterfaceGuid, std::wstring& profile);
    void DoDisconnect(const GUID* pInterfaceGuid);
    bool WaitConnectComplete();

    void GetAvailableNetworkList(const GUID* pInterfaceGuid);

private:

    static std::shared_ptr<CWALNController> m_instance;

    HANDLE m_hClientHandle;
    DWORD m_NegotiatedVersion;

    HANDLE m_hConnected = NULL;

    DWORD m_dwPrevNotifSource;
};

