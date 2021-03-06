// ini.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <iostream>
#include <assert.h>
#include "scope_guard.h"

void read_data(const wchar_t* file_name)
{
	HANDLE hFile = nullptr;
	HANDLE hFileMapping = nullptr;
	LPVOID lpBaseAddress = nullptr;

	//////////////////////////////////////////////////////////////////////////
	// Open file 
	hFile = ::CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE) {
		assert(false && "Could not open file");
		throw std::exception("Could not open file");
	}
	scope_guard guard; 
	guard += [&hFile]() {
		if(hFile != INVALID_HANDLE_VALUE) {
			::CloseHandle(hFile);
		}
	};


	//////////////////////////////////////////////////////////////////////////
	// Mapping Given file to Memory
	hFileMapping = ::CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if(hFileMapping == NULL) {
		assert(false && "Could not map file exe");
		throw std::exception("Could not map file exe");
	}
	guard += [&hFileMapping]() {
		if(hFileMapping != NULL) {
			::CloseHandle(hFileMapping);
		}
	};

	lpBaseAddress = ::MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if(lpBaseAddress == NULL) {
		assert(false && "Map view of file fail");
		throw std::exception("Map view of file fail");
	}
	guard += [&lpBaseAddress]() {
		if(lpBaseAddress != NULL) {
			::UnmapViewOfFile(lpBaseAddress);
		}
	};

	//////////////////////////////////////////////////////////////////////////


	// https://docs.microsoft.com/en-us/windows/desktop/FileIO/obtaining-directory-change-notifications
}

int main()
{
	
    std::cout << "Hello World!\n"; 
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
