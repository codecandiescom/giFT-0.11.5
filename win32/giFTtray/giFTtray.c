/*
 * $Header: /cvsroot/gift/giFT/win32/giFTtray/giFTtray.c,v 1.34 2003/11/01 06:00:27 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "config.h"

#include "giftd.h"
#include "queue.h"
#include "network.h"

#include <shellapi.h>	/* NOTIFYICONDATA */
#include <io.h>			/* _access() */

#ifndef GIFT_INTERFACE_PORT
#define GIFT_INTERFACE_PORT (1213)
#endif

#define GIFT_IP_ADDRESS	"127.0.0.1"

#define APP_NAME "giFTtray"
#define APP_VERSION "0.10"

#define WINDOW_CLASS APP_NAME

#define ICON_ID_LARGE 100
#define ICON_ID_SMALL 100


enum
{
	MY_WM_TRAY			= WM_USER,
	MY_WM_SOCKET_EVENT
};

enum
{
	MENU_EXIT,
	MENU_ENABLE_DISABLE,
	MENU_VIEW_LOG,
	MENU_ABOUT,
	MENU_OPTIONS,
#ifdef _DEBUG
	MENU_STOP,
	MENU_START,
#endif /* _DEBUG */
	MENU_RESTART		/* temporary until the <opt> command works */
};

#define NOT_CONNECTED APP_NAME " (not connected)"
#define CONNECTING APP_NAME " (connecting)"
#define DISCONNECTING APP_NAME " (disconnecting)"
#define RESTARTING APP_NAME " (restarting)"

/*****************************************************************************/


static TCPC           *gift_conn;


static HINSTANCE       app_instance;
static HWND            app_window;
static HMENU           popup_menu;
static int             sharing_enabled = TRUE;
static char           *gift_ip;
static unsigned short  gift_port;
static UINT            client_foreground_message = 0;

/*****************************************************************************/

#ifdef _DEBUG
#define DBG(x) dbg (x)

static void dbg (const char *format, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, format);
	_vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	OutputDebugString (buf);
	OutputDebugString ("\n");
}
#else
#define DBG(x)
#endif

/*****************************************************************************/

static void info (const char *format, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, format);
	_vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	MessageBox (NULL, buf, APP_NAME, MB_OK | MB_ICONINFORMATION);
}

static void error (const char *format, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, format);
	_vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	MessageBox (NULL, buf, APP_NAME, MB_OK | MB_ICONERROR);
}

static void fatal_error (const char *format, ...)
{
	va_list args;
	va_start (args, format);
	error (format, args);
	va_end (args);

	ExitProcess (1);
}

/*****************************************************************************/

static List *write_list = NULL;

static void perform_read ()
{
	unsigned char *data;
	size_t         data_len;
	FDBuf         *buf;
	int            n;


	if (!gift_conn)
		return;

	buf = tcp_readbuf (gift_conn);

	/*
	 * Read as much data as possible, attempting to locate a ';' on the
	 * data stream.
	 */
	if ((n = fdbuf_delim (buf, ";")) < 0)
	{
/*
// we don't want to close the connection, just return, right?
		tcp_close (gift_conn);
		gift_conn = NULL;
		event_quit (0);
*/
		return;
	}

	/*
	 * We were unable to locate the sentinel character (';') so we bail out
	 * of this call and wait for more data through the event loop.
	 */
	if (n > 0)
		return;

	/*
	 * Parse the interface protocol into a usable high-level structure for
	 * handling.
	 */
	data = fdbuf_data (buf, &data_len);
	assert (data != NULL);

	/*
	 * Parse the packet into a tree-structure accessable through the API
	 * provided by interface.c.  Then process the object with our high-level
	 * logic and cleanup.
	 */
/*
	cmd = interface_unserialize ((char *)data, data_len);
	handle_search_result (cmd);
	interface_free (cmd);
*/
	/*
	 * Release the buffer so that the subsequent calls can use a fresh
	 * buffer.
	 */
	fdbuf_release (buf);
}

