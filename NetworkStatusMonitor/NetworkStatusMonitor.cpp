// NetworkStatusMonitor.cpp : 此檔案包含 'main' 函式。程式會於該處開始執行及結束執行。
//


#include "WMIControler.h"
#include "json.hpp"
#include "CWALNController.h"

#include <iostream>
#include <fstream>
#include <Windows.h>
#include <locale>
#include <codecvt>
#include <psapi.h>
#include <share.h>

std::string module_path;
extern VOID SvcReportEvent(LPTSTR szFunction);


typedef struct __MONITOR_INTERFACE {
    std::wstring name;
    std::wstring GUIDStr;
    GUID guid;
    std::vector<std::string> profiles;
}MONITOR_INTERFACE, * pMONITOR_INTERFACE;

std::vector<MONITOR_INTERFACE> vMonitor;



std::shared_ptr<CWALNController> CWALNController::m_instance = NULL;


void DoConnect(const MONITOR_INTERFACE &pInterface ) {
    auto pWLAN = CWALNController::GetInstance();
    if(pInterface.guid == GUID_NULL) {
        CLSIDFromString(pInterface.GUIDStr.c_str(), (LPCLSID) &pInterface.guid);
    }

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    for(auto profile : pInterface.profiles) {
        auto wsProfile = converter.from_bytes(profile);
        if(pWLAN->DoConnect(&pInterface.guid, wsProfile) == false) {
            continue;
        }

        if (pWLAN->WaitConnectComplete() == true) {
            break;
        }
        pWLAN->DoDisconnect(&pInterface.guid);
    }

}

void DoExport(std::string profile) {
    printf("Do export!!\n");
    char buf[1024];
    sprintf_s(buf, "netsh wlan export profile %s folder=%%cd%%", profile.c_str());
    system(buf);
}

static bool ASyncNetWorkDisconnectHandler(IWbemClassObject* pclsObj) {
    VARIANT vtProp;
    HRESULT hr;
    VariantInit(&vtProp);
    //SvcReportEvent((LPTSTR)L"Get Disconnect: 0");
    printf("Disconnect\n");
    hr = pclsObj->Get(L"InstanceName", 0, &vtProp, 0, 0);
    if (hr == 0) {
        printf("%ws disconnect!!\n", vtProp.bstrVal);
        std::find_if(vMonitor.begin(), vMonitor.end(), [pVar = &vtProp](const MONITOR_INTERFACE &pInfo) -> bool {
            if (pInfo.name == pVar->bstrVal) {
                //SvcReportEvent((LPTSTR)L"Get Disconnect");
                printf("Target interface disconnect: try to reconnect!!\n");
                DoConnect(pInfo);
                return true;
            }
            return false;
        });

    }

    VariantClear(&vtProp);


    return true;
}

#if 1
HANDLE hExitEvent = NULL;
BOOL WINAPI HandlerRoutine(
    _In_ DWORD dwCtrlType
) {
    if (CTRL_C_EVENT == dwCtrlType) {
        SetEvent(hExitEvent);
    }
    return TRUE;
}
#endif


WMIControler ROOT_WMI;


void CheckNetworkStatus() {
    WMIControler WMI_CIMv2;

    WMI_CIMv2.ConnectToServer(L"ROOT\\CIMV2");
    std::list<std::map<std::wstring, std::wstring>> outObj;

    WMI_CIMv2.ExecQuery(L"SELECT * FROM Win32_NetworkAdapter", { L"DeviceID",  L"Description", L"NetConnectionStatus", L"GUID"}, outObj);

    printf("List all interface\n");
    for (auto item : outObj) {
        MONITOR_INTERFACE newInterface = {
            item[L"Description"], L"", {}, {""},
        };
        for (auto& v : vMonitor) {
            printf("%ws => %ws\n", newInterface.name.c_str(), v.name.c_str());
            if (_wcsicmp(newInterface.name.c_str(), v.name.c_str()) == 0) {
                printf("Find: %ws\n", newInterface.name.c_str());
                v.name = newInterface.name;
                v.GUIDStr = item[L"GUID"].c_str();
                printf("NetConnectionStatus: %ws: %ws\n", item[L"NetConnectionStatus"].c_str(), item[L"GUID"].c_str());
                if (item[L"NetConnectionStatus"].compare(L"2") != 0) {
                    printf("Init not connect, try connect!!\n");
                    DoConnect(v);
                }
            }
        }
    }
}


DWORD StartMonitor(LPVOID lpThreadParameter)
{
    // get current folder
    std::string path;

    path.resize(1024);
    GetModuleFileNameExA(GetCurrentProcess(), NULL, (char*)path.c_str(), path.size());
    size_t iEnd = path.find_last_of("\\");
    if (iEnd != std::wstring::npos) {
        module_path = path.substr(0, iEnd+1);
    }

    printf("Currect path = %s\n", module_path.c_str());
    //return 0;

    // get monitor interface from config file
    module_path.append("config.json");
    printf("config file path = %s\n", module_path.c_str());
    std::ifstream f(module_path.c_str());

    auto j = nlohmann::json::parse(f);
    bool bAutoExprot = false;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    //std::string narrow = converter.to_bytes(wide_utf16_source_string);
    //std::wstring wide = converter.from_bytes(narrow_utf8_source_string);
    vMonitor.push_back({
        converter.from_bytes(j["interface"]),
        L"",
        GUID_NULL ,
        j["profile_name"]
    });

#if 0
    printf("%d: %s\n", argc, argv[1]);
    if (argc >= 2) {
        if (_stricmp(argv[1], "auto") == 0) {
            bAutoExprot = true;
            printf("auto mode\n");
        }
    }
#endif

    //vMonitor.push_back({L"Realtek Gaming 2.5GbE Family Controller", -1});
    //vMonitor.push_back({ L"Intel(R) Wi - Fi 6E AX211 160MHz", -1 });


    CheckNetworkStatus();

    //WMI_CIMv2.ExecNotificationQueryAsync("test", L"SELECT * "
    //        "FROM __InstanceCreationEvent WITHIN 1 "
    //    "WHERE TargetInstance ISA 'Win32_Process'", ASyncHandler);



    //WMI_CIMv2.CancelExecNotificationQueryAsync("test");
#if 1

    ROOT_WMI.ConnectToServer(L"ROOT\\WMI");

    std::list<std::map<std::wstring, std::wstring>> outObj2;


    if (ROOT_WMI.ExecNotificationQueryAsync("NetworkDisconn", L"SELECT * FROM MSNdis_StatusMediaDisconnect", ASyncNetWorkDisconnectHandler) == false) {
        SvcReportEvent((LPTSTR)L"ExecNotificationQueryAsync fail");
    }

#if 0
    hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(HandlerRoutine, true);
    WaitForSingleObject(hExitEvent, INFINITE);

    CloseHandle(hExitEvent);
#endif
#endif


    return 0;   // Program successfully completed.


}


int StartMonitorWorker(bool bWait) {
    CreateThread(NULL, 0, StartMonitor, NULL, 0, NULL);

#if 1
    if (bWait == true) {
        hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        SetConsoleCtrlHandler(HandlerRoutine, true);

        while (WaitForSingleObject(hExitEvent, 1000) == WAIT_TIMEOUT) {
            CheckNetworkStatus();
        }

        CloseHandle(hExitEvent);
    }
#else
    while (1) {
        Sleep(1000);
    }
#endif
    return 0;
}