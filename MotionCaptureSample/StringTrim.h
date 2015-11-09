// Reference: http://www.codeproject.com/Articles/10880/A-trim-implementation-for-std-string

#pragma once
#include <algorithm>
#include <cctype>
#include <string>

using namespace std;

static inline string& Trim(string& str)
{
	str.erase(str.begin(), find_if(str.begin(), str.end(),
		[](char& ch)->bool { return !isspace(ch); }));
	str.erase(find_if(str.rbegin(), str.rend(),
		[](char& ch)->bool { return !isspace(ch); }).base(), str.end());
	return str;
}