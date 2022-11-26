#include "./DataScriptReader.hpp"
#include <iostream>
#include <fstream>

enum class DsReader::PrimitiveType : char
{
	INT,
	UINT,
	LONG,
	ULONG,
	FLOAT,
	DOUBLE,
	STRING
};

DsReader::DsReader(std::string const& file)
	: read_ptr(0)
{
#define VALIDATE(x) if (!(x)) { status = DsStatus::Bad; return; }
	std::ifstream ifs(file);
	VALIDATE(ifs.is_open());
	status = DsStatus::Good;

	typelist.reserve(10);
	typelist["int"].primitives = { PrimitiveType::INT };
	typelist["uint"].primitives = { PrimitiveType::UINT };
	typelist["long"].primitives = { PrimitiveType::LONG };
	typelist["ulong"].primitives = { PrimitiveType::ULONG };
	typelist["float"].primitives = { PrimitiveType::FLOAT };
	typelist["double"].primitives = { PrimitiveType::DOUBLE };
	typelist["string"].primitives = { PrimitiveType::STRING };
	unsigned type_prim_offset = 0u;

	char temp;
	std::string type = "";
	type.reserve(10);
	size_t typelength = 1, begin_ptr = 0;
	enum class ReadMode { None, Type, TypeLength, Value, StringValue, ControlStatement }
	readmode = ReadMode::None;
	constexpr auto is_alphabet_char = [](char c) -> bool { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
	bool on_line_begin = true;
	std::vector<std::string> commands;
	commands.reserve(10);

	for (char prev = '\0'; ifs.get(temp); prev = temp)
	{
#define STRING_MODE_ACTION if (readmode == ReadMode::StringValue) { rdatas.back().value += temp; break; }
		switch (temp)
		{
		case '[': // begin type read
			STRING_MODE_ACTION;
			VALIDATE(readmode == ReadMode::None);
			readmode = ReadMode::Type;
			type = "";
			typelength = 1;
			break;

		case ']': // end type read
			STRING_MODE_ACTION;
			VALIDATE(readmode == ReadMode::Type || readmode == ReadMode::TypeLength);
			readmode = ReadMode::None;
			VALIDATE(typelist.count(type));
			type_prim_offset = 0u;
			break;

		case '(': // begin value read
			STRING_MODE_ACTION;
			VALIDATE(readmode == ReadMode::None);
			readmode = ReadMode::Value;
			break;

		case ')': // end value read
			STRING_MODE_ACTION;
			VALIDATE(readmode == ReadMode::Value);
			VALIDATE((!typelength && (rdatas.size() - begin_ptr) % typelist[type].primitives.size() == 0)
				|| rdatas.size() - begin_ptr == typelength * typelist[type].primitives.size());
			VALIDATE(ValueValidate(begin_ptr));
			begin_ptr = rdatas.size();
			readmode = ReadMode::None;
			break;

		case '\n':
			on_line_begin = true;
			if (readmode == ReadMode::ControlStatement)
			{
				HandleCommand(commands);
				readmode = ReadMode::None;
			}
			break;

		case '#':
			STRING_MODE_ACTION;
			VALIDATE(on_line_begin && readmode == ReadMode::None);
			readmode = ReadMode::ControlStatement;
			commands.clear();
			break;

		default:
			// read characters
			switch (readmode)
			{
				using enum ReadMode;

			case Type:
				if (temp == ' ') break;
				if (temp == ':') { readmode = TypeLength; typelength = 0; break; }
				VALIDATE(prev != ' ' || type.empty());
				type += temp;
				break;

			case TypeLength:
				if (temp == ' ') break;
				VALIDATE(temp >= '0' && temp <= '9');
				typelength = typelength * 10 + temp - '0';
				break;

			case Value:
				if (temp == ' ' || temp == ',' || temp == '\t') break;
				if (prev == '\n' || prev == '\t' || prev == '(' || prev == ' ' || prev == ',')
				{
					rdatas.emplace_back(typelist.at(type).primitives[type_prim_offset++], "")
						.value.reserve(30);
					if (type_prim_offset == typelist.at(type).primitives.size()) type_prim_offset = 0u;
				}
				rdatas.back().value += temp;
				if (temp == '"')
					readmode = StringValue;
				break;

			case StringValue:
				if (temp != '\\' || prev == '\\')
					rdatas.back().value += temp;
				if (temp == '"' && prev != '\\')
					readmode = Value;
				break;

			case ControlStatement:
				if (temp == ' ' || temp == '\t' || temp == ',') break;
				if (prev == ' ' || prev == '\t' || prev == ',' || prev == '#')
					commands.emplace_back("").reserve(10);
				commands.back() += temp;
				break;
			}
		}
		if (temp != ' ' && temp != '\t' && temp != '\n')
			on_line_begin = false;
	}
	VALIDATE(readmode == ReadMode::None);
	if (rdatas.empty())
		status = DsStatus::Eof;
#undef VALIDATE
#undef STRING_MODE_ACTION
	ifs.close();
}

DsStatus DsReader::Status() const
{
	return status;
}

size_t DsReader::Position() const
{
	return read_ptr;
}

bool DsReader::ValueValidate(size_t begin_ptr) const
{
	for (size_t i = begin_ptr; i < rdatas.size(); i++)
	{
		bool good = false;
		switch (rdatas[i].type)
		{
			using enum PrimitiveType;
		case INT:		good = ReadNumber<int>(rdatas[i].value, nullptr); break;
		case UINT:		good = ReadNumber<unsigned>(rdatas[i].value, nullptr); break;
		case LONG:		good = ReadNumber<long long>(rdatas[i].value, nullptr); break;
		case ULONG:		good = ReadNumber<unsigned long long>(rdatas[i].value, nullptr); break;
		case FLOAT:		good = ReadNumber<float>(rdatas[i].value, nullptr); break;
		case DOUBLE:	good = ReadNumber<double>(rdatas[i].value, nullptr); break;
		case STRING:	good = ReadString(rdatas[i].value, nullptr); break;
		}

		if (!good) return false;
	}
	return true;
}

template<class Type>
bool DsReader::ReadNumber(std::string const& str, Type* output) const
{
	if (std::is_unsigned_v<Type> && str[0] == '-')
		return false;
	std::stringstream ss;
	ss << str;
	Type x;
	ss >> x;
	if (!ss.eof() || ss.fail()) return false;
	if (output) *output = x;
	return true;
}

bool DsReader::ReadString(std::string const& str, std::string* output) const
{
	if (str.length() < 2 || str[0] != '"' || str.back() != '"') return false;
	if (output) *output = str.substr(1, str.length() - 2);
	return true;
}

bool DsReader::HandleCommand(std::vector<std::string> const& args)
{
	if (args.empty())
		return false;
	if (args[0] == "section")
	{
		if (args.size() != 2) return false;
		sections[args[1]] = rdatas.size();
		return true;
	}
	if (args[0] == "typedef")
	{
		if (args.size() < 4 && args[2] != "=") return false;
		auto& x = typelist[args[1]];
		x.primitives.reserve(20);
		for (int i = 3; i < args.size(); i++)
		{
			if (!typelist.count(args[i]))
				return false;
			for (auto& y : typelist.at(args[i]).primitives)
				x.primitives.emplace_back(y);
		}
		return true;
	}

	return false;
}

size_t DsReader::SectionPosition(std::string const& name) const
{
	return sections.count(name) ? sections.at(name) : static_cast<size_t>(-1);
}

void DsReader::Goto(size_t position)
{
	read_ptr = position;
	if (position >= rdatas.size() && status != DsStatus::Bad)
		status = DsStatus::Eof;
}

void DsReader::Goto(std::string const& name)
{
	if (!sections.count(name)) return;
	read_ptr = sections.at(name);
	if (read_ptr >= rdatas.size() && status != DsStatus::Bad)
		status = DsStatus::Eof;
}

#define READ_IMPL(ty, pty, method) template<> void DsReader::Read<ty>(ty& val)\
	{ if (status == DsStatus::Bad || status == DsStatus::Eof) return;\
	if (rdatas[read_ptr].type != PrimitiveType::pty) { status = DsStatus::BadType; return; }\
	method(rdatas[read_ptr++].value, &val); if (read_ptr == rdatas.size()) status = DsStatus::Eof; }

READ_IMPL(int, INT, ReadNumber);
READ_IMPL(unsigned, UINT, ReadNumber);
READ_IMPL(long, INT, ReadNumber);
READ_IMPL(unsigned long, UINT, ReadNumber);
READ_IMPL(long long, LONG, ReadNumber);
READ_IMPL(unsigned long long, ULONG, ReadNumber);
READ_IMPL(float, FLOAT, ReadNumber);
READ_IMPL(double, DOUBLE, ReadNumber);
READ_IMPL(std::string, STRING, ReadString);

#undef READ_IMPL