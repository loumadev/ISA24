/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <stdexcept>
// #include <format>

#include "stream/Readable.cpp"
#include "stream/Writable.cpp"

#include "inspector.h"


class ReadableString : public Readable {
private:
	const char *data;
	size_t size;
	size_t position;

public:
	ReadableString(const char *data, size_t size) {
		this->data = data;
		this->size = size;
		this->position = 0;
	}

	int readByte() {
		// Reading past the end of the string
		if(this->position >= this->size) {
			return -1;
		}

		return this->data[this->position++];
	}

	size_t read(char *buffer, size_t size) {
		size_t bytesRead = 0;

		while(bytesRead < size) {
			int byte = this->readByte();

			// EOF reached
			if(byte == -1) break;

			buffer[bytesRead++] = byte;
		}

		return bytesRead;
	}

	void close() {
		// Nothing to do
	}
};
