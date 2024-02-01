/*        DirectInput
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998,1999 Lionel Ulmer
 * Copyright 2000-2002 TransGaming Technologies Inc.
 * Copyright 2007 Vitaliy Margolen
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
/* Status:
 *
 * - Tomb Raider 2 Demo:
 *   Playable using keyboard only.
 * - WingCommander Prophecy Demo:
 *   Doesn't get Input Focus.
 *
 * - Fallout : works great in X and DGA mode
 */

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#define COBJMACROS
#define NONAMELESSUNION

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "objbase.h"
#include "rpcproxy.h"
#include "devguid.h"
#include "initguid.h"
#include "dinputd.h"

#include "dinput_private.h"
#include "device_private.h"

#include "wine/asm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dinput);

static const IDirectInput7WVtbl dinput7_vtbl;
static const IDirectInput8WVtbl dinput8_vtbl;
static const IDirectInputJoyConfig8Vtbl joy_config_vtbl;

static inline struct dinput *impl_from_IDirectInput7W( IDirectInput7W *iface )
{
    return CONTAINING_RECORD( iface, struct dinput, IDirectInput7W_iface );
}

static inline struct dinput *impl_from_IDirectInput8W( IDirectInput8W *iface )
{
    return CONTAINING_RECORD( iface, struct dinput, IDirectInput8W_iface );
}

static inline struct dinput_device *impl_from_IDirectInputDevice8W( IDirectInputDevice8W *iface )
{
    return CONTAINING_RECORD( iface, struct dinput_device, IDirectInputDevice8W_iface );
}

HINSTANCE DINPUT_instance;

static HWND di_em_win;

static HANDLE dinput_thread;
static DWORD dinput_thread_id;

static CRITICAL_SECTION dinput_hook_crit;
static CRITICAL_SECTION_DEBUG dinput_critsect_debug =
{
    0, 0, &dinput_hook_crit,
    { &dinput_critsect_debug.ProcessLocksList, &dinput_critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": dinput_hook_crit") }
};
static CRITICAL_SECTION dinput_hook_crit = { &dinput_critsect_debug, -1, 0, 0, 0, 0 };

static struct list acquired_mouse_list = LIST_INIT( acquired_mouse_list );
static struct list acquired_rawmouse_list = LIST_INIT( acquired_rawmouse_list );
static struct list acquired_keyboard_list = LIST_INIT( acquired_keyboard_list );
static struct list acquired_device_list = LIST_INIT( acquired_device_list );

static HRESULT initialize_directinput_instance( struct dinput *impl, DWORD version );
static void uninitialize_directinput_instance( struct dinput *impl );

void dinput_hooks_acquire_device( IDirectInputDevice8W *iface )
{
    struct dinput_device *impl = impl_from_IDirectInputDevice8W( iface );

    EnterCriticalSection( &dinput_hook_crit );
    if (IsEqualGUID( &impl->guid, &GUID_SysMouse ))
        list_add_tail( impl->use_raw_input ? &acquired_rawmouse_list : &acquired_mouse_list, &impl->entry );
    else if (IsEqualGUID( &impl->guid, &GUID_SysKeyboard ))
        list_add_tail( &acquired_keyboard_list, &impl->entry );
    else
        list_add_tail( &acquired_device_list, &impl->entry );
    LeaveCriticalSection( &dinput_hook_crit );
}

void dinput_hooks_unacquire_device( IDirectInputDevice8W *iface )
{
    struct dinput_device *impl = impl_from_IDirectInputDevice8W( iface );

    EnterCriticalSection( &dinput_hook_crit );
    list_remove( &impl->entry );
    LeaveCriticalSection( &dinput_hook_crit );
}

static void dinput_device_internal_unacquire( IDirectInputDevice8W *iface )
{
    struct dinput_device *impl = impl_from_IDirectInputDevice8W( iface );

    TRACE( "iface %p.\n", iface );

    EnterCriticalSection( &impl->crit );
    if (impl->status == STATUS_ACQUIRED)
    {
        impl->vtbl->unacquire( iface );
        impl->status = STATUS_UNACQUIRED;
        list_remove( &impl->entry );
    }
    LeaveCriticalSection( &impl->crit );
}

static HRESULT dinput_create( IUnknown **out )
{
    struct dinput *impl;

    if (!(impl = calloc( 1, sizeof(struct dinput) ))) return E_OUTOFMEMORY;
    impl->IDirectInput7A_iface.lpVtbl = &dinput7_a_vtbl;
    impl->IDirectInput7W_iface.lpVtbl = &dinput7_vtbl;
    impl->IDirectInput8A_iface.lpVtbl = &dinput8_a_vtbl;
    impl->IDirectInput8W_iface.lpVtbl = &dinput8_vtbl;
    impl->IDirectInputJoyConfig8_iface.lpVtbl = &joy_config_vtbl;
    impl->ref = 1;

#if DIRECTINPUT_VERSION == 0x0700
    *out = (IUnknown *)&impl->IDirectInput7W_iface;
#else
    *out = (IUnknown *)&impl->IDirectInput8W_iface;
#endif
    return DI_OK;
}

/******************************************************************************
 *    DirectInputCreateEx (DINPUT.@)
 */
HRESULT WINAPI DirectInputCreateEx( HINSTANCE hinst, DWORD version, REFIID iid, void **out, IUnknown *outer )
{
    IUnknown *unknown;
    HRESULT hr;

    TRACE( "hinst %p, version %#lx, iid %s, out %p, outer %p.\n", hinst, version, debugstr_guid( iid ), out, outer );

    if (!IsEqualGUID( &IID_IDirectInputA, iid ) &&
        !IsEqualGUID( &IID_IDirectInputW, iid ) &&
        !IsEqualGUID( &IID_IDirectInput2A, iid ) &&
        !IsEqualGUID( &IID_IDirectInput2W, iid ) &&
        !IsEqualGUID( &IID_IDirectInput7A, iid ) &&
        !IsEqualGUID( &IID_IDirectInput7W, iid ))
        return DIERR_NOINTERFACE;

    if (FAILED(hr = dinput_create( &unknown ))) return hr;
    hr = IUnknown_QueryInterface( unknown, iid, out );
    IUnknown_Release( unknown );
    if (FAILED(hr)) return hr;

    if (outer || FAILED(hr = IDirectInput7_Initialize( (IDirectInput7W *)unknown, hinst, version )))
    {
        IUnknown_Release( unknown );
        *out = NULL;
        return hr;
    }

    return DI_OK;
}

/******************************************************************************
 *    DirectInput8Create (DINPUT8.@)
 */
