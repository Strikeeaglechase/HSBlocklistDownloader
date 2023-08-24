#include <iostream>
#include "HTTPRequest.hpp"
#include <fstream>
#include "trim.h"

struct BannedUser {
    std::string userId;
    std::string name;
    std::string steamName;
};

std::vector<BannedUser> parseBanList(const std::string& banlistRaw){
    std::vector<BannedUser> banList;

    int currentIndex = 0;
    while(true) {
        auto nextIdx = banlistRaw.find('\n',currentIndex + 1);
        if(nextIdx == std::string::npos) break;

        std::string user = banlistRaw.substr(currentIndex, nextIdx - currentIndex);
        auto commaPos = user.find(',');
        auto id = trim(user.substr(0,commaPos));
        auto name = trim(user.substr(commaPos + 1));
        banList.push_back({.userId = id, .name = name, .steamName = name});

        currentIndex = (int)nextIdx;
    }

    return banList;
}

std::vector<BannedUser> readUserExistingBanList() {
    std::vector<BannedUser> banList;

    std::ifstream file;

    char* appdata = getenv("APPDATA");
    std::string appdataPath{appdata};
    std::cout << "Checking " << appdataPath << "\\Boundless Dynamics, LLC\\VTOLVR\\SaveData\\blocklist.cfg  for blocklist" << std::endl;
    file.open(appdataPath +  "\\Boundless Dynamics, LLC\\VTOLVR\\SaveData\\blocklist.cfg");
    if(!file || !file.is_open()) {
        std::cout << "No existing blocklist" << std::endl;
        return banList;
    }


    std::string line;
    std::string steamName;
    std::string pilotName;
    std::string id;
    int depth = 0;

    while(std::getline(file, line)) {
        if(line.find("steamName") != std::string::npos) steamName = line.substr(line.find('=') + 2);
        else if(line.find("pilotName") != std::string::npos) pilotName = line.substr(line.find('=') + 2);
        else if(line.find("id") != std::string::npos) id = line.substr(line.find('=') + 2);
        else if(line.find('{') != std::string::npos) depth++;
        else if(line.find('}') != std::string::npos) {
            depth--;
            if(depth == 1) {
                banList.push_back({.userId=id, .name=pilotName, .steamName=steamName});
            }
        }
    }


    return banList;
}

std::vector<std::string> readUserWhitelist() {
    std::vector<std::string> whitelist;

    std::ifstream file;

    char* appdata = getenv("APPDATA");
    std::string appdataPath{appdata};
    std::cout << "Checking " << appdataPath << "\\Boundless Dynamics, LLC\\VTOLVR\\SaveData\\allowlist.txt  for allowlist" << std::endl;
    file.open(appdataPath +  "\\Boundless Dynamics, LLC\\VTOLVR\\SaveData\\allowlist.txt");
    if(!file || !file.is_open()) {
        std::cout << "No user whitelist" << std::endl;
        return whitelist;
    }

    std::string line;
    while(std::getline(file, line)) {
        whitelist.push_back(trim(line));
    }

    return whitelist;
}

std::vector<BannedUser> createFinalBanList(const std::vector<BannedUser>& hsBanList, const std::vector<BannedUser>& currentUserList, const std::vector<std::string>& whitelist) {
    std::vector<BannedUser> finalBanList{currentUserList};

    for(const auto& bannedUser : hsBanList) {
        // Is banned user whitelisted?
        bool isWhitelisted = std::find(whitelist.begin(), whitelist.end(), bannedUser.userId) != whitelist.end();
        if (isWhitelisted){
            std::cout << "User " << bannedUser.name << " (" << bannedUser.userId << ") is whitelisted! Not blocking" << std::endl;
        } else {
            bool isAlreadyBlocked = false;
            for(const auto& currentlyBannedUser : finalBanList) {
                if(currentlyBannedUser.userId == bannedUser.userId) {
                    isAlreadyBlocked = true;
                    break;
                }
            }

            if(!isAlreadyBlocked) finalBanList.push_back(bannedUser);
        }
    }

    return finalBanList;
}

void writeBlocklist(const std::vector<BannedUser>& bannedUsers) {
    char* appdata = getenv("APPDATA");
    std::string appdataPath{appdata};
    std::ofstream output(appdataPath +  "\\Boundless Dynamics, LLC\\VTOLVR\\SaveData\\blocklist.cfg", std::ofstream::trunc);

    output << "NODE\n{\n";
    for(const auto& bannedUser : bannedUsers) {
        output << "\tUSER\n\t{\n";
        output << "\t\tid = " << bannedUser.userId << "\n";
        output << "\t\tsteamName = " << bannedUser.steamName << "\n";
        output << "\t\tpilotName = " << bannedUser.name << "\n";
        output << "\t}\n";
    }

    output << "}";

    output.flush();
    output.close();
}

int main() {
    std::cout << "Sending request..." << std::endl;

    http::Request request{"http://hs.vtolvr.live/api/v1/public/bannedraw"};
    const auto response = request.send("GET");


    std::string banlistRaw = std::string{response.body.begin(), response.body.end()};
    std::cout << "Got reply with " << banlistRaw.length() << " bytes" << std::endl;

    auto banList = parseBanList(banlistRaw);

    std::cout << "Resolved " << banList.size() << " banned users" << std::endl;

    auto existingBans = readUserExistingBanList();
    std::cout << "Found " << existingBans.size() << " users in existing blocklist" << std::endl;

    auto userWhitelist = readUserWhitelist();
    std::cout << "Found " << userWhitelist.size() << " users in existing whitelist" << std::endl;

    auto finalBanList = createFinalBanList(banList, existingBans, userWhitelist);
    std::cout << "Final ban list has " << finalBanList.size() << " users" << std::endl;

    writeBlocklist(finalBanList);

    return 0;
}
