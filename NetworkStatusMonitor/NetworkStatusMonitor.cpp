// NetworkStatusMonitor.cpp : 此檔案包含 'main' 函式。程式會於該處開始執行及結束執行。
//


#include "WMIControler.h"
#include "json.hpp"

#include <iostream>
#include <fstream>
#include <Windows.h>
#include <locale>
#include <codecvt>



typedef struct __MONITOR_INTERFACE {
    std::wstring name;
    std::string profile;
    bool bFind = false;
}MONITOR_INTERFACE, * pMONITOR_INTERFACE;

std::vector<MONITOR_INTERFACE> vMonitor;


static bool ASyncHandler(IWbemClassObject* pclsObj) {
    return true;
}

void DoConnect(std::string profile) {
    char buf[1024];

    sprintf_s(buf, "netsh wlan add profile filename=\"Wi-Fi-%s.xml\" interface=\"Wi-Fi\"", profile.c_str());
    printf(buf);
    system(buf);
    sprintf_s(buf, "netsh wlan connect ssid=%s interface=Wi-Fi name=%s", profile.c_str(), profile.c_str());
    printf(buf);
    system(buf);
}

void DoExport(std::string profile) {
    printf("Do export!!\n");
    char buf[1024];
    sprintf_s(buf, "netsh wlan export profile %s key=clear folder=%%cd%%", profile.c_str());
    system(buf);
}

static bool ASyncNetWorkDisconnectHandler(IWbemClassObject* pclsObj) {
    VARIANT vtProp;
    HRESULT hr;
    VariantInit(&vtProp);

    hr = pclsObj->Get(L"InstanceName", 0, &vtProp, 0, 0);
    if (hr == 0) {
        printf("%ws disconnect!!\n", vtProp.bstrVal);
        std::find_if(vMonitor.begin(), vMonitor.end(), [pVar = &vtProp](const MONITOR_INTERFACE &pInfo) -> bool {
            if (pInfo.name == pVar->bstrVal) {
                printf("Target interface disconnect: try to reconnect!!\n");
                DoConnect(pInfo.profile);
                return true;
            }
            return false;
        });

    }

    VariantClear(&vtProp);


    return true;
}


HANDLE hExitEvent = NULL;
BOOL WINAPI HandlerRoutine(
    _In_ DWORD dwCtrlType
) {
    if (CTRL_C_EVENT == dwCtrlType) {
        SetEvent(hExitEvent);
    }
    return TRUE;
}


int main(int argc, char* argv[])
{
    // get monitor interface from config file

    std::ifstream f("config.json");
    auto j = nlohmann::json::parse(f);
    bool bAutoExprot = false;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    //std::string narrow = converter.to_bytes(wide_utf16_source_string);
    //std::wstring wide = converter.from_bytes(narrow_utf8_source_string);

    vMonitor.push_back({
        converter.from_bytes(j["interface"]),
        j["profile_name"],
        false
    });

    printf("%d: %s\n", argc, argv[1]);
    if (argc >= 2) {
        if (_stricmp(argv[1], "auto") == 0) {
            bAutoExprot = true;
            printf("auto mode\n");
        }
    }


    //vMonitor.push_back({L"Realtek Gaming 2.5GbE Family Controller", -1});
    //vMonitor.push_back({ L"Intel(R) Wi - Fi 6E AX211 160MHz", -1 });

    WMIControler WMI_CIMv2;
    WMI_CIMv2.ConnectToServer(L"ROOT\\CIMV2");

    std::list<std::map<std::wstring, std::wstring>> outObj;

    WMI_CIMv2.ExecQuery(L"SELECT * FROM Win32_NetworkAdapter", { L"DeviceID",  L"Description", L"NetConnectionStatus"}, outObj);
    printf("List all interface\n");
    for (auto item : outObj) {
        // for (auto obj : item) {
        //     printf("%ws: %ws\n", obj.first.c_str(), obj.second.c_str());
        // }
        MONITOR_INTERFACE newInterface = {
            item[L"Description"], "", false,
        };

        for (auto &v : vMonitor) {
            if (v.bFind == true) continue;
            printf("%ws => %ws\n", newInterface.name.c_str(), v.name.c_str());
            if (_wcsicmp(newInterface.name.c_str(), v.name.c_str()) == 0) {
                printf("Find: %ws\n", newInterface.name.c_str());
                v.name = newInterface.name;
                v.bFind = true;
                printf("NetConnectionStatus: %ws\n", item[L"NetConnectionStatus"].c_str());
                if (item[L"NetConnectionStatus"].compare(L"2") != 0) {
                    printf("Init not connect, try connect!!");
                    DoConnect(v.profile);
                }
                else if (bAutoExprot == true) {
                    DoExport(v.profile);
                }
            }
        }


    }

    //WMI_CIMv2.ExecNotificationQueryAsync("test", L"SELECT * "
    //        "FROM __InstanceCreationEvent WITHIN 1 "
    //    "WHERE TargetInstance ISA 'Win32_Process'", ASyncHandler);



    //WMI_CIMv2.CancelExecNotificationQueryAsync("test");

#if 1
    WMIControler ROOT_WMI;
    ROOT_WMI.ConnectToServer(L"ROOT\\WMI");

    std::list<std::map<std::wstring, std::wstring>> outObj2;


    ROOT_WMI.ExecNotificationQueryAsync("NetworkDisconn", L"SELECT * FROM MSNdis_StatusMediaDisconnect", ASyncNetWorkDisconnectHandler);

    hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(HandlerRoutine, true);
    WaitForSingleObject(hExitEvent, INFINITE);

    CloseHandle(hExitEvent);
#endif


    return 0;   // Program successfully completed.


}