HRESULT WINAPI DECLSPEC_HOTPATCH DirectInput8Create( HINSTANCE hinst, DWORD version, REFIID iid, void **out, IUnknown *outer )
{
    IUnknown *unknown;
    HRESULT hr;

    TRACE( "hinst %p, version %#lx, iid %s, out %p, outer %p.\n", hinst, version, debugstr_guid( iid ), out, outer );

    if (!out) return E_POINTER;

    if (!IsEqualGUID( &IID_IDirectInput8A, iid ) &&
        !IsEqualGUID( &IID_IDirectInput8W, iid ) &&
        !IsEqualGUID( &IID_IUnknown, iid ))
    {
        *out = NULL;
        return DIERR_NOINTERFACE;
    }

    if (FAILED(hr = dinput_create( &unknown ))) return hr;
    hr = IUnknown_QueryInterface( unknown, iid, out );
    IUnknown_Release( unknown );
    if (FAILED(hr)) return hr;

    if (outer || FAILED(hr = IDirectInput8_Initialize( (IDirectInput8W *)unknown, hinst, version )))
    {
        IUnknown_Release( (IUnknown *)unknown );
        *out = NULL;
        return hr;
    }

    return S_OK;
}

/******************************************************************************
 *    DirectInputCreateA (DINPUT.@)
 */
HRESULT WINAPI DECLSPEC_HOTPATCH DirectInputCreateA( HINSTANCE hinst, DWORD version, IDirectInputA **out, IUnknown *outer )
{
    return DirectInputCreateEx( hinst, version, &IID_IDirectInput7A, (void **)out, outer );
}

/******************************************************************************
 *    DirectInputCreateW (DINPUT.@)
 */
HRESULT WINAPI DECLSPEC_HOTPATCH DirectInputCreateW( HINSTANCE hinst, DWORD version, IDirectInputW **out, IUnknown *outer )
{
    return DirectInputCreateEx( hinst, version, &IID_IDirectInput7W, (void **)out, outer );
}

static DWORD diactionformat_priorityW( DIACTIONFORMATW *action_format, DWORD genre )
{
    int i;
    DWORD priorityFlags = 0;

    /* If there's at least one action for the device it's priority 1 */
    for (i = 0; i < action_format->dwNumActions; i++)
        if ((action_format->rgoAction[i].dwSemantic & genre) == genre)
            priorityFlags |= DIEDBS_MAPPEDPRI1;

    return priorityFlags;
}

#if defined __i386__ && defined _MSC_VER
__declspec(naked) BOOL enum_callback_wrapper(void *callback, const void *instance, void *ref)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push [ebp+16]
        push [ebp+12]
        call [ebp+8]
        leave
        ret
    }
}
#elif defined __i386__ && defined __GNUC__
extern BOOL enum_callback_wrapper(void *callback, const void *instance, void *ref);
__ASM_GLOBAL_FUNC( enum_callback_wrapper,
    "pushl %ebp\n\t"
    __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
    __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
    "movl %esp,%ebp\n\t"
    __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
    "pushl 16(%ebp)\n\t"
    "pushl 12(%ebp)\n\t"
    "call *8(%ebp)\n\t"
    "leave\n\t"
    __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
    __ASM_CFI(".cfi_same_value %ebp\n\t")
    "ret" )
#else
#define enum_callback_wrapper(callback, instance, ref) (callback)((instance), (ref))
#endif

/******************************************************************************
 *    IDirectInputW_EnumDevices
 */
static HRESULT WINAPI dinput7_EnumDevices( IDirectInput7W *iface, DWORD type, LPDIENUMDEVICESCALLBACKW callback,
                                           void *context, DWORD flags )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );

    TRACE( "iface %p, type %#lx, callback %p, context %p, flags %#lx.\n", iface, type, callback, context, flags );

    if (!callback) return DIERR_INVALIDPARAM;

    if (type > DIDEVTYPE_JOYSTICK) return DIERR_INVALIDPARAM;
    if (flags & ~(DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK | DIEDFL_INCLUDEALIASES | DIEDFL_INCLUDEPHANTOMS))
        return DIERR_INVALIDPARAM;

    return IDirectInput8_EnumDevices( &impl->IDirectInput8W_iface, type, callback, context, flags );
}

static ULONG WINAPI dinput7_AddRef( IDirectInput7W *iface )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI dinput7_Release( IDirectInput7W *iface )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );

    if (ref == 0)
    {
        uninitialize_directinput_instance( impl );
        free( impl );
    }

    return ref;
}

static HRESULT WINAPI dinput7_QueryInterface( IDirectInput7W *iface, REFIID iid, void **out )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (!iid || !out) return E_POINTER;

    *out = NULL;

#if DIRECTINPUT_VERSION == 0x0700
    if (IsEqualGUID( &IID_IDirectInputA, iid ) ||
        IsEqualGUID( &IID_IDirectInput2A, iid ) ||
        IsEqualGUID( &IID_IDirectInput7A, iid ))
        *out = &impl->IDirectInput7A_iface;
    else if (IsEqualGUID( &IID_IUnknown, iid ) ||
             IsEqualGUID( &IID_IDirectInputW, iid ) ||
             IsEqualGUID( &IID_IDirectInput2W, iid ) ||
             IsEqualGUID( &IID_IDirectInput7W, iid ))
        *out = &impl->IDirectInput7W_iface;

#else
    if (IsEqualGUID( &IID_IDirectInput8A, iid ))
        *out = &impl->IDirectInput8A_iface;
    else if (IsEqualGUID( &IID_IUnknown, iid ) ||
             IsEqualGUID( &IID_IDirectInput8W, iid ))
        *out = &impl->IDirectInput8W_iface;

#endif

    if (IsEqualGUID( &IID_IDirectInputJoyConfig8, iid ))
        *out = &impl->IDirectInputJoyConfig8_iface;

    if (*out)
    {
        IUnknown_AddRef( (IUnknown *)*out );
        return DI_OK;
    }

    WARN( "Unsupported interface: %s\n", debugstr_guid( iid ) );
    return E_NOINTERFACE;
}

static LRESULT WINAPI di_em_win_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    struct dinput_device *impl;
    RAWINPUT ri;
    UINT size = sizeof(ri);
    int rim = GET_RAWINPUT_CODE_WPARAM( wparam );

    TRACE( "%p %d %Ix %Ix\n", hwnd, msg, wparam, lparam );

    if (msg == WM_INPUT && (rim == RIM_INPUT || rim == RIM_INPUTSINK))
    {
        size = GetRawInputData( (HRAWINPUT)lparam, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER) );
        if (size == (UINT)-1 || size < sizeof(RAWINPUTHEADER))
            WARN( "Unable to read raw input data\n" );
        else if (ri.header.dwType == RIM_TYPEMOUSE)
        {
            EnterCriticalSection( &dinput_hook_crit );
            LIST_FOR_EACH_ENTRY( impl, &acquired_rawmouse_list, struct dinput_device, entry )
                dinput_mouse_rawinput_hook( &impl->IDirectInputDevice8W_iface, wparam, lparam, &ri );
            LeaveCriticalSection( &dinput_hook_crit );
        }
    }

    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

