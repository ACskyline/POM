#include "CommandLineArg.h"

unordered_map<string, CommandLineArg::MultiTypeValue> CommandLineArg::s_NameValueMap;

CommandLineArg::CommandLineArg(string name) : mName(name)
{
	fatalAssertf(mName.size() > 1, "arg name is too short!");
	fatalAssertf(s_NameValueMap.find(mName) == s_NameValueMap.end(), "can't define the same arg twice!");
	s_NameValueMap[mName] = MultiTypeValue();
}

bool CommandLineArg::GetAsString(string& asString)
{
	fatalAssertf(s_NameValueMap.find(mName) != s_NameValueMap.end(), "arg undefined");
	if (s_NameValueMap[mName].mParsed)
	{
		asString = s_NameValueMap[mName].mAsString;
		return true;
	}
	return false;
}

bool CommandLineArg::GetAsFloatVec(vector<float>& asFloatVec)
{
	fatalAssertf(s_NameValueMap.find(mName) != s_NameValueMap.end(), "arg undefined");
	if (s_NameValueMap[mName].mParsed && s_NameValueMap[mName].mAsFloatVec.size() > 0)
	{
		asFloatVec = s_NameValueMap[mName].mAsFloatVec;
		return true;
	}
	return false;
}

int CommandLineArg::GetAsInt()
{
	fatalAssertf(s_NameValueMap.find(mName) != s_NameValueMap.end(), "arg undefined");
	if (s_NameValueMap[mName].mParsed && s_NameValueMap[mName].mAsFloatVec.size() > 0)
	{
		return s_NameValueMap[mName].mAsFloatVec[0];
	}
	return 0;
}

bool CommandLineArg::Get()
{
	fatalAssertf(s_NameValueMap.find(mName) != s_NameValueMap.end(), "arg undefined");
	return s_NameValueMap[mName].mParsed;
}

vector<string> CommandLineArg::ParseArgs(const char* cstr)
{
	vector<string> argv;
	string str(cstr);
	stringstream fss;
	fss << str; 
	string line;
	while (getline(fss, line))
	{
		stringstream ss;
		ss << line;
		string arg;
		ss >> arg;
		if (arg[0] == '#') // if '#' is present ignore the rest of the line
			continue;
		size_t filePos = arg.find("@");
		if (filePos != string::npos) // file
		{
			string filePathName = arg.substr(filePos + 1);
			auto lambda = [&](const char* buf) {
				vector<string> argFile = ParseArgs(buf);
				argv.insert(argv.end(), argFile.begin(), argFile.end());
			};
			ReadFile(filePathName, lambda);
		}
		else
		{
			argv.push_back(arg);
		}
	}
	return argv;
}

void CommandLineArg::ReadArgs(const vector<string>& argv)
{
	for (int i = 0; i < argv.size(); i++)
	{
		const string& line = argv[i];
		size_t valuePos = line.find('=');
		string name = line.substr(0, valuePos);
		if (name[0] == '-')
		{
			MultiTypeValue mtv;
			mtv.mParsed = true;
			if (valuePos != string::npos && line.substr(valuePos + 1).size()) // has equal sign
			{
				mtv.mAsString = line.substr(valuePos + 1);
				stringstream ss;
				ss << mtv.mAsString;
				char peek;
				float fValue;
				do
				{
					ss >> peek;
					if (!ss.eof() && (peek == '+' || peek == '-' || (peek >= '0' && peek <= '9')))
					{
						if (peek == '+' || peek == '-')
						{
							char peek2;
							ss >> peek2;
							if (peek2 >= '0' && peek2 <= '9')
								ss.putback(peek2);
							else
								break;
						}
						ss.putback(peek);
						ss >> fValue;
						mtv.mAsFloatVec.push_back(fValue);
					}
					else if (peek == ',')
					{
						if (mtv.mAsFloatVec.size() == 0)
							break; // if comma is the first char, we break
					}
				} while (!ss.eof());
			}
			if (verifyf(s_NameValueMap.find(name) != s_NameValueMap.end(), "CommandLineArg [%s] is not used", name.c_str()))
				s_NameValueMap[name] = mtv;
		}
	}

	for (auto nameValue : s_NameValueMap)
	{
		if (nameValue.second.mParsed)
		{
			if (nameValue.second.mAsString.size())
				displayfln("[%s=%s]", nameValue.first.c_str(), nameValue.second.mAsString.c_str());
			else
				displayfln("[%s]", nameValue.first.c_str());
		}
	}
}
