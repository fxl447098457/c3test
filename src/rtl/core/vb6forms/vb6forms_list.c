// vb6forms_list.c - vb6forms 模块拆分: ListBox/ComboBox + TextBox 特有属性
// 由 vb6forms.c 按控件/窗体功能家族拆分而来 (纯搬移, 零行为改动)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#endif

#include "vb6forms.h"
#include "vb6forms_internal.h"
#include <stdio.h>
#include <stdarg.h>

#include <stdlib.h>   /* malloc, free */
#include <oleauto.h>  /* SysAllocString, BSTR */
#include <olectl.h>   /* IPicture, OleLoadPicture, OLE_HANDLE */


// ============================================================
// ============================================================
// P13.3: ListBox/ComboBox properties
// ============================================================

// Helper: check if HWND is a ListBox
static int vb6_IsListBox(void* hwnd) {
    if (!hwnd) return 0;
    wchar_t cls[64];
    GetClassNameW((HWND)hwnd, cls, 64);
    // VB6 ListBox creates "ListBox" class, our RTL uses "LISTBOX"
    return (wcsicmp(cls, L"LISTBOX") == 0 || wcsicmp(cls, L"ListBox") == 0);
}

int vb6_GetListCount(void* hwnd) {
    if (!hwnd) return 0;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETCOUNT, 0, 0);
    }
    return (int)SendMessageW((HWND)hwnd, CB_GETCOUNT, 0, 0);
}

int vb6_GetListIndex(void* hwnd) {
    if (!hwnd) return -1;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETCURSEL, 0, 0);
    }
    return (int)SendMessageW((HWND)hwnd, CB_GETCURSEL, 0, 0);
}

void vb6_SetListIndex(void* hwnd, int index) {
    if (!hwnd) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_SETCURSEL, (WPARAM)index, 0);
    } else {
        SendMessageW((HWND)hwnd, CB_SETCURSEL, (WPARAM)index, 0);
    }
}

void* vb6_GetListItem(void* hwnd, int index) {
    if (!hwnd) return SysAllocString(L"");
    int len;
    WCHAR* buf;
    BSTR bstr;
    if (vb6_IsListBox(hwnd)) {
        len = (int)SendMessageW((HWND)hwnd, LB_GETTEXTLEN, (WPARAM)index, 0);
        if (len == LB_ERR) return SysAllocString(L"");
        buf = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
        if (!buf) return SysAllocString(L"");
        SendMessageW((HWND)hwnd, LB_GETTEXT, (WPARAM)index, (LPARAM)buf);
        bstr = SysAllocString(buf);
        free(buf);
    } else {
        len = (int)SendMessageW((HWND)hwnd, CB_GETLBTEXTLEN, (WPARAM)index, 0);
        if (len == CB_ERR) return SysAllocString(L"");
        buf = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
        if (!buf) return SysAllocString(L"");
        SendMessageW((HWND)hwnd, CB_GETLBTEXT, (WPARAM)index, (LPARAM)buf);
        bstr = SysAllocString(buf);
        free(buf);
    }
    return (void*)bstr;
}

void vb6_AddItem(void* hwnd, void* bstrItem) {
    if (!hwnd) return;
    BSTR bs = (BSTR)bstrItem;
    if (!bs) {
        bs = SysAllocString(L"");
    }
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_ADDSTRING, 0, (LPARAM)bs);
    } else {
        SendMessageW((HWND)hwnd, CB_ADDSTRING, 0, (LPARAM)bs);
    }
    if (bstrItem == NULL) {
        SysFreeString(bs);
    }
}

void vb6_RemoveItem(void* hwnd, int index) {
    if (!hwnd) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_DELETESTRING, (WPARAM)index, 0);
    } else {
        SendMessageW((HWND)hwnd, CB_DELETESTRING, (WPARAM)index, 0);
    }
}

void vb6_ClearList(void* hwnd) {
    if (!hwnd) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_RESETCONTENT, 0, 0);
    } else {
        SendMessageW((HWND)hwnd, CB_RESETCONTENT, 0, 0);
    }
}

// P14.4.3: ListBox/ComboBox extended properties
void vb6_SetListItem(void* hwnd, int index, void* bstrItem) {
    if (!hwnd) return;
    if (index < 0) return;
    const wchar_t* wstr = bstrItem ? (const wchar_t*)bstrItem : L"";
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_DELETESTRING, (WPARAM)index, 0);
        SendMessageW((HWND)hwnd, LB_INSERTSTRING, (WPARAM)index, (LPARAM)wstr);
    } else {
        SendMessageW((HWND)hwnd, CB_DELETESTRING, (WPARAM)index, 0);
        SendMessageW((HWND)hwnd, CB_INSERTSTRING, (WPARAM)index, (LPARAM)wstr);
    }
}

