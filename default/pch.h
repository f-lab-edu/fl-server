#pragma once

#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "mswsock.lib")
#include <mswsock.h>

#include "spdlog/spdlog.h"

#include <string>
#include <span>
#include <vector>
#include <array>
#include <list>
#include <queue>
#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <mutex>
#include <cstring>
#include <utility>
#include <functional>
#include <ctime>
#include <variant>
#include <iostream>
#include <memory>

using namespace std;

#include "Server_Defines.h"
#include "Define.h"
#include "Enums.h"