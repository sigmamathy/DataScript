#include "DataScriptReader.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

enum : char
{
	DsInt,
	DsUint,
	DsLong,
	DsUlong,
	DsFloat,
	DsDouble,
	DsString
};

enum : char
{
	DsReadNone,
	DsReadType,
	DsReadTypelength,
	DsReadLabel,
	DsReadString
};

bool andneq(auto&& x, auto&&... args) { return ((x != args) && ...); }
bool oreq(auto&& x, auto&&... args) { return ((x == args) || ...); }

static char DsStringToType(std::string const& str)
{
	static std::unordered_map<std::string, char> lookup =
	{
		{ "int", DsInt },
		{ "uint", DsUint },
		{ "long", DsLong },
		{ "ulong", DsUlong },
		{ "float", DsFloat },
		{ "double", DsDouble },
		{ "string", DsString }
	};
	return lookup.count(str) ? lookup.at(str) : -1;
}

template<class Type>
static bool ReadNumber(std::string const& str, Type* var)
{
	if (std::is_unsigned_v<Type> && str[0] == '-')
		return false;
	std::stringstream ss;
	ss << str;
	Type x;
	ss >> x;
	if (!ss.eof() || ss.fail()) return false;
	if (var) *var = x;
	return true;
}

static bool ReadString(std::string const& str, std::string* output)
{
	if (str.length() < 2 || str[0] != '"' || str.back() != '"') return false;
	if (output) *output = str.substr(1, str.length() - 2);
	return true;
}

static bool ValueValidate(char type, std::string const& val)
{
	switch (type)
	{
		case DsInt:		return ReadNumber<int>(val, nullptr);
		case DsUint:	return ReadNumber<unsigned>(val, nullptr);
		case DsLong:	return ReadNumber<long long>(val, nullptr);
		case DsUlong:	return ReadNumber<unsigned long long>(val, nullptr);
		case DsFloat:	return ReadNumber<float>(val, nullptr);
		case DsDouble:	return ReadNumber<double>(val, nullptr);
		case DsString:	return ReadString(val, nullptr);
		default:		return false;
	}
}

