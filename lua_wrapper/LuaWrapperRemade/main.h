#include <Windows.h>
#include <iostream>
#include "rbx_library.h"
#include "rbx_wrap.h"
#include "sigscanner.h"

#define DATAMODEL_VF_OFF		0x37 + 6
#define Check(v, o)				if (!(v)) { \
									std::cout << "[!] Failed to " << o << std::endl; \
									return; \
								}

/*
TO-DO:
	- Fix pcall

*/