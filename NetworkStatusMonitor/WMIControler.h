

#include "EventSink.h"

#include <windows.h>
#include <vector>
#include <string>
#include <list>
#include <map>
#include <Wbemidl.h>


typedef struct __ASyncInfo {
    std::wstring command;
    pfn_ASyncHandler cb = NULL;
    IUnsecuredApartment* pUnsecApp = NULL;
    EventSink* pSink = NULL;
    IUnknown* pStubUnk = NULL;
    IWbemObjectSink* pStubSink = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;

    void Release() {
        printf("~__ASyncInfo\n");
        if (pUnsecApp) {
            pUnsecApp->Release();
        }
        if (pStubUnk) {
            pStubUnk->Release();
        }
        if (pSink) {
            pSink->Release();
        }
        if (pStubSink) {
            pStubSink->Release();
        }
    }
}ASyncInfo, *pASyncInfo;

class WMIControler {
public:
    WMIControler();
    virtual ~WMIControler();

    bool ConnectToServer(const wchar_t* pNamespace);
    bool ExecQuery(const wchar_t* WQLCommand, std::vector<std::wstring>queryName, std::list<std::map<std::wstring, std::wstring>>& outObj);
    bool ExecNotificationQueryAsync(std::string key, const wchar_t* WQLCommand, pfn_ASyncHandler cb) ;
    void CancelExecNotificationQueryAsync(std::string key);

    bool ExecNotificationQuery(std::string key, const wchar_t* WQLCommand, pfn_ASyncHandler cb);

private:
    bool m_bInitOK = false;

    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;

    std::map<std::string, ASyncInfo> m_async_info;
};