#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>

enum class DsStatus
{
	Good,
	Bad,
	Eof,
	BadType,
};

class DsReader
{
public:
	// Load source file
	DsReader(std::string const& file);

	// Returns data, false if type not match
	template<class Type>
	void Read(Type& val);

	// Identical to Read
	template<class Type>
	DsReader& operator>>(Type& val)
	{
		Read(val);
		return *this;
	}

	// Get current status
	DsStatus Status() const;

	// Get current read position
	size_t Position() const;

	// Get section position
	size_t SectionPosition(std::string const& name) const;

	// goto pointer location
	void Goto(size_t position);

	// goto section
	void Goto(std::string const& section);

private:
	// Check if such value is valid
	bool ValueValidate(size_t begin_ptr) const;

	// Read string as number
	template<class Type>
	bool ReadNumber(std::string const& str, Type* output) const;

	// Read as string
	bool ReadString(std::string const& str, std::string* output) const;

	// Handles commands
	bool HandleCommand(std::vector<std::string> const& args);

private:
	enum class PrimitiveType : char;
	struct RawData { PrimitiveType type; std::string value; };
	struct CustomType { std::vector<PrimitiveType> primitives; };
	std::vector<RawData> rdatas;
	std::unordered_map<std::string, size_t> sections;
	std::unordered_map<std::string, CustomType> typelist;
	DsStatus status;
	size_t read_ptr;
};