static int perform_write ()
{
	int result;
	static size_t written_len = 0;

	if (write_list)
	{
		assert (gift_conn);

		result = net_send (gift_conn->fd, (char *) write_list->data
						   + written_len, 0);

		if (result == -1 && WSAGetLastError () != WSAEWOULDBLOCK)
			return FALSE;

		written_len += (result == -1 ? 0 : result);

		if (written_len == strlen (write_list->data))
		{
			written_len = 0;
			free (write_list->data);
			write_list = list_remove (write_list,
			                          write_list->data);
		}
	}

	return TRUE;
}

/*****************************************************************************/

static void insert_menu_item (HMENU menu, int id, char *caption)
{
	MENUITEMINFO item;

	ZeroMemory (&item, sizeof (item));

	item.cbSize = sizeof (item);
	item.fMask = MIIM_ID | MIIM_TYPE;
	item.wID = id;
	item.fType = MFT_STRING;
	item.dwTypeData = caption;
	item.cch = strlen (caption);

	InsertMenuItem (menu, 0, TRUE, &item);
}

static void insert_menu_sep (HMENU menu)
{
	MENUITEMINFO item;

	ZeroMemory (&item, sizeof (item));

	item.cbSize = sizeof (item);
	item.fMask = MIIM_TYPE;
	item.fType = MFT_SEPARATOR;

	InsertMenuItem (menu, 0, TRUE, &item);
}

static void change_menu_item_caption (HMENU menu, int id, char *caption)
{
	MENUITEMINFO item;

	ZeroMemory (&item, sizeof (item));

	item.cbSize = sizeof (item);
	item.fMask = MIIM_TYPE;
	item.fType = MFT_STRING;
	item.dwTypeData = caption;
	item.cch = strlen (caption);

	SetMenuItemInfo (menu, id, FALSE, &item);
}

/*****************************************************************************/

static int create_process (char *cmd_line, PROCESS_INFORMATION *proc_info)
{
	STARTUPINFO startup_info;

	assert (proc_info);

	ZeroMemory (proc_info, sizeof (PROCESS_INFORMATION));

	ZeroMemory (&startup_info, sizeof (startup_info));
	startup_info.cb = sizeof (startup_info);

	if (!CreateProcess (NULL, cmd_line, NULL, NULL, TRUE,
						CREATE_NO_WINDOW,
						NULL, NULL, &startup_info, proc_info))
		return FALSE;

	assert (proc_info->hProcess);

	/* wait for the process to finish loading, time out after 1 second */
	WaitForInputIdle (proc_info->hProcess, 1000);

	return TRUE;
}

/*****************************************************************************/

static void add_tray (const char* format, ...)
{
	NOTIFYICONDATA nid;
	va_list args;

	ZeroMemory (&nid, sizeof (nid));

	nid.cbSize = sizeof (nid);
	nid.hWnd = app_window;
	nid.uID = 0;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = MY_WM_TRAY;
	nid.hIcon = LoadIcon (app_instance, (LPCSTR) ICON_ID_SMALL);

	va_start (args, format);
	_vsnprintf (nid.szTip, sizeof (nid.szTip) - 1, format, args);
	va_end (args);

	Shell_NotifyIcon (NIM_ADD, &nid);
}

static void modify_tray (const char* format, ...)
{
	NOTIFYICONDATA nid;
	va_list args;

	ZeroMemory (&nid, sizeof (nid));

	nid.cbSize = sizeof (nid);
	nid.hWnd = app_window;
	nid.uID = 0;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = MY_WM_TRAY;
	nid.hIcon = LoadIcon (app_instance, (LPCSTR) ICON_ID_SMALL);

	va_start (args, format);
	_vsnprintf (nid.szTip, sizeof (nid.szTip) - 1, format, args);
	va_end (args);

	Shell_NotifyIcon (NIM_MODIFY, &nid);
}

static void remove_tray ()
{
	NOTIFYICONDATA nid;

	ZeroMemory (&nid, sizeof (nid));

	nid.cbSize = sizeof (nid);
	nid.hWnd = app_window;
	nid.uID = 0;
	nid.uFlags = 0;

	Shell_NotifyIcon (NIM_DELETE, &nid);
}