static void register_di_em_win_class(void)
{
    WNDCLASSEXW class;

    memset(&class, 0, sizeof(class));
    class.cbSize = sizeof(class);
    class.lpfnWndProc = di_em_win_wndproc;
    class.hInstance = DINPUT_instance;
    class.lpszClassName = L"DIEmWin";

    if (!RegisterClassExW( &class ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        WARN( "Unable to register message window class\n" );
}

static void unregister_di_em_win_class(void)
{
    if (!UnregisterClassW( L"DIEmWin", NULL ) && GetLastError() != ERROR_CLASS_DOES_NOT_EXIST)
        WARN( "Unable to unregister message window class\n" );
}

static HRESULT initialize_directinput_instance( struct dinput *impl, DWORD version )
{
    if (!impl->initialized)
    {
        impl->dwVersion = version;
        impl->evsequence = 1;

        list_init( &impl->device_players );

        impl->initialized = TRUE;
    }

    return DI_OK;
}

static void uninitialize_directinput_instance( struct dinput *impl )
{
    if (impl->initialized)
    {
        struct DevicePlayer *device_player, *device_player2;

        LIST_FOR_EACH_ENTRY_SAFE ( device_player, device_player2, &impl->device_players, struct DevicePlayer, entry )
            free( device_player );

        impl->initialized = FALSE;
    }
}

enum directinput_versions
{
    DIRECTINPUT_VERSION_300 = 0x0300,
    DIRECTINPUT_VERSION_500 = 0x0500,
    DIRECTINPUT_VERSION_50A = 0x050A,
    DIRECTINPUT_VERSION_5B2 = 0x05B2,
    DIRECTINPUT_VERSION_602 = 0x0602,
    DIRECTINPUT_VERSION_61A = 0x061A,
    DIRECTINPUT_VERSION_700 = 0x0700,
};

static HRESULT WINAPI dinput7_Initialize( IDirectInput7W *iface, HINSTANCE hinst, DWORD version )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );

    TRACE( "iface %p, hinst %p, version %#lx.\n", iface, hinst, version );

    if (!hinst)
        return DIERR_INVALIDPARAM;
    else if (version == 0)
        return DIERR_NOTINITIALIZED;
    else if (version > DIRECTINPUT_VERSION_700)
        return DIERR_OLDDIRECTINPUTVERSION;
    else if (version != DIRECTINPUT_VERSION_300 && version != DIRECTINPUT_VERSION_500 &&
             version != DIRECTINPUT_VERSION_50A && version != DIRECTINPUT_VERSION_5B2 &&
             version != DIRECTINPUT_VERSION_602 && version != DIRECTINPUT_VERSION_61A &&
             version != DIRECTINPUT_VERSION_700 && version != DIRECTINPUT_VERSION)
        return DIERR_BETADIRECTINPUTVERSION;

    return initialize_directinput_instance( impl, version );
}

static HRESULT WINAPI dinput7_GetDeviceStatus( IDirectInput7W *iface, const GUID *guid )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );
    HRESULT hr;
    IDirectInputDeviceW *device;

    TRACE( "iface %p, guid %s.\n", iface, debugstr_guid( guid ) );

    if (!guid) return E_POINTER;
    if (!impl->initialized) return DIERR_NOTINITIALIZED;

    hr = IDirectInput_CreateDevice( iface, guid, &device, NULL );
    if (hr != DI_OK) return DI_NOTATTACHED;

    IUnknown_Release( device );

    return DI_OK;
}

static HRESULT WINAPI dinput7_RunControlPanel( IDirectInput7W *iface, HWND owner, DWORD flags )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );
    WCHAR control_exe[] = {L"control.exe"};
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi;

    TRACE( "iface %p, owner %p, flags %#lx.\n", iface, owner, flags );

    if (owner && !IsWindow( owner )) return E_HANDLE;
    if (flags) return DIERR_INVALIDPARAM;
    if (!impl->initialized) return DIERR_NOTINITIALIZED;

    if (!CreateProcessW( NULL, control_exe, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi ))
        return HRESULT_FROM_WIN32(GetLastError());

    return DI_OK;
}

static HRESULT WINAPI dinput7_FindDevice( IDirectInput7W *iface, const GUID *guid, const WCHAR *name, GUID *instance_guid )
{
    FIXME( "iface %p, guid %s, name %s, instance_guid %s stub!\n", iface, debugstr_guid( guid ),
           debugstr_w(name), debugstr_guid( instance_guid ) );
    return DI_OK;
}

static HRESULT WINAPI dinput7_CreateDeviceEx( IDirectInput7W *iface, const GUID *guid,
                                              REFIID iid, void **out, IUnknown *outer )
{
    struct dinput *impl = impl_from_IDirectInput7W( iface );
    IDirectInputDevice8W *device;
    HRESULT hr;

    TRACE( "iface %p, guid %s, iid %s, out %p, outer %p.\n", iface, debugstr_guid( guid ),
           debugstr_guid( iid ), out, outer );

    if (!out) return E_POINTER;
    *out = NULL;

    if (!guid) return E_POINTER;
    if (!impl->initialized) return DIERR_NOTINITIALIZED;

    if (IsEqualGUID( &GUID_SysKeyboard, guid )) hr = keyboard_create_device( impl, guid, &device );
    else if (IsEqualGUID( &GUID_SysMouse, guid )) hr = mouse_create_device( impl, guid, &device );
    else if (IsEqualGUID( &GUID_Joystick, guid )) hr = gamepad_create_device( impl, guid, &device );
    else hr = hid_joystick_create_device( impl, guid, &device );

    if (FAILED(hr)) return hr;

    if (FAILED(hr = dinput_device_init_device_format( device )))
    {
        IDirectInputDevice8_Release( device );
        return hr;
    }

    hr = IDirectInputDevice8_QueryInterface( device, iid, out );
    IDirectInputDevice8_Release( device );
    return hr;
}

static HRESULT WINAPI dinput7_CreateDevice( IDirectInput7W *iface, const GUID *guid,
                                            IDirectInputDeviceW **out, IUnknown *outer )
{
    return IDirectInput7_CreateDeviceEx( iface, guid, &IID_IDirectInputDeviceW, (void **)out, outer );
}

/*******************************************************************************
 *      DirectInput8
 */