DsReader::DsReader(std::string const& filepath)
	: state(0), read_pointer(0)
{
#define PreValidate(x, msg) if (!(x)) { state |= DsBad; message = msg; return; }
	std::ifstream ifs(filepath);
	PreValidate(ifs.is_open(), "Cannot open file \"" + filepath + "\"");

	char readmode = DsReadNone;
	unsigned line = 1u;
#define Validate(x, msg) PreValidate(x, msg " [line:" + std::to_string(line) + "]")
	raw_datas.reserve(100);
	std::string type_s, typelength_s, label_s;
	std::vector<char> types;
	types.reserve(10);
	unsigned type_offset = 0u;
	bool comment = false;

	for (char prev = '\0', temp; ifs.get(temp); prev = temp)
	{
		// Reading string
		if (readmode == DsReadString)
		{
			if (temp == '\n') ++line;
			if (temp != '\\' || prev == '\\')
				raw_datas.back().value += temp;
			if (temp == '"' && prev != '\\')
				readmode = DsReadNone;
			continue;
		}
		// Comments mode
		if (comment)
		{
			temp = prev;
			if (temp != '\n') continue;
			++line;
			comment = false;
			continue;
		}
		// Check characters
		switch (temp)
		{
		case '(':
			// Reading type begin
			Validate(readmode == DsReadNone, "Unexpected '(' found");
			Validate(type_offset == 0u, "Numbers of value does not match the type defined");
			if (andneq(prev, ' ', '\t', ',', '\n', ')') && !raw_datas.empty())
				Validate(ValueValidate(raw_datas.back().type, raw_datas.back().value), "Invalid value found");
			readmode = DsReadType;
			type_s = "";
			typelength_s = "";
			types.clear();
			type_offset = 0u;
			break;

		case ')':
			// Reading type end
			if (readmode == DsReadType)
			{
				if (andneq(prev, ' ', '\t', ',', '('))
					Validate(types.emplace_back(DsStringToType(type_s)) != -1, "Invalid typename found");
			}
			else if (readmode == DsReadTypelength)
			{
				std::stringstream ss;
				ss << typelength_s;
				unsigned x;
				ss >> x;
				Validate(ss.eof() && !ss.fail() && x > 0u, "Invalid typelength found");
				for (unsigned i = 0; i < (x - 1); i++)
					types.emplace_back(types.back());
			}
			else
				Validate(false, "Unexpected ')' found");
			readmode = DsReadNone;
			break;

		case '[':
			Validate(readmode == DsReadNone, "Unexpected '[' found");
			label_s = "";
			readmode = DsReadLabel;
			break;

		case ']':
			Validate(readmode == DsReadLabel, "Unexpected ']' found");
			Validate(!labels.count(label_s), "Label redefinition");
			labels[label_s] = (unsigned) raw_datas.size();
			readmode = DsReadNone;
			break;

		case '#':
			comment = true;
			break;

		case '\n':
			// new line character
			++line;
			if (readmode == DsReadType && andneq(prev, ' ', '\t', ',', '(', '\n'))
			{
				types.emplace_back(DsStringToType(type_s));
				Validate(types.back() != -1, "Invalid typename found");
				type_s = "";
			}
			else if (readmode == DsReadTypelength && andneq(prev, ' ', '\t', ',', ':', '\n'))
			{
				std::stringstream ss;
				ss << typelength_s;
				unsigned x;
				ss >> x;
				Validate(ss.eof() && !ss.fail() && x > 0u, "Invalid typelength found");
				for (unsigned i = 0; i < (x - 1); i++)
					types.emplace_back(types.back());
				typelength_s = "";
				readmode = DsReadType;
			}
			break;

		default:
			// Reading normal characters
			switch (readmode)
			{
			case DsReadType:
				// Reading type
				if (temp == ':' || (oreq(temp, ' ', '\t', ',') && andneq(prev, ' ', '\t', ',', '(', '\n')))
				{
					if (temp == ':')
					{
						readmode = DsReadTypelength;
						if (oreq(prev, ' ', '\t', ',', '\n'))
							break;
					}
					types.emplace_back(DsStringToType(type_s));
					Validate(types.back() != -1, "Invalid typename found");
					type_s = "";
					break;
				}
				if (andneq(temp, ' ', '\t', ','))
					type_s += temp;
				break;

			case DsReadTypelength:
				// Reading type length
				if (oreq(temp, ' ', '\t', ',', '\n') && andneq(prev, ' ', '\t', ',', ':', '\n'))
				{
					std::stringstream ss;
					ss << typelength_s;
					unsigned x;
					ss >> x;
					Validate(ss.eof() && !ss.fail() && x > 0u, "Invalid typelength found");
					for (unsigned i = 0; i < (x - 1); i++)
						types.emplace_back(types.back());
					typelength_s = "";
					readmode = DsReadType;
				}
				if (andneq(temp, ' ', '\t', ',', '\n'))
					typelength_s += temp;
				break;

			case DsReadLabel:
				Validate(temp != '\n', "newline character is not allowed in label declaration")
				label_s += temp;
				break;

			case DsReadNone:
				// Reading values
				if (oreq(temp, ' ', '\t', ','))
					break;
				if (oreq(prev, ' ', '\t', ',', '\n', ')', ']'))
				{
					if (!raw_datas.empty())
						Validate(ValueValidate(raw_datas.back().type, raw_datas.back().value), "Invalid value found");
					Validate(!types.empty(), "value cannot be determined while type being empty");
					raw_datas.emplace_back(types[type_offset++], "").value.reserve(10);
					if (type_offset == types.size()) type_offset = 0u;
				}
				raw_datas.back().value += temp;
				if (temp == '"')
					readmode = DsReadString;
				break;
			}
		}
	}

	Validate(type_offset == 0u, "Numbers of value does not match the type defined");
	Validate(readmode == DsReadNone, "Statements not enclosed");
	if (raw_datas.empty()) state |= DsEof;
	else Validate(ValueValidate(raw_datas.back().type, raw_datas.back().value), "Invalid value found");
#undef PreValidate
#undef Validate
	ifs.close();
}

DsReader::operator unsigned char() const
{
	return state;
}

std::string DsReader::Message() const
{
	return message;
}

void DsReader::Goto(unsigned location)
{
	read_pointer = location;
	if (location >= raw_datas.size())
		state |= DsEof;
}

unsigned DsReader::Pointer() const
{
	return read_pointer;
}

void DsReader::Goto(std::string const& label)
{
	if (!labels.count(label)) return;
	read_pointer = labels.at(label);
	if (read_pointer >= raw_datas.size())
		state |= DsEof;
}

unsigned DsReader::Label(std::string const& name) const
{
	return labels.count(name) ? labels.at(name) : static_cast<unsigned>(-1);
}

#define DefineReadOperator(ty, ety, method) template<>\
DsReader& DsReader::operator>>(ty& var) {\
	if (state & DsBad || state & DsEof) return *this;\
	if (raw_datas[read_pointer].type != ety) { state |= DsBadtype; return *this; }\
	method(raw_datas[read_pointer].value, &var); ++read_pointer;\
	if (read_pointer >= raw_datas.size()) state |= DsEof;\
	return *this;\
}

DefineReadOperator(int, DsInt, ReadNumber);
DefineReadOperator(unsigned, DsUint, ReadNumber);
DefineReadOperator(long long, DsLong, ReadNumber);
DefineReadOperator(unsigned long long, DsUlong, ReadNumber);
DefineReadOperator(float, DsFloat, ReadNumber);
DefineReadOperator(double, DsDouble, ReadNumber);
DefineReadOperator(std::string, DsString, ReadString);

#undef DefineReadOperator