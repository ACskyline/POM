#pragma once
#include "GlobalInclude.h"
#include "Renderer.h"
#include <unordered_map>

class CommandLineArg
{
public:
	CommandLineArg(string name);

	bool GetAsString(string& asString);
	bool GetAsFloatVec(vector<float>& asFloatVec);
	bool GetAsInt(int& asInt);
	int GetAsInt();
	bool Get();

	static vector<string> ParseArgs(const char* cstr);
	static void ReadArgs(const vector<string>& argv);

private:
	string mName;
	struct MultiTypeValue
	{
		vector<float> mAsFloatVec;
		string mAsString;
		bool mParsed = false;
	};
	static unordered_map<string, MultiTypeValue> s_NameValueMap;
};
