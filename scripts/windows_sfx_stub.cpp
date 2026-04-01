#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr std::array<char, 10> kFooterMagic{'C', 'U', 'B', 'E', 'O', 'S', 'S', 'F', 'X', '1'};
constexpr std::size_t kFooterSize = kFooterMagic.size() + sizeof(std::uint64_t);
constexpr wchar_t kInstallerTitle[] = L"CubeOS Snapshot Installer";
constexpr wchar_t kReleaseTag[] = L"v0.3.0-snapshot.2";
constexpr wchar_t kGameExecutable[] = L"CubeOS-v0.3.0-snapshot.2.exe";

void showError(const std::wstring& message) {
  MessageBoxW(nullptr, message.c_str(), kInstallerTitle, MB_ICONERROR | MB_OK);
}

std::wstring quoteForPowerShell(const std::wstring& value) {
  std::wstring quoted;
  quoted.reserve(value.size() + 2);
  quoted.push_back(L'\'');
  for (wchar_t ch : value) {
    if (ch == L'\'') {
      quoted.append(L"''");
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back(L'\'');
  return quoted;
}

bool runProcessAndWait(const std::wstring& commandLine, std::wstring& error) {
  STARTUPINFOW startupInfo{};
  startupInfo.cb = sizeof(startupInfo);
  PROCESS_INFORMATION processInfo{};

  std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
  buffer.push_back(L'\0');

  if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                      nullptr, &startupInfo, &processInfo)) {
    error = L"Could not start the extraction helper.";
    return false;
  }

  WaitForSingleObject(processInfo.hProcess, INFINITE);

  DWORD exitCode = 1;
  GetExitCodeProcess(processInfo.hProcess, &exitCode);
  CloseHandle(processInfo.hThread);
  CloseHandle(processInfo.hProcess);

  if (exitCode != 0) {
    error = L"The extraction helper exited with a non-zero code.";
    return false;
  }

  return true;
}

bool launchDetached(const std::filesystem::path& executablePath,
                    const std::filesystem::path& workingDirectory,
                    std::wstring& error) {
  STARTUPINFOW startupInfo{};
  startupInfo.cb = sizeof(startupInfo);
  PROCESS_INFORMATION processInfo{};

  const std::wstring commandLine = L"\"" + executablePath.wstring() + L"\"";
  std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
  buffer.push_back(L'\0');

  if (!CreateProcessW(executablePath.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      workingDirectory.c_str(), &startupInfo, &processInfo)) {
    error = L"CubeOS was extracted, but launching it failed.";
    return false;
  }

  CloseHandle(processInfo.hThread);
  CloseHandle(processInfo.hProcess);
  return true;
}

bool readFooter(std::ifstream& input, std::uint64_t fileSize, std::uint64_t& payloadSize) {
  if (fileSize < kFooterSize) {
    return false;
  }

  input.seekg(static_cast<std::streamoff>(fileSize - kFooterSize), std::ios::beg);
  std::array<char, kFooterSize> footer{};
  input.read(footer.data(), static_cast<std::streamsize>(footer.size()));
  if (!input) {
    return false;
  }

  for (std::size_t i = 0; i < kFooterMagic.size(); ++i) {
    if (footer[i] != kFooterMagic[i]) {
      return false;
    }
  }

  payloadSize = 0;
  for (std::size_t i = 0; i < sizeof(std::uint64_t); ++i) {
    payloadSize |= static_cast<std::uint64_t>(static_cast<unsigned char>(footer[kFooterMagic.size() + i]))
                   << (i * 8);
  }
  return true;
}

bool extractPayload(const std::filesystem::path& selfPath,
                    const std::filesystem::path& zipPath,
                    std::wstring& error) {
  std::ifstream input(selfPath, std::ios::binary);
  if (!input) {
    error = L"Could not open the installer executable.";
    return false;
  }

  input.seekg(0, std::ios::end);
  const auto endPos = input.tellg();
  if (endPos <= 0) {
    error = L"Could not determine the installer size.";
    return false;
  }

  const std::uint64_t fileSize = static_cast<std::uint64_t>(endPos);
  std::uint64_t payloadSize = 0;
  if (!readFooter(input, fileSize, payloadSize)) {
    error = L"This installer does not contain a valid CubeOS payload.";
    return false;
  }
  if (payloadSize == 0 || payloadSize + kFooterSize > fileSize) {
    error = L"The embedded CubeOS payload is invalid.";
    return false;
  }

  const std::uint64_t payloadOffset = fileSize - payloadSize - kFooterSize;
  input.seekg(static_cast<std::streamoff>(payloadOffset), std::ios::beg);

  std::ofstream output(zipPath, std::ios::binary | std::ios::trunc);
  if (!output) {
    error = L"Could not create the temporary payload archive.";
    return false;
  }

  std::vector<char> buffer(64 * 1024);
  std::uint64_t remaining = payloadSize;
  while (remaining > 0) {
    const auto chunkSize =
        static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, buffer.size()));
    input.read(buffer.data(), chunkSize);
    if (!input) {
      error = L"Failed to read the embedded CubeOS payload.";
      return false;
    }
    output.write(buffer.data(), chunkSize);
    if (!output) {
      error = L"Failed to write the temporary payload archive.";
      return false;
    }
    remaining -= static_cast<std::uint64_t>(chunkSize);
  }

  return true;
}

std::filesystem::path localAppDataPath() {
  const wchar_t* value = _wgetenv(L"LOCALAPPDATA");
  if (value != nullptr && *value != L'\0') {
    return std::filesystem::path(value);
  }
  const wchar_t* profile = _wgetenv(L"USERPROFILE");
  if (profile != nullptr && *profile != L'\0') {
    return std::filesystem::path(profile) / L"AppData" / L"Local";
  }
  return std::filesystem::temp_directory_path();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  try {
    std::wstring modulePathBuffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, modulePathBuffer.data(),
                                      static_cast<DWORD>(modulePathBuffer.size()));
    if (length == 0) {
      showError(L"Could not determine the installer path.");
      return 1;
    }
    modulePathBuffer.resize(length);
    const std::filesystem::path selfPath(modulePathBuffer);

    const std::filesystem::path installDir =
        localAppDataPath() / L"CubeOS" / L"versions" / kReleaseTag;
    std::filesystem::create_directories(installDir);

    const std::filesystem::path tempZip =
        std::filesystem::temp_directory_path() / L"cubeos-snapshot-payload.zip";
    std::wstring error;
    if (!extractPayload(selfPath, tempZip, error)) {
      showError(error);
      return 1;
    }

    const std::wstring powershellCommand =
        L"powershell -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "
        L"\"Expand-Archive -LiteralPath "
        + quoteForPowerShell(tempZip.wstring()) + L" -DestinationPath "
        + quoteForPowerShell(installDir.wstring()) + L" -Force\"";
    if (!runProcessAndWait(powershellCommand, error)) {
      std::error_code ignored;
      std::filesystem::remove(tempZip, ignored);
      showError(error);
      return 1;
    }

    std::error_code ignored;
    std::filesystem::remove(tempZip, ignored);

    const std::filesystem::path gameExecutable = installDir / kGameExecutable;
    if (!std::filesystem::exists(gameExecutable)) {
      showError(L"CubeOS was extracted, but the game executable was not found.");
      return 1;
    }

    if (!launchDetached(gameExecutable, installDir, error)) {
      showError(error);
      return 1;
    }

    return 0;
  } catch (const std::exception&) {
    showError(L"CubeOS snapshot installation failed unexpectedly.");
    return 1;
  }
}
