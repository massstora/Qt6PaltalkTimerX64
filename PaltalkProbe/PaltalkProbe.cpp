#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <UIAutomation.h>
#include <atlbase.h>
#include <psapi.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uiautomationcore.lib")
#pragma comment(lib, "psapi.lib")

static CComPtr<IUIAutomation> gUia;
static std::wofstream gOut;

static std::wstring Bstr(const CComBSTR& value) { return value.m_str ? static_cast<const wchar_t*>(value) : L""; }

static std::wstring WinText(HWND hwnd)
{
    wchar_t text[512] = {};
    GetWindowTextW(hwnd, text, _countof(text));
    return text;
}

static std::wstring WinClass(HWND hwnd)
{
    wchar_t klass[256] = {};
    GetClassNameW(hwnd, klass, _countof(klass));
    return klass;
}

static std::wstring RectText(RECT rc)
{
    std::wstringstream ss;
    ss << L"(" << rc.left << L"," << rc.top << L")-(" << rc.right << L"," << rc.bottom
       << L") " << (rc.right - rc.left) << L"x" << (rc.bottom - rc.top);
    return ss.str();
}

static std::wstring ExePath(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) return L"";

    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    return path;
}

static const wchar_t* ControlTypeName(CONTROLTYPEID id)
{
    switch (id) {
    case UIA_ButtonControlTypeId: return L"Button";
    case UIA_EditControlTypeId: return L"Edit";
    case UIA_ListControlTypeId: return L"List";
    case UIA_ListItemControlTypeId: return L"ListItem";
    case UIA_MenuControlTypeId: return L"Menu";
    case UIA_MenuItemControlTypeId: return L"MenuItem";
    case UIA_TextControlTypeId: return L"Text";
    case UIA_WindowControlTypeId: return L"Window";
    case UIA_PaneControlTypeId: return L"Pane";
    case UIA_GroupControlTypeId: return L"Group";
    case UIA_HeaderControlTypeId: return L"Header";
    case UIA_HeaderItemControlTypeId: return L"HeaderItem";
    case UIA_DataGridControlTypeId: return L"DataGrid";
    case UIA_DataItemControlTypeId: return L"DataItem";
    default: return L"Other";
    }
}

static std::wstring Patterns(IUIAutomationElement* el)
{
    struct Pattern { PATTERNID id; const wchar_t* name; };
    Pattern patterns[] = {
        { UIA_InvokePatternId, L"Invoke" },
        { UIA_ValuePatternId, L"Value" },
        { UIA_LegacyIAccessiblePatternId, L"Legacy" },
        { UIA_SelectionPatternId, L"Selection" },
        { UIA_SelectionItemPatternId, L"SelectionItem" },
        { UIA_TextPatternId, L"Text" },
        { UIA_TogglePatternId, L"Toggle" },
        { UIA_ExpandCollapsePatternId, L"ExpandCollapse" },
        { UIA_ScrollPatternId, L"Scroll" },
    };

    std::wstring out;
    for (auto& pattern : patterns) {
        CComPtr<IUnknown> unk;
        if (SUCCEEDED(el->GetCurrentPattern(pattern.id, &unk)) && unk) {
            if (!out.empty()) out += L",";
            out += pattern.name;
        }
    }
    return out;
}

static void DumpHwndTreeRecursive(HWND hwnd, int depth)
{
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    gOut << std::wstring(depth * 2, L' ')
         << L"- hwnd=0x" << std::hex << reinterpret_cast<uintptr_t>(hwnd) << std::dec
         << L" class=\"" << WinClass(hwnd) << L"\""
         << L" text=\"" << WinText(hwnd) << L"\""
         << L" pid=" << pid << L" tid=" << tid
         << L" visible=" << IsWindowVisible(hwnd)
         << L" rect=" << RectText(rc) << L"\n";

    for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        DumpHwndTreeRecursive(child, depth + 1);
    }
}

static void DumpHwndTree(HWND hwnd, const std::wstring& title)
{
    if (!hwnd) return;
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    gOut << L"\n== HWND " << title << L" ==\n";
    gOut << L"hwnd=0x" << std::hex << reinterpret_cast<uintptr_t>(hwnd) << std::dec
         << L" class=\"" << WinClass(hwnd) << L"\""
         << L" text=\"" << WinText(hwnd) << L"\""
         << L" pid=" << pid << L" tid=" << tid
         << L" exe=\"" << ExePath(hwnd) << L"\""
         << L" visible=" << IsWindowVisible(hwnd)
         << L" rect=" << RectText(rc) << L"\n";
    for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        DumpHwndTreeRecursive(child, 1);
    }
}

