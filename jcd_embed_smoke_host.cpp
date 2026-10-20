/* Minimal Win32 panel host to smoke-test Blender --parent-hwnd embed.
 *
 * Build (Developer Command Prompt):
 *   cl /EHsc /O2 jcd_embed_smoke_host.cpp /link user32.lib gdi32.lib
 *
 * Run:
 *   jcd_embed_smoke_host.exe "D:\path\to\blender.exe"
 *
 * Creates a WS_CLIPCHILDREN panel, launches blender with --parent-hwnd,
 * and resizes the child on WM_SIZE. Close the panel to exit.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <cstdio>

static HWND g_panel = nullptr;
static PROCESS_INFORMATION g_pi = {};

static void resize_blender_child()
{
  if (!g_panel || !IsWindow(g_panel)) {
    return;
  }
  RECT rc;
  GetClientRect(g_panel, &rc);
  HWND child = GetWindow(g_panel, GW_CHILD);
  if (child && IsWindow(child)) {
    SetWindowPos(child,
                 nullptr,
                 0,
                 0,
                 rc.right - rc.left,
                 rc.bottom - rc.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    case WM_SIZE:
      resize_blender_child();
      return 0;
    case WM_DESTROY:
      if (g_pi.hProcess) {
        TerminateProcess(g_pi.hProcess, 0);
        CloseHandle(g_pi.hProcess);
        CloseHandle(g_pi.hThread);
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int wmain(int argc, wchar_t **argv)
{
  if (argc < 2) {
    fwprintf(stderr, L"Usage: %s <path\\to\\blender.exe>\n", argv[0]);
    return 1;
  }

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"JcdBlenderEmbedHost";
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);

  HWND frame = CreateWindowExW(0,
                               wc.lpszClassName,
                               L"JCD Blender Embed Smoke Host",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT,
                               CW_USEDEFAULT,
                               1280,
                               800,
                               nullptr,
                               nullptr,
                               wc.hInstance,
                               nullptr);

  g_panel = CreateWindowExW(0,
                            L"STATIC",
                            L"",
                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                            0,
                            0,
                            100,
                            100,
                            frame,
                            nullptr,
                            wc.hInstance,
                            nullptr);

  RECT fr;
  GetClientRect(frame, &fr);
  MoveWindow(g_panel, 0, 0, fr.right, fr.bottom, TRUE);

  const uintptr_t hwnd_val = reinterpret_cast<uintptr_t>(g_panel);
  const DWORD host_pid = GetCurrentProcessId();

  wchar_t cmd[2048];
  _snwprintf_s(cmd,
               _TRUNCATE,
               L"\"%s\" --parent-hwnd %llu --host-pid %lu",
               argv[1],
               static_cast<unsigned long long>(hwnd_val),
               static_cast<unsigned long>(host_pid));

  STARTUPINFOW si = {sizeof(si)};
  if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &g_pi)) {
    fwprintf(stderr, L"CreateProcess failed (%lu)\n", GetLastError());
    return 2;
  }

  /* Give Blender a moment, then force fill. */
  Sleep(2000);
  resize_blender_child();

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}