/*****************************************************************************/

static int spawn_gift ()
{
	PROCESS_INFORMATION proc_info;
	char gift_exe[_MAX_PATH];
	char *path;

	ZeroMemory (&proc_info, sizeof (proc_info));

	path = platform_home_dir ();

#ifdef _DEBUG
	if (strstr (__argv[0], "S:\\gift\\win32\\giFTtray\\Debug\\giFTtray.exe")) {
		path = "S:\\gift\\win32\\Debug";
	}
#endif

	_snprintf (gift_exe, _MAX_PATH, "%s\\giFT.exe", path);

	if (_access (gift_exe, 04))
	{
		error ("File not found: %s: %s", gift_exe, platform_error ());
		return FALSE;
	}

	strncat (gift_exe, " -d", _MAX_PATH);

	if (!create_process (gift_exe, &proc_info))
	{
		error ("Failed to execute %s: %s", gift_exe, platform_error ());
		return FALSE;
	}

	return TRUE;
}

static int gift_attach (char *ip_str, unsigned short port)
{
	Interface *cmd;
	in_addr_t ip;

	ip = net_ip (ip_str);
	gift_conn = tcp_open (ip, port, TRUE);
	if (!gift_conn)
		return FALSE;

	WSAAsyncSelect (gift_conn->fd, app_window, MY_WM_SOCKET_EVENT,
					FD_READ | FD_WRITE);

	if (!(cmd = interface_new ("attach", NULL)))
		return FALSE;

	interface_put (cmd, "attach", NULL);
	interface_put (cmd, "client", APP_NAME);
	interface_put (cmd, "version", APP_VERSION);

	interface_send (cmd, gift_conn);
	interface_free (cmd);
	tcp_flush (gift_conn, TRUE);

	return TRUE;
}

/* number of times we'll attempt to connect to the daemon */
#define ATTACH_ATTEMPTS 10

/* milliseconds to sleep between attempts */
#define SLEEP_BETWEEN_ATTEMPTS 100

static int gift_start ()
{
	int attempts = 0;
	int gift_spawned = 0;

	modify_tray (CONNECTING);

	while (attempts++ < ATTACH_ATTEMPTS)
	{
		if (gift_attach (gift_ip, gift_port))
		{
			modify_tray (APP_NAME " (%s:%d)", gift_ip, gift_port);
			return TRUE;
		}
		else
		{
			if (!gift_spawned++)
			{
				if (!spawn_gift ())
					break;
			}
			Sleep (SLEEP_BETWEEN_ATTEMPTS);
		}
	}

	modify_tray (NOT_CONNECTED);

	error ("Failed to attach to giFT at %s:%d.\n\n"
	       "Select 'Options...' and verify you have\n"
	       "properly edited gift.conf.\n"
	       "\n"
	       "You may also wish to select 'View Log...' to view\n"
	       "any error messages that the giFT daemon has "
	       "reported.", gift_ip, gift_port);

	return FALSE;
}

static int gift_stop ()
{
	Interface *cmd;

	if (!gift_conn)
		return FALSE;


	modify_tray (DISCONNECTING);

	WSAAsyncSelect (gift_conn->fd, app_window, 0, 0);

	if (!(cmd = interface_new ("quit", NULL)))
		return FALSE;

	interface_put (cmd, "quit", NULL);
	interface_send (cmd, gift_conn);
	interface_free (cmd);


	tcp_flush (gift_conn, TRUE);

	tcp_close (gift_conn);
	gift_conn = NULL;
	modify_tray (NOT_CONNECTED);

	return TRUE;
}

static void gift_restart ()
{
	modify_tray (RESTARTING);
	gift_stop ();
	gift_start ();
}

static void gift_opt ()
{
	/* TODO send <opt /> command, for now just restart the daemon */
	gift_restart ();
}

/*****************************************************************************/