int vb6_GetSelected(void* hwnd, int index) {
    if (!hwnd || index < 0) return 0;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETSEL, (WPARAM)index, 0);
    }
    return 0;  /* ComboBox: use ListIndex instead */
}

void vb6_SetSelected(void* hwnd, int index, int selected) {
    if (!hwnd || index < 0) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_SETSEL, (WPARAM)(selected ? 1 : 0), (LPARAM)index);
    }
}

int32_t vb6_GetItemData(void* hwnd, int index) {
    if (!hwnd || index < 0) return 0;
    if (vb6_IsListBox(hwnd)) {
        return (int32_t)SendMessageW((HWND)hwnd, LB_GETITEMDATA, (WPARAM)index, 0);
    } else {
        return (int32_t)SendMessageW((HWND)hwnd, CB_GETITEMDATA, (WPARAM)index, 0);
    }
}

void vb6_SetItemData(void* hwnd, int index, int32_t data) {
    if (!hwnd || index < 0) return;
    if (vb6_IsListBox(hwnd)) {
        SendMessageW((HWND)hwnd, LB_SETITEMDATA, (WPARAM)index, (LPARAM)data);
    } else {
        SendMessageW((HWND)hwnd, CB_SETITEMDATA, (WPARAM)index, (LPARAM)data);
    }
}

int vb6_GetNewIndex(void* hwnd) {
    if (!hwnd) return -1;
    if (vb6_IsListBox(hwnd)) {
        return (int)SendMessageW((HWND)hwnd, LB_GETCOUNT, 0, 0) - 1;
    } else {
        return (int)SendMessageW((HWND)hwnd, CB_GETCOUNT, 0, 0) - 1;
    }
}

// ============================================================
// P13.4: TextBox-specific properties
// ============================================================

int vb6_GetMultiLine(void* hwnd) {
    if (!hwnd) return 0;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    return (style & ES_MULTILINE) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetMultiLine(void* hwnd, int multiline) {
    // Note: ES_MULTILINE cannot be changed after creation in Win32
    // This is a VB6 design-time only property; we store it but it has no effect
    if (!hwnd) return;
    // Store in window property for consistency
    SetPropW((HWND)hwnd, L"VB6_MultiLine", (HANDLE)(INT_PTR)(multiline ? -1 : 0));
}

int vb6_GetScrollBars(void* hwnd) {
    if (!hwnd) return 0;
    HANDLE hProp = GetPropW((HWND)hwnd, L"VB6_ScrollBars");
    if (hProp) return (int)(INT_PTR)hProp;
    // Infer from style
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    int result = 0;
    if (style & WS_HSCROLL) result |= 1;
    if (style & WS_VSCROLL) result |= 2;
    return result;
}

void vb6_SetScrollBars(void* hwnd, int scrollbars) {
    // ScrollBars is design-time only in VB6; store as property
    if (!hwnd) return;
    SetPropW((HWND)hwnd, L"VB6_ScrollBars", (HANDLE)(INT_PTR)scrollbars);
}

int vb6_GetMaxLength(void* hwnd) {
    if (!hwnd) return 0;
    return (int)SendMessageW((HWND)hwnd, EM_GETLIMITTEXT, 0, 0);
}

void vb6_SetMaxLength(void* hwnd, int maxlength) {
    if (!hwnd) return;
    if (maxlength <= 0) maxlength = 0x7FFFFFFE;  // VB6: 0 = unlimited
    SendMessageW((HWND)hwnd, EM_SETLIMITTEXT, (WPARAM)maxlength, 0);
}

void* vb6_GetPasswordChar(void* hwnd) {
    if (!hwnd) return SysAllocString(L"");
    WCHAR ch = (WCHAR)SendMessageW((HWND)hwnd, EM_GETPASSWORDCHAR, 0, 0);
    if (ch == 0) return SysAllocString(L"");
    WCHAR buf[2] = { ch, 0 };
    return SysAllocString(buf);
}

void vb6_SetPasswordChar(void* hwnd, void* bstrChar) {
    if (!hwnd) return;
    WCHAR ch = 0;
    if (bstrChar) {
        BSTR bs = (BSTR)bstrChar;
        if (SysStringLen(bs) > 0) ch = bs[0];
    }
    SendMessageW((HWND)hwnd, EM_SETPASSWORDCHAR, (WPARAM)ch, 0);
    InvalidateRect((HWND)hwnd, NULL, TRUE);
}

int vb6_GetLocked(void* hwnd) {
    if (!hwnd) return 0;
    LONG style = GetWindowLongW((HWND)hwnd, GWL_STYLE);
    // Locked = read-only in Win32 terms
    return (style & ES_READONLY) ? -1 : 0;  // VB6: True=-1
}

void vb6_SetLocked(void* hwnd, int locked) {
    if (!hwnd) return;
    SendMessageW((HWND)hwnd, EM_SETREADONLY, (WPARAM)(locked ? TRUE : FALSE), 0);
}