static ULONG WINAPI dinput8_AddRef( IDirectInput8W *iface )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    return IDirectInput7_AddRef( &impl->IDirectInput7W_iface );
}

static HRESULT WINAPI dinput8_QueryInterface( IDirectInput8W *iface, REFIID iid, void **out )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    return IDirectInput7_QueryInterface( &impl->IDirectInput7W_iface, iid, out );
}

static ULONG WINAPI dinput8_Release( IDirectInput8W *iface )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    return IDirectInput7_Release( &impl->IDirectInput7W_iface );
}

static HRESULT WINAPI dinput8_CreateDevice( IDirectInput8W *iface, const GUID *guid,
                                            IDirectInputDevice8W **out, IUnknown *outer )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    return IDirectInput7_CreateDeviceEx( &impl->IDirectInput7W_iface, guid,
                                         &IID_IDirectInputDevice8W, (void **)out, outer );
}

static BOOL try_enum_device( DWORD type, LPDIENUMDEVICESCALLBACKW callback,
                             DIDEVICEINSTANCEW *instance, void *context, DWORD flags )
{
    if (type && (instance->dwDevType & 0xff) != type) return DIENUM_CONTINUE;
    if ((flags & DIEDFL_FORCEFEEDBACK) && IsEqualGUID( &instance->guidFFDriver, &GUID_NULL ))
        return DIENUM_CONTINUE;
    return enum_callback_wrapper( callback, instance, context );
}

static HRESULT WINAPI dinput8_EnumDevices( IDirectInput8W *iface, DWORD type, LPDIENUMDEVICESCALLBACKW callback, void *context,
                                           DWORD flags )
{
    DIDEVICEINSTANCEW instance = {.dwSize = sizeof(DIDEVICEINSTANCEW)};
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    DWORD device_class = 0, device_type = 0;
    unsigned int i = 0;
    HRESULT hr;

    TRACE( "iface %p, type %#lx, callback %p, context %p, flags %#lx.\n", iface, type, callback, context, flags );

    if (!callback) return DIERR_INVALIDPARAM;

    if ((type > DI8DEVCLASS_GAMECTRL && type < DI8DEVTYPE_DEVICE) || type > DI8DEVTYPE_SUPPLEMENTAL)
        return DIERR_INVALIDPARAM;
    if (flags & ~(DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK | DIEDFL_INCLUDEALIASES |
                  DIEDFL_INCLUDEPHANTOMS | DIEDFL_INCLUDEHIDDEN))
        return DIERR_INVALIDPARAM;

    if (!impl->initialized) return DIERR_NOTINITIALIZED;

    if (type <= DI8DEVCLASS_GAMECTRL) device_class = type;
    else device_type = type;

    if (device_class == DI8DEVCLASS_ALL || device_class == DI8DEVCLASS_POINTER)
    {
        hr = mouse_enum_device( type, flags, &instance, impl->dwVersion );
        if (hr == DI_OK && try_enum_device( device_type, callback, &instance, context, flags ) == DIENUM_STOP)
            return DI_OK;
    }

    if (device_class == DI8DEVCLASS_ALL || device_class == DI8DEVCLASS_KEYBOARD)
    {
        hr = keyboard_enum_device( type, flags, &instance, impl->dwVersion );
        if (hr == DI_OK && try_enum_device( device_type, callback, &instance, context, flags ) == DIENUM_STOP)
            return DI_OK;
    }

    if (device_class == DI8DEVCLASS_ALL || device_class == DI8DEVCLASS_GAMECTRL)
    {
        do
        {
            hr = hid_joystick_enum_device( type, flags, &instance, impl->dwVersion, i++ );
            if (hr == DI_OK && try_enum_device( device_type, callback, &instance, context, flags ) == DIENUM_STOP)
                return DI_OK;
        } while (SUCCEEDED(hr));
        
        hr = gamepad_enum_device( type, flags, &instance, impl->dwVersion );
        if (hr == DI_OK && try_enum_device( device_type, callback, &instance, context, flags ) == DIENUM_STOP)
            return DI_OK;        
    }

    return DI_OK;
}

static HRESULT WINAPI dinput8_GetDeviceStatus( IDirectInput8W *iface, const GUID *guid )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    return IDirectInput7_GetDeviceStatus( &impl->IDirectInput7W_iface, guid );
}

static HRESULT WINAPI dinput8_RunControlPanel( IDirectInput8W *iface, HWND owner, DWORD flags )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    return IDirectInput7_RunControlPanel( &impl->IDirectInput7W_iface, owner, flags );
}

static HRESULT WINAPI dinput8_Initialize( IDirectInput8W *iface, HINSTANCE hinst, DWORD version )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );

    TRACE( "iface %p, hinst %p, version %#lx.\n", iface, hinst, version );

    if (!hinst)
        return DIERR_INVALIDPARAM;
    else if (version == 0)
        return DIERR_NOTINITIALIZED;
    else if (version < DIRECTINPUT_VERSION)
        return DIERR_BETADIRECTINPUTVERSION;
    else if (version > DIRECTINPUT_VERSION)
        return DIERR_OLDDIRECTINPUTVERSION;

    return initialize_directinput_instance( impl, version );
}

static HRESULT WINAPI dinput8_FindDevice( IDirectInput8W *iface, const GUID *guid, const WCHAR *name, GUID *instance_guid )
{
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    return IDirectInput7_FindDevice( &impl->IDirectInput7W_iface, guid, name, instance_guid );
}

static BOOL should_enumerate_device( const WCHAR *username, DWORD flags, struct list *device_players, const GUID *guid )
{
    BOOL should_enumerate = TRUE;
    struct DevicePlayer *device_player;

    /* Check if user owns impl device */
    if (flags & DIEDBSFL_THISUSER && username && *username)
    {
        should_enumerate = FALSE;
        LIST_FOR_EACH_ENTRY(device_player, device_players, struct DevicePlayer, entry)
        {
            if (IsEqualGUID(&device_player->instance_guid, guid))
            {
                if (*device_player->username && !wcscmp( username, device_player->username ))
                    return TRUE; /* Device username matches */
                break;
            }
        }
    }

    /* Check if impl device is not owned by anyone */
    if (flags & DIEDBSFL_AVAILABLEDEVICES)
    {
        BOOL found = FALSE;
        should_enumerate = FALSE;
        LIST_FOR_EACH_ENTRY(device_player, device_players, struct DevicePlayer, entry)
        {
            if (IsEqualGUID(&device_player->instance_guid, guid))
            {
                if (*device_player->username)
                    found = TRUE;
                break;
            }
        }
        if (!found)
            return TRUE; /* Device does not have a username */
    }

    return should_enumerate;
}