static HMENU create_popup_menu ()
{
	HMENU menu;

	menu = CreatePopupMenu ();

	insert_menu_item (menu, MENU_EXIT, "E&xit");
	insert_menu_sep (menu);
	insert_menu_item (menu, MENU_OPTIONS,        "&Options...");
	insert_menu_item (menu, MENU_VIEW_LOG,       "&View Log...");
	insert_menu_item (menu, MENU_ENABLE_DISABLE, "&Disable");
	insert_menu_item (menu, MENU_RESTART,        "&Restart");
#ifdef _DEBUG
	insert_menu_item (menu, MENU_STOP,           "&Stop");
	insert_menu_item (menu, MENU_START,          "&Start");
#endif /* _DEBUG */
	insert_menu_item (menu, MENU_ABOUT,          "&About...");

	return menu;
}

static void popup_client ()
{
	int preferred_client_id = 0;

	/* TODO send the preferred client ID in wparam */
	PostMessage (HWND_BROADCAST, client_foreground_message,
				 preferred_client_id, 0);
}

static void handle_tray_message (UINT message)
{
	switch (message)
	{
	 case WM_LBUTTONDOWN: /* let's respond to any button */
	 {
// for now
//		 popup_client ();
//		 break;
	 }
	 case WM_RBUTTONDOWN:
	 {
		POINT point;

		GetCursorPos (&point);
		SetForegroundWindow (app_window);

		TrackPopupMenuEx (popup_menu, 0, point.x, point.y,
		                  app_window, NULL);

		PostMessage (app_window, WM_NULL, 0, 0);

		break;
	 }
	}
}

static void toggle_sharing ()
{
	char *caption;
	char *if_state;
	Interface *cmd;

	if (!gift_conn)
		return;

	if (sharing_enabled)
	{
		caption = "&Enable";
		if_state = "hide";
	}
	else
	{
		caption = "&Disable";
		if_state = "show";
	}

	WSAAsyncSelect (gift_conn->fd, app_window, 0, 0);

	if (!(cmd = interface_new ("share", NULL)))
		return;

	interface_put (cmd, "action", if_state);
	interface_send (cmd, gift_conn);
	interface_free (cmd);

	tcp_flush (gift_conn, TRUE);

	change_menu_item_caption (popup_menu, MENU_ENABLE_DISABLE,
	                          caption);

	sharing_enabled = !sharing_enabled;
}

static void show_about_box ()
{
	char buf[1024];

	char *message =
		"Copyright \xa9 2001-2003, giFT project (http://giftproject.org/)\n"
		"\n"
		"This program is free software; you can redistribute it and/or\n"
		"modify it under the terms of the GNU General Public License\n"
		"as published by the Free Software Foundation; either version 2,\n"
		"or (at your option) any later version.\n"
		"\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
		"See the GNU General Public License for more details.\n";

	_snprintf (buf, sizeof (buf), "giFT: Internet File Transfer %s (%s %s)",
			  VERSION, __DATE__, __TIME__);

	MessageBox (NULL, message, buf, MB_OK | MB_ICONINFORMATION);
}

static void show_view_log ()
{
	char path[_MAX_PATH];

	_snprintf (path, sizeof(path), "%s/%s", platform_home_dir (), "giftd.log");

	ShellExecute (app_window, "open", "NOTEPAD", path, NULL, SW_SHOWNORMAL);
}

