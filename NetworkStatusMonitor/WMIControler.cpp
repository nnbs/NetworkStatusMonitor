

#include "WMIControler.h"

#pragma comment(lib, "wbemuuid.lib")



WMIControler::WMIControler() {
    HRESULT hres;
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
        std::cout << "Failed to initialize COM library. Error code = 0x"
            << std::hex << hres << std::endl;
        return;                  // Program has failed.
    }
    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------
#if 0
    hres = CoInitializeSecurity(
        NULL,
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities
        NULL                         // Reserved
    );

    if (FAILED(hres))
    {
        std::cout << "Failed to initialize security. Error code = 0x"
            << std::hex << hres << std::endl;
        CoUninitialize();
        return;                    // Program has failed.
    }
#endif

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);

    if (FAILED(hres))
    {
        std::cout << "Failed to create IWbemLocator object."
            << " Err code = 0x"
            << std::hex << hres << std::endl;
        CoUninitialize();
        return;                 // Program has failed.
    }

    m_bInitOK = true;

}

WMIControler::~WMIControler() {
    for (auto item : m_async_info) {
        item.second.Release();
    }

    m_async_info.clear();

    if (pLoc) {
        pLoc->Release();
    }

    if (pSvc) {
        pSvc->Release();
    }

    if (m_bInitOK) {
        CoUninitialize();
    }
}

bool WMIControler::ConnectToServer(const wchar_t* pNamespace) {
    if (!m_bInitOK) {
        return false;
    }

    // Step 4: -----------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method


    HRESULT hres;
    // Connect to the root\cimv2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hres = pLoc->ConnectServer(
        _bstr_t(pNamespace), // Object path of WMI namespace
        NULL,                    // User name. NULL = current user
        NULL,                    // User password. NULL = current
        0,                       // Locale. NULL indicates current
        NULL,                    // Security flags.
        0,                       // Authority (for example, Kerberos)
        0,                       // Context object
        &pSvc                    // pointer to IWbemServices proxy
    );
    if (FAILED(hres))
    {
        std::cout << "Could not connect. Error code = 0x"
            << std::hex << hres << std::endl;

        return false;                // Program has failed.
    }

    std::cout << "Connected to " << pNamespace << " WMI namespace" << std::endl;

    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------
    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities
    );

    if (FAILED(hres))
    {
        std::cout << "Could not set proxy blanket. Error code = 0x"
            << std::hex << hres << std::endl;

        return false;               // Program has failed.
    }

    return true;
}

bool WMIControler::ExecQuery(const wchar_t* WQLCommand, std::vector<std::wstring>queryName, std::list<std::map<std::wstring, std::wstring>>& outObj) {
    HRESULT hres;
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(WQLCommand),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres))
    {
        std::cout << "Query for operating system name failed."
            << " Error code = 0x"
            << std::hex << hres << std::endl;
        return false;               // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    while (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
            &pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        std::map<std::wstring, std::wstring> obj;
        VariantInit(&vtProp);
        wchar_t buf[1024];
        for (auto& item : queryName) {
            hr = pclsObj->Get(item.c_str(), 0, &vtProp, 0, 0);

            switch (vtProp.vt) {
            case VT_BSTR:
                obj[item] = vtProp.bstrVal;
                break;
            case VT_NULL:
            case VT_EMPTY:
                break;
            case VT_I4:
                wsprintf(buf, L"%d", vtProp.intVal);
                obj[item] = buf;
                break;
            default:
                wsprintf(buf, L"Unhandler type: %d\n", vtProp.vt);
                obj[item] = buf;
            }
        }

        outObj.push_back(obj);

        // Get the value of the Name property

        //std::wcout << " OS Name : " << vtProp.bstrVal << std::endl;
        VariantClear(&vtProp);

        pclsObj->Release();
    }

    return true;
}

bool WMIControler::ExecNotificationQueryAsync(std::string key, const wchar_t* WQLCommand, pfn_ASyncHandler cb) {
    HRESULT hres;

    ASyncInfo info;

    info.pSink = new EventSink;
    info.pSink->AddRef();

    info.pSink->RegCallback(cb);


    hres = CoCreateInstance(CLSID_UnsecuredApartment, NULL,
        CLSCTX_LOCAL_SERVER, IID_IUnsecuredApartment,
        (void**)&info.pUnsecApp);
    info.pUnsecApp->CreateObjectStub(info.pSink, &info.pStubUnk);


    info.pStubUnk->QueryInterface(IID_IWbemObjectSink,
        (void**)&info.pStubSink);

    // The ExecNotificationQueryAsync method will call
    // The EventQuery::Indicate method when an event occurs
    hres = pSvc->ExecNotificationQueryAsync(
        _bstr_t("WQL"),
        _bstr_t(WQLCommand),
        WBEM_FLAG_SEND_STATUS,
        NULL,
        info.pStubSink);

    // Check for errors.
    if (FAILED(hres))
    {
        return false;
    }

    info.cb = cb;
    info.command = WQLCommand;

    m_async_info[key] = info;
    return true;
}

void WMIControler::CancelExecNotificationQueryAsync(std::string key) {
    auto item = m_async_info.find(key);
    if (item == m_async_info.end()) {
        return;
    }

    pSvc->CancelAsyncCall(item->second.pStubSink);
    item->second.Release();
    m_async_info.erase(item);
}