struct enum_device_by_semantics_params
{
    IDirectInput8W *iface;
    const WCHAR *username;
    DWORD flags;

    DIDEVICEINSTANCEW *instances;
    DWORD instance_count;
};

static BOOL CALLBACK enum_device_by_semantics( const DIDEVICEINSTANCEW *instance, void *context )
{
    struct enum_device_by_semantics_params *params = context;
    struct dinput *impl = impl_from_IDirectInput8W( params->iface );

    if (should_enumerate_device( params->username, params->flags, &impl->device_players, &instance->guidInstance ))
    {
        params->instance_count++;
        params->instances = realloc( params->instances, sizeof(DIDEVICEINSTANCEW) * params->instance_count );
        params->instances[params->instance_count - 1] = *instance;
    }

    return DIENUM_CONTINUE;
}

static HRESULT WINAPI dinput8_EnumDevicesBySemantics( IDirectInput8W *iface, const WCHAR *username, DIACTIONFORMATW *action_format,
                                                      LPDIENUMDEVICESBYSEMANTICSCBW callback, void *context, DWORD flags )
{
    struct enum_device_by_semantics_params params = {.iface = iface, .username = username, .flags = flags};
    DWORD callbackFlags, enum_flags = DIEDFL_ATTACHEDONLY | (flags & DIEDFL_FORCEFEEDBACK);
    static const GUID *guids[2] = {&GUID_SysKeyboard, &GUID_SysMouse};
    static const DWORD actionMasks[] = { DIKEYBOARD_MASK, DIMOUSE_MASK };
    struct dinput *impl = impl_from_IDirectInput8W( iface );
    DIDEVICEINSTANCEW didevi;
    IDirectInputDevice8W *lpdid;
    unsigned int i = 0;
    HRESULT hr;
    int remain;

    FIXME( "iface %p, username %s, action_format %p, callback %p, context %p, flags %#lx stub!\n",
           iface, debugstr_w(username), action_format, callback, context, flags );

    didevi.dwSize = sizeof(didevi);

    hr = IDirectInput8_EnumDevices( &impl->IDirectInput8W_iface, DI8DEVCLASS_GAMECTRL,
                                    enum_device_by_semantics, &params, enum_flags );
    if (FAILED(hr))
    {
        free( params.instances );
        return hr;
    }

    remain = params.instance_count;
    /* Add keyboard and mouse to remaining device count */
    if (!(flags & DIEDBSFL_FORCEFEEDBACK))
    {
        for (i = 0; i < ARRAY_SIZE(guids); i++)
        {
            if (should_enumerate_device( username, flags, &impl->device_players, guids[i] )) remain++;
        }
    }

    for (i = 0; i < params.instance_count; i++)
    {
        callbackFlags = diactionformat_priorityW( action_format, action_format->dwGenre );
        IDirectInput_CreateDevice( iface, &params.instances[i].guidInstance, &lpdid, NULL );

        if (callback( &params.instances[i], lpdid, callbackFlags, --remain, context ) == DIENUM_STOP)
        {
            free( params.instances );
            IDirectInputDevice_Release(lpdid);
            return DI_OK;
        }
        IDirectInputDevice_Release(lpdid);
    }

    free( params.instances );

    if (flags & DIEDBSFL_FORCEFEEDBACK) return DI_OK;

    /* Enumerate keyboard and mouse */
    for (i = 0; i < ARRAY_SIZE(guids); i++)
    {
        if (should_enumerate_device( username, flags, &impl->device_players, guids[i] ))
        {
            callbackFlags = diactionformat_priorityW( action_format, actionMasks[i] );

            IDirectInput_CreateDevice(iface, guids[i], &lpdid, NULL);
            IDirectInputDevice_GetDeviceInfo(lpdid, &didevi);

            if (callback( &didevi, lpdid, callbackFlags, --remain, context ) == DIENUM_STOP)
            {
                IDirectInputDevice_Release(lpdid);
                return DI_OK;
            }
            IDirectInputDevice_Release(lpdid);
        }
    }

    return DI_OK;
}

static HRESULT WINAPI dinput8_ConfigureDevices( IDirectInput8W *iface, LPDICONFIGUREDEVICESCALLBACK callback,
                                                DICONFIGUREDEVICESPARAMSW *params, DWORD flags, void *context )
{
    FIXME( "iface %p, callback %p, params %p, flags %#lx, context %p stub!\n", iface, callback,
           params, flags, context );

    /* Call helper function in config.c to do the real work */
    return _configure_devices( iface, callback, params, flags, context );
}

/*****************************************************************************
 * IDirectInputJoyConfig8 interface
 */

static inline struct dinput *impl_from_IDirectInputJoyConfig8( IDirectInputJoyConfig8 *iface )
{
    return CONTAINING_RECORD( iface, struct dinput, IDirectInputJoyConfig8_iface );
}

static HRESULT WINAPI joy_config_QueryInterface( IDirectInputJoyConfig8 *iface, REFIID iid, void **out )
{
    struct dinput *impl = impl_from_IDirectInputJoyConfig8( iface );
    return IDirectInput7_QueryInterface( &impl->IDirectInput7W_iface, iid, out );
}

static ULONG WINAPI joy_config_AddRef( IDirectInputJoyConfig8 *iface )
{
    struct dinput *impl = impl_from_IDirectInputJoyConfig8( iface );
    return IDirectInput7_AddRef( &impl->IDirectInput7W_iface );
}

static ULONG WINAPI joy_config_Release( IDirectInputJoyConfig8 *iface )
{
    struct dinput *impl = impl_from_IDirectInputJoyConfig8( iface );
    return IDirectInput7_Release( &impl->IDirectInput7W_iface );
}