static void show_setup ()
{
	PROCESS_INFORMATION proc_info;
	char giftsetup_exe[_MAX_PATH];
	DWORD exit_code;

	_snprintf (giftsetup_exe, _MAX_PATH, "%s\\giFTsetup.exe",
	           platform_home_dir ());

	if (_access (giftsetup_exe, 04))
	{
		error ("File not found: %s: %s", giftsetup_exe, platform_error ());
		return;
	}

	if (!create_process (giftsetup_exe, &proc_info))
	{
		error ("Failed to execute %s: %s", giftsetup_exe, platform_error ());
		return;
	}

	while (WaitForSingleObject (proc_info.hProcess, 100) == WAIT_TIMEOUT)
	{
		static MSG msg;

		while (PeekMessage (&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
			DispatchMessage (&msg);
	}
	GetExitCodeProcess (proc_info.hProcess, &exit_code);

	CloseHandle (proc_info.hProcess);

	if (exit_code == 1)
		gift_opt ();
}

static int handle_command (int id, int event)
{
	switch (id)
	{
	 case MENU_EXIT:			DestroyWindow (app_window);	break;
	 case MENU_RESTART:			gift_restart ();			break;
#ifdef _DEBUG
	 case MENU_START:			gift_start ();				break;
	 case MENU_STOP:			gift_stop ();				break;
#endif /* _DEBUG */
	 case MENU_ABOUT:			show_about_box ();			break;
	 case MENU_VIEW_LOG:		show_view_log ();			break;
	 case MENU_OPTIONS:			show_setup ();				break;
	 case MENU_ENABLE_DISABLE:	toggle_sharing ();	break;
	 default:					return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static LRESULT CALLBACK window_proc (HWND window, UINT message,
				     WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	 case MY_WM_TRAY:
		handle_tray_message (lparam);
		break;
	 case MY_WM_SOCKET_EVENT:
		if (WSAGETSELECTEVENT (lparam) == FD_READ)
			perform_read ();
		else
			perform_write ();
		break;
	 case WM_COMMAND:
		if (handle_command (LOWORD (wparam), HIWORD (wparam)))
			break;
		else
			return DefWindowProc (window, message, wparam, lparam);
	 case WM_DESTROY:
		PostQuitMessage (0);
		break;
	 default:
		return DefWindowProc (window, message, wparam, lparam);
	}

	return 0;
}

static HWND create_app_window ()
{
	WNDCLASSEX wcex;

	ZeroMemory (&wcex, sizeof (wcex));

	wcex.cbSize = sizeof (wcex);
	wcex.style = 0;
	wcex.lpfnWndProc = (WNDPROC) window_proc;
	wcex.cbClsExtra	= 0;
	wcex.cbWndExtra	= 0;
	wcex.hInstance = app_instance;
	wcex.hIcon = LoadIcon (app_instance, (LPCSTR) ICON_ID_LARGE);
	wcex.hCursor = NULL;
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = WINDOW_CLASS;
	wcex.hIconSm = LoadIcon (app_instance, (LPCSTR) ICON_ID_SMALL);

	RegisterClassEx (&wcex);

	return CreateWindow (WINDOW_CLASS, APP_NAME, 0,
	                     CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL,
	                     NULL, app_instance, NULL);
}

void __cdecl _setargv (void);

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prev_instance,
                    LPSTR cmd_line, int cmd_show)
{
	MSG msg;
	HANDLE mutex;

	_setargv ();


	if (!libgift_init (APP_NAME, GLOG_STDOUT, NULL))
		fatal_error ("Initialization failed");


	/* only allow one instance of giFTtray */
	mutex = CreateMutex (NULL, TRUE, APP_NAME);
	if (GetLastError () == ERROR_ALREADY_EXISTS)
		/* TODO popup existing instance? */
		return 0;
	else
		if (mutex == NULL)
			fatal_error ("Can't create mutex: %s", platform_error ());

	app_instance = instance;
	app_window = create_app_window ();
	popup_menu = create_popup_menu ();

	if (__argv[1])
		gift_ip   = __argv[1];
	else
		gift_ip   = GIFT_IP_ADDRESS;

	gift_port = GIFT_INTERFACE_PORT;

	add_tray (NOT_CONNECTED);

	gift_start ();

	client_foreground_message = RegisterWindowMessage (APP_NAME);

	while (GetMessage (&msg, NULL, 0, 0))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
		Sleep (1); /* otherwise, my CPU hits 100%, braindead f**kin' winblows! */
	}


	gift_stop ();


	libgift_finish ();

	DestroyMenu (popup_menu);

	remove_tray ();

	ReleaseMutex (mutex);

	return msg.wParam;
}