static void DumpUiaElement(IUIAutomationElement* el, int depth, int& count)
{
    if (!el || depth > 8 || count > 700) return;
    ++count;

    CComBSTR name, klass, autoId;
    CONTROLTYPEID type = 0;
    RECT rc{};
    BOOL offscreen = FALSE;
    BOOL enabled = FALSE;
    el->get_CurrentName(&name);
    el->get_CurrentClassName(&klass);
    el->get_CurrentAutomationId(&autoId);
    el->get_CurrentControlType(&type);
    el->get_CurrentBoundingRectangle(&rc);
    el->get_CurrentIsOffscreen(&offscreen);
    el->get_CurrentIsEnabled(&enabled);

    gOut << std::wstring(depth * 2, L' ')
         << L"- " << ControlTypeName(type)
         << L" name=\"" << Bstr(name) << L"\""
         << L" class=\"" << Bstr(klass) << L"\""
         << L" automationId=\"" << Bstr(autoId) << L"\""
         << L" rect=" << RectText(rc)
         << L" enabled=" << enabled
         << L" offscreen=" << offscreen
         << L" patterns=[" << Patterns(el) << L"]\n";

    CComPtr<IUIAutomationTreeWalker> walker;
    if (FAILED(gUia->get_ControlViewWalker(&walker)) || !walker) return;

    CComPtr<IUIAutomationElement> child;
    if (FAILED(walker->GetFirstChildElement(el, &child))) return;
    while (child && count <= 700) {
        DumpUiaElement(child, depth + 1, count);
        CComPtr<IUIAutomationElement> next;
        if (FAILED(walker->GetNextSiblingElement(child, &next))) break;
        child = next;
    }
}

static void DumpUiaTree(HWND hwnd, const std::wstring& title)
{
    if (!hwnd) return;
    CComPtr<IUIAutomationElement> el;
    gOut << L"\n== UIA " << title << L" ==\n";
    if (FAILED(gUia->ElementFromHandle(hwnd, &el)) || !el) {
        gOut << L"ElementFromHandle failed.\n";
        return;
    }
    int count = 0;
    DumpUiaElement(el, 0, count);
    if (count > 700) gOut << L"... stopped after 700 elements\n";
}

static void WaitMouseUp()
{
    while (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

static HWND Capture(const std::wstring& label, HWND* clicked, POINT* clickedPoint = nullptr)
{
    if (clicked) *clicked = nullptr;
    if (clickedPoint) *clickedPoint = {};
    std::wcout << L"\n" << label << L"\nClick target area, or press Esc to skip.\n";
    WaitMouseUp();
    for (;;) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return nullptr;
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            POINT pt{};
            GetCursorPos(&pt);
            HWND underMouse = WindowFromPoint(pt);
            HWND root = GetAncestor(underMouse, GA_ROOT);
            if (clicked) *clicked = underMouse;
            if (clickedPoint) *clickedPoint = pt;
            WaitMouseUp();
            gOut << L"\n\n######## " << label << L" ########\n";
            gOut << L"point=(" << pt.x << L"," << pt.y << L") clicked=0x"
                 << std::hex << reinterpret_cast<uintptr_t>(underMouse)
                 << L" root=0x" << reinterpret_cast<uintptr_t>(root) << std::dec << L"\n";
            DumpHwndTree(underMouse, label + L" clicked");
            DumpUiaTree(underMouse, label + L" clicked");
            if (root && root != underMouse) {
                DumpHwndTree(root, label + L" root");
                DumpUiaTree(root, label + L" root");
            }
            return root;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

static void ContextMenuProbe()
{
    std::wstring answer;
    std::wcout << L"\nOptional: type Y then Enter to right-click a user row and record the menu: ";
    std::getline(std::wcin, answer);
    if (answer.empty() || (answer[0] != L'y' && answer[0] != L'Y')) return;

    HWND clicked = nullptr;
    POINT pt{};
    Capture(L"Context menu target row", &clicked, &pt);
    if (!clicked) return;

    SetCursorPos(pt.x, pt.y);
    mouse_event(MOUSEEVENTF_RIGHTDOWN, pt.x, pt.y, 0, 0);
    mouse_event(MOUSEEVENTF_RIGHTUP, pt.x, pt.y, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    HWND fg = GetForegroundWindow();
    HWND atPoint = WindowFromPoint(pt);
    gOut << L"\n\n######## Context menu after right-click ########\n";
    DumpHwndTree(fg, L"context foreground");
    DumpUiaTree(fg, L"context foreground");
    if (atPoint && atPoint != fg) {
        DumpHwndTree(atPoint, L"context at point");
        DumpUiaTree(atPoint, L"context at point");
    }

    INPUT esc{};
    esc.type = INPUT_KEYBOARD;
    esc.ki.wVk = VK_ESCAPE;
    SendInput(1, &esc, sizeof(esc));
    esc.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &esc, sizeof(esc));
}

int wmain()
{
    SetConsoleOutputCP(CP_UTF8);
    std::wcout << L"Paltalk Probe\n";
    std::wcout << L"Run once while someone is on mic, then again with a different/no mic user.\n";

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&gUia)))) return 2;

    gOut.open("paltalk-probe-report.txt", std::ios::trunc);
    if (!gOut) return 3;
    gOut << L"Paltalk Probe Report\n";

    std::wstring line;
    std::wcout << L"\nPress Enter to begin.";
    std::getline(std::wcin, line);

    HWND clicked = nullptr;
    Capture(L"Room window", &clicked);
    Capture(L"Mic/current speaker area", &clicked);
    Capture(L"Member/user list area", &clicked);
    Capture(L"Chat input area", &clicked);
    ContextMenuProbe();

    gOut.close();
    gUia.Release();
    CoUninitialize();

    std::wcout << L"\nWrote paltalk-probe-report.txt next to this EXE.\n";
    std::wcout << L"Send that file back here. Press Enter to exit.";
    std::getline(std::wcin, line);
    return 0;
}
