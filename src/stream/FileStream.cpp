/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
// #include <format>

#include "stream/Readable.cpp"
#include "stream/Writable.cpp"

#include "inspector.h"


class FileStream : public Readable, public Writable {
private:
	FILE *file;
public:
	FileStream(const char *path, const char *mode) {
		this->file = ::fopen(path, mode);
		if(this->file == NULL) {
			print_debug("Error: Failed to open file (%d)", errno);
			// throw std::runtime_error(std::format("Failed to open file '{}' ({})", path, errno));
			throw std::runtime_error("Failed to open file '" + std::string(path) + "'");
		}
	}

	FileStream(FILE *file) {
		this->file = file;
	}

	~FileStream() {
		this->close();
	}

	size_t read(char *buffer, size_t size) {
		ssize_t bytesRead = ::fread(buffer, 1, size, this->file);

		if(bytesRead == -1) {
			print_debug("Error: Failed to read from file (%d)", errno);
			// throw std::runtime_error(std::format("Failed to read from file ({})", errno));
			throw std::runtime_error("Failed to read from file (" + std::to_string(errno) + ")");
		}

		return bytesRead;
	}

	void write(const char *buffer, size_t size) {
		if(::fwrite(buffer, 1, size, this->file) != size) {
			print_debug("Error: Failed to write to file (%d)", errno);
			// throw std::runtime_error(std::format("Failed to write to file ({})", errno));
			throw std::runtime_error("Failed to write to file (" + std::to_string(errno) + ")");
		}
	}

	void close() {
		::fclose(this->file);
	}

	static FileStream open(const char *path, const char *mode) {
		return FileStream(path, mode);
	}
};