static HRESULT WINAPI joy_config_Acquire( IDirectInputJoyConfig8 *iface )
{
    FIXME( "iface %p stub!\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_Unacquire( IDirectInputJoyConfig8 *iface )
{
    FIXME( "iface %p stub!\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_SetCooperativeLevel( IDirectInputJoyConfig8 *iface, HWND hwnd, DWORD flags )
{
    FIXME( "iface %p, hwnd %p, flags %#lx stub!\n", iface, hwnd, flags );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_SendNotify( IDirectInputJoyConfig8 *iface )
{
    FIXME( "iface %p stub!\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_EnumTypes( IDirectInputJoyConfig8 *iface, LPDIJOYTYPECALLBACK callback, void *context )
{
    FIXME( "iface %p, callback %p, context %p stub!\n", iface, callback, context );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_GetTypeInfo( IDirectInputJoyConfig8 *iface, const WCHAR *name,
                                              DIJOYTYPEINFO *info, DWORD flags )
{
    FIXME( "iface %p, name %s, info %p, flags %#lx stub!\n", iface, debugstr_w(name), info, flags );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_SetTypeInfo( IDirectInputJoyConfig8 *iface, const WCHAR *name,
                                              const DIJOYTYPEINFO *info, DWORD flags, WCHAR *new_name )
{
    FIXME( "iface %p, name %s, info %p, flags %#lx, new_name %s stub!\n",
           iface, debugstr_w(name), info, flags, debugstr_w(new_name) );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_DeleteType( IDirectInputJoyConfig8 *iface, const WCHAR *name )
{
    FIXME( "iface %p, name %s stub!\n", iface, debugstr_w(name) );
    return E_NOTIMPL;
}

struct find_device_from_index_params
{
    UINT index;
    DIDEVICEINSTANCEW instance;
};

static BOOL CALLBACK find_device_from_index( const DIDEVICEINSTANCEW *instance, void *context )
{
    struct find_device_from_index_params *params = context;
    params->instance = *instance;
    if (!params->index--) return DIENUM_STOP;
    return DIENUM_CONTINUE;
}

static HRESULT WINAPI joy_config_GetConfig( IDirectInputJoyConfig8 *iface, UINT id, DIJOYCONFIG *info, DWORD flags )
{
    struct dinput *impl = impl_from_IDirectInputJoyConfig8( iface );
    struct find_device_from_index_params params = {.index = id};
    HRESULT hr;

    FIXME( "iface %p, id %u, info %p, flags %#lx stub!\n", iface, id, info, flags );

#define X(x) if (flags & x) FIXME("\tflags |= "#x"\n");
    X(DIJC_GUIDINSTANCE)
    X(DIJC_REGHWCONFIGTYPE)
    X(DIJC_GAIN)
    X(DIJC_CALLOUT)
#undef X

    hr = IDirectInput8_EnumDevices( &impl->IDirectInput8W_iface, DI8DEVCLASS_GAMECTRL,
                                    find_device_from_index, &params, 0 );
    if (FAILED(hr)) return hr;
    if (params.index != ~0) return DIERR_NOMOREITEMS;
    if (flags & DIJC_GUIDINSTANCE) info->guidInstance = params.instance.guidInstance;
    return DI_OK;
}

static HRESULT WINAPI joy_config_SetConfig( IDirectInputJoyConfig8 *iface, UINT id, const DIJOYCONFIG *info, DWORD flags )
{
    FIXME( "iface %p, id %u, info %p, flags %#lx stub!\n", iface, id, info, flags );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_DeleteConfig( IDirectInputJoyConfig8 *iface, UINT id )
{
    FIXME( "iface %p, id %u stub!\n", iface, id );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_GetUserValues( IDirectInputJoyConfig8 *iface, DIJOYUSERVALUES *info, DWORD flags )
{
    FIXME( "iface %p, info %p, flags %#lx stub!\n", iface, info, flags );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_SetUserValues( IDirectInputJoyConfig8 *iface, const DIJOYUSERVALUES *info, DWORD flags )
{
    FIXME( "iface %p, info %p, flags %#lx stub!\n", iface, info, flags );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_AddNewHardware( IDirectInputJoyConfig8 *iface, HWND hwnd, const GUID *guid )
{
    FIXME( "iface %p, hwnd %p, guid %s stub!\n", iface, hwnd, debugstr_guid( guid ) );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_OpenTypeKey( IDirectInputJoyConfig8 *iface, const WCHAR *name, DWORD security, HKEY *key )
{
    FIXME( "iface %p, name %s, security %lu, key %p stub!\n", iface, debugstr_w(name), security, key );
    return E_NOTIMPL;
}

static HRESULT WINAPI joy_config_OpenAppStatusKey( IDirectInputJoyConfig8 *iface, HKEY *key )
{
    FIXME( "iface %p, key %p stub!\n", iface, key );
    return E_NOTIMPL;
}

static const IDirectInput7WVtbl dinput7_vtbl =
{
    dinput7_QueryInterface,
    dinput7_AddRef,
    dinput7_Release,
    dinput7_CreateDevice,
    dinput7_EnumDevices,
    dinput7_GetDeviceStatus,
    dinput7_RunControlPanel,
    dinput7_Initialize,
    dinput7_FindDevice,
    dinput7_CreateDeviceEx,
};

static const IDirectInput8WVtbl dinput8_vtbl =
{
    dinput8_QueryInterface,
    dinput8_AddRef,
    dinput8_Release,
    dinput8_CreateDevice,
    dinput8_EnumDevices,
    dinput8_GetDeviceStatus,
    dinput8_RunControlPanel,
    dinput8_Initialize,
    dinput8_FindDevice,
    dinput8_EnumDevicesBySemantics,
    dinput8_ConfigureDevices,
};

static const IDirectInputJoyConfig8Vtbl joy_config_vtbl =
{
    joy_config_QueryInterface,
    joy_config_AddRef,
    joy_config_Release,
    joy_config_Acquire,
    joy_config_Unacquire,
    joy_config_SetCooperativeLevel,
    joy_config_SendNotify,
    joy_config_EnumTypes,
    joy_config_GetTypeInfo,
    joy_config_SetTypeInfo,
    joy_config_DeleteType,
    joy_config_GetConfig,
    joy_config_SetConfig,
    joy_config_DeleteConfig,
    joy_config_GetUserValues,
    joy_config_SetUserValues,
    joy_config_AddNewHardware,
    joy_config_OpenTypeKey,
    joy_config_OpenAppStatusKey,
};

struct class_factory
{
    IClassFactory IClassFactory_iface;
};

static inline struct class_factory *impl_from_IClassFactory( IClassFactory *iface )
{
    return CONTAINING_RECORD( iface, struct class_factory, IClassFactory_iface );
}

static HRESULT WINAPI class_factory_QueryInterface( IClassFactory *iface, REFIID iid, void **out )
{
    struct class_factory *impl = impl_from_IClassFactory(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IClassFactory))
        *out = &impl->IClassFactory_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI class_factory_AddRef( IClassFactory *iface )
{
    return 2;
}

static ULONG WINAPI class_factory_Release( IClassFactory *iface )
{
    return 1;
}

static HRESULT WINAPI class_factory_CreateInstance( IClassFactory *iface, IUnknown *outer, REFIID iid, void **out )
{
    IUnknown *unknown;
    HRESULT hr;

    TRACE( "iface %p, outer %p, iid %s, out %p.\n", iface, outer, debugstr_guid( iid ), out );

    if (outer) return CLASS_E_NOAGGREGATION;

    if (FAILED(hr = dinput_create( &unknown ))) return hr;
    hr = IUnknown_QueryInterface( unknown, iid, out );
    IUnknown_Release( unknown );

    return hr;
}

static HRESULT WINAPI class_factory_LockServer( IClassFactory *iface, BOOL lock )
{
    FIXME( "iface %p, lock %d stub!\n", iface, lock );
    return S_OK;
}

static const IClassFactoryVtbl class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    class_factory_CreateInstance,
    class_factory_LockServer,
};

static struct class_factory class_factory = {{&class_factory_vtbl}};

/***********************************************************************
 *        DllGetClassObject (DINPUT.@)
 */
HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID iid, void **out )
{
    TRACE( "clsid %s, iid %s, out %p.\n", debugstr_guid( clsid ), debugstr_guid( iid ), out );

#if DIRECTINPUT_VERSION == 0x0700
    if (IsEqualCLSID( &CLSID_DirectInput, clsid ))
        return IClassFactory_QueryInterface( &class_factory.IClassFactory_iface, iid, out );
#else
    if (IsEqualCLSID( &CLSID_DirectInput8, clsid ))
        return IClassFactory_QueryInterface( &class_factory.IClassFactory_iface, iid, out );
#endif

    WARN( "%s not implemented, returning CLASS_E_CLASSNOTAVAILABLE.\n", debugstr_guid( clsid ) );
    return CLASS_E_CLASSNOTAVAILABLE;
}

/******************************************************************************
 *    DInput hook thread
 */

static LRESULT CALLBACK LL_hook_proc( int code, WPARAM wparam, LPARAM lparam )
{
    struct dinput_device *impl;
    int skip = 0;

    if (code != HC_ACTION) return CallNextHookEx( 0, code, wparam, lparam );

    EnterCriticalSection( &dinput_hook_crit );
    LIST_FOR_EACH_ENTRY( impl, &acquired_mouse_list, struct dinput_device, entry )
    {
        TRACE( "calling dinput_mouse_hook (%p %Ix %Ix)\n", impl, wparam, lparam );
        skip |= dinput_mouse_hook( &impl->IDirectInputDevice8W_iface, wparam, lparam );
    }
    LIST_FOR_EACH_ENTRY( impl, &acquired_keyboard_list, struct dinput_device, entry )
    {
        if (impl->use_raw_input) continue;
        TRACE( "calling dinput_keyboard_hook (%p %Ix %Ix)\n", impl, wparam, lparam );
        skip |= dinput_keyboard_hook( &impl->IDirectInputDevice8W_iface, wparam, lparam );
    }
    LeaveCriticalSection( &dinput_hook_crit );

    return skip ? 1 : CallNextHookEx( 0, code, wparam, lparam );
}

static LRESULT CALLBACK callwndproc_proc( int code, WPARAM wparam, LPARAM lparam )
{
    struct dinput_device *impl, *next;
    CWPSTRUCT *msg = (CWPSTRUCT *)lparam;
    HWND foreground;

    if (code != HC_ACTION || (msg->message != WM_KILLFOCUS &&
        msg->message != WM_ACTIVATEAPP && msg->message != WM_ACTIVATE))
        return CallNextHookEx( 0, code, wparam, lparam );

    foreground = GetForegroundWindow();

    EnterCriticalSection( &dinput_hook_crit );
    LIST_FOR_EACH_ENTRY_SAFE( impl, next, &acquired_device_list, struct dinput_device, entry )
    {
        if (msg->hwnd == impl->win && msg->hwnd != foreground)
        {
            TRACE( "%p window is not foreground - unacquiring %p\n", impl->win, impl );
            dinput_device_internal_unacquire( &impl->IDirectInputDevice8W_iface );
        }
    }
    LIST_FOR_EACH_ENTRY_SAFE( impl, next, &acquired_mouse_list, struct dinput_device, entry )
    {
        if (msg->hwnd == impl->win && msg->hwnd != foreground)
        {
            TRACE( "%p window is not foreground - unacquiring %p\n", impl->win, impl );
            dinput_device_internal_unacquire( &impl->IDirectInputDevice8W_iface );
        }
    }
    LIST_FOR_EACH_ENTRY_SAFE( impl, next, &acquired_rawmouse_list, struct dinput_device, entry )
    {
        if (msg->hwnd == impl->win && msg->hwnd != foreground)
        {
            TRACE( "%p window is not foreground - unacquiring %p\n", impl->win, impl );
            dinput_device_internal_unacquire( &impl->IDirectInputDevice8W_iface );
        }
    }
    LIST_FOR_EACH_ENTRY_SAFE( impl, next, &acquired_keyboard_list, struct dinput_device, entry )
    {
        if (msg->hwnd == impl->win && msg->hwnd != foreground)
        {
            TRACE( "%p window is not foreground - unacquiring %p\n", impl->win, impl );
            dinput_device_internal_unacquire( &impl->IDirectInputDevice8W_iface );
        }
    }
    LeaveCriticalSection( &dinput_hook_crit );

    return CallNextHookEx( 0, code, wparam, lparam );
}

static DWORD WINAPI dinput_thread_proc( void *params )
{
    HANDLE events[128], start_event = params;
    static HHOOK kbd_hook, mouse_hook;
    struct dinput_device *impl, *next;
    SIZE_T events_count = 0;
    HANDLE finished_event;
    DWORD ret;
    MSG msg;

    di_em_win = CreateWindowW( L"DIEmWin", L"DIEmWin", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, DINPUT_instance, NULL );

    /* Force creation of the message queue */
    PeekMessageW( &msg, 0, 0, 0, PM_NOREMOVE );
    SetEvent( start_event );

    while ((ret = MsgWaitForMultipleObjectsEx( events_count, events, INFINITE, QS_ALLINPUT, 0 )) <= events_count)
    {
        UINT kbd_cnt = 0, mice_cnt = 0;

        if (ret < events_count)
        {
            EnterCriticalSection( &dinput_hook_crit );
            LIST_FOR_EACH_ENTRY_SAFE( impl, next, &acquired_device_list, struct dinput_device, entry )
            {
                if (impl->read_event == events[ret])
                {
                    if (FAILED( impl->vtbl->read( &impl->IDirectInputDevice8W_iface ) ))
                    {
                        dinput_device_internal_unacquire( &impl->IDirectInputDevice8W_iface );
                        impl->status = STATUS_UNPLUGGED;
                    }
                    break;
                }
            }
            LeaveCriticalSection( &dinput_hook_crit );
        }

        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE ))
        {
            if (msg.message != WM_USER+0x10)
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                continue;
            }

            finished_event = (HANDLE)msg.lParam;

            TRACE( "Processing hook change notification wparam %#Ix, lparam %#Ix.\n", msg.wParam, msg.lParam );

            if (!msg.wParam)
            {
                if (kbd_hook) UnhookWindowsHookEx( kbd_hook );
                if (mouse_hook) UnhookWindowsHookEx( mouse_hook );
                kbd_hook = mouse_hook = NULL;
                goto done;
            }

            EnterCriticalSection( &dinput_hook_crit );
            kbd_cnt = list_count( &acquired_keyboard_list );
            mice_cnt = list_count( &acquired_mouse_list );
            LeaveCriticalSection( &dinput_hook_crit );

            if (kbd_cnt && !kbd_hook)
                kbd_hook = SetWindowsHookExW( WH_KEYBOARD_LL, LL_hook_proc, DINPUT_instance, 0 );
            else if (!kbd_cnt && kbd_hook)
            {
                UnhookWindowsHookEx( kbd_hook );
                kbd_hook = NULL;
            }

            if (mice_cnt && !mouse_hook)
                mouse_hook = SetWindowsHookExW( WH_MOUSE_LL, LL_hook_proc, DINPUT_instance, 0 );
            else if (!mice_cnt && mouse_hook)
            {
                UnhookWindowsHookEx( mouse_hook );
                mouse_hook = NULL;
            }

            SetEvent(finished_event);
        }

        events_count = 0;
        EnterCriticalSection( &dinput_hook_crit );
        LIST_FOR_EACH_ENTRY( impl, &acquired_device_list, struct dinput_device, entry )
        {
            if (!impl->read_event || !impl->vtbl->read) continue;
            if (events_count >= ARRAY_SIZE(events)) break;
            events[events_count++] = impl->read_event;
        }
        LeaveCriticalSection( &dinput_hook_crit );
    }

    if (ret != events_count) ERR("Unexpected termination, ret %#lx\n", ret);

done:
    DestroyWindow( di_em_win );
    di_em_win = NULL;
    return 0;
}

static BOOL WINAPI dinput_thread_start_once( INIT_ONCE *once, void *param, void **context )
{
    HANDLE start_event;

    start_event = CreateEventW( NULL, FALSE, FALSE, NULL );
    if (!start_event) ERR( "failed to create start event, error %lu\n", GetLastError() );

    dinput_thread = CreateThread( NULL, 0, dinput_thread_proc, start_event, 0, &dinput_thread_id );
    if (!dinput_thread) ERR( "failed to create internal thread, error %lu\n", GetLastError() );

    WaitForSingleObject( start_event, INFINITE );
    CloseHandle( start_event );

    return TRUE;
}

static void dinput_thread_start(void)
{
    static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce( &init_once, dinput_thread_start_once, NULL, NULL );
}

static void dinput_thread_stop(void)
{
    PostThreadMessageW( dinput_thread_id, WM_USER + 0x10, 0, 0 );
    if (WaitForSingleObject( dinput_thread, 500 ) == WAIT_TIMEOUT)
        WARN("Timeout while waiting for internal thread\n");
    CloseHandle( dinput_thread );
}

void check_dinput_hooks( IDirectInputDevice8W *iface, BOOL acquired )
{
    static HHOOK callwndproc_hook;
    static ULONG foreground_cnt;
    struct dinput_device *impl = impl_from_IDirectInputDevice8W( iface );
    HANDLE hook_change_finished_event = NULL;

    dinput_thread_start();

    EnterCriticalSection(&dinput_hook_crit);

    if (impl->dwCoopLevel & DISCL_FOREGROUND)
    {
        if (acquired)
            foreground_cnt++;
        else
            foreground_cnt--;
    }

    if (foreground_cnt && !callwndproc_hook)
        callwndproc_hook = SetWindowsHookExW( WH_CALLWNDPROC, callwndproc_proc,
                                              DINPUT_instance, GetCurrentThreadId() );
    else if (!foreground_cnt && callwndproc_hook)
    {
        UnhookWindowsHookEx( callwndproc_hook );
        callwndproc_hook = NULL;
    }

    if (impl->use_raw_input)
    {
        if (acquired)
        {
            impl->raw_device.dwFlags = 0;
            if (impl->dwCoopLevel & DISCL_BACKGROUND)
                impl->raw_device.dwFlags |= RIDEV_INPUTSINK;
            if (impl->dwCoopLevel & DISCL_EXCLUSIVE)
                impl->raw_device.dwFlags |= RIDEV_NOLEGACY;
            if ((impl->dwCoopLevel & DISCL_EXCLUSIVE) && impl->raw_device.usUsage == 2)
                impl->raw_device.dwFlags |= RIDEV_CAPTUREMOUSE;
            if ((impl->dwCoopLevel & DISCL_EXCLUSIVE) && impl->raw_device.usUsage == 6)
                impl->raw_device.dwFlags |= RIDEV_NOHOTKEYS;
            impl->raw_device.hwndTarget = di_em_win;
        }
        else
        {
            impl->raw_device.dwFlags = RIDEV_REMOVE;
            impl->raw_device.hwndTarget = NULL;
        }

        if (!RegisterRawInputDevices( &impl->raw_device, 1, sizeof(RAWINPUTDEVICE) ))
            WARN( "Unable to (un)register raw device %x:%x\n", impl->raw_device.usUsagePage, impl->raw_device.usUsage );
    }

    hook_change_finished_event = CreateEventW( NULL, FALSE, FALSE, NULL );
    PostThreadMessageW( dinput_thread_id, WM_USER + 0x10, 1, (LPARAM)hook_change_finished_event );

    LeaveCriticalSection(&dinput_hook_crit);

    WaitForSingleObject(hook_change_finished_event, INFINITE);
    CloseHandle(hook_change_finished_event);
}

void check_dinput_events(void)
{
    /* Windows does not do that, but our current implementation of winex11
     * requires periodic event polling to forward events to the wineserver.
     *
     * We have to call this function from multiple places, because:
     * - some games do not explicitly poll for mouse events
     *   (for example Culpa Innata)
     * - some games only poll the device, and neither keyboard nor mouse
     *   (for example Civilization: Call to Power 2)
     * - some games do not explicitly poll for keyboard events
     *   (for example Morrowind in its key binding page)
     */
    MsgWaitForMultipleObjectsEx(0, NULL, 0, QS_ALLINPUT, 0);
}

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    TRACE( "inst %p, reason %lu, reserved %p.\n", inst, reason, reserved );

    switch(reason)
    {
      case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(inst);
        DINPUT_instance = inst;
        register_di_em_win_class();
        break;
      case DLL_PROCESS_DETACH:
        if (reserved) break;
        dinput_thread_stop();
        unregister_di_em_win_class();
        break;
    }
    return TRUE;
}
