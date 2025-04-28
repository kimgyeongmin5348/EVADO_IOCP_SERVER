#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "pch.h"
#include <WinSock2.h>
#include <mswsock.h>
#include <unordered_map>
#include <WS2tcpip.h>
#include <atomic>
#include <algorithm>
#include <shared_mutex>

#include <thread>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <math.h>

#include <string>
#include <wrl.h>
#include <shellapi.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iomanip>


// DirectX ฐüทร 
#include <d3d12.h>  //
#include <DirectXMath.h>
#include <DirectXPackedVector.h> //
#include <DirectXCollision.h> // 


#pragma comment (lib, "WS2_32.LIB")
#pragma comment (lib, "MSWSock.LIB")



using namespace DirectX;