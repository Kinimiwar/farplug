#pragma once

#include <windows.h>
#include <msi.h>
#include <winhttp.h>
#include <shobjidl.h>
#undef max

#include <assert.h>
#include <process.h>
#include <time.h>

#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
using namespace std;

#define INITGUID
#include "CPP/7zip/Archive/IArchive.h"
#include "CPP/7zip/IPassword.h"

#include "plugin.hpp"
#include "farkeys.hpp"