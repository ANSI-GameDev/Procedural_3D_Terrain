#include "ANSIException.h"
#include <sstream>


ANSIException::ANSIException(int line, const char* file) noexcept
	:
	line(line),
	file(file)
{}

const char* ANSIException::what() const noexcept
{
	// dangling pointer 문제 피하기 위해, 멤버 변수 (whatBuffer)에 복사 후 반환
	std::ostringstream oss;
	oss << GetType() << std::endl
		<< GetOriginString();
	whatBuffer = oss.str();
	return whatBuffer.c_str();
}

const char* ANSIException::GetType() const noexcept
{
	return "ANSI Exception";
}

int ANSIException::GetLine() const noexcept
{
	return line;
}

const std::string& ANSIException::GetFile() const noexcept
{
	return file;
}

std::string ANSIException::GetOriginString() const noexcept
{
	std::ostringstream oss;
	oss << "[File] " << file << std::endl
		<< "[Line] " << line;
	return oss.str();
}