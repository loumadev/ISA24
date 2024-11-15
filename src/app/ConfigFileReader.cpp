/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <map>
#include <string>

#include "stream/FileStream.cpp"
#include "stream/BufferedReader.cpp"

class ConfigFileReader {
public:
	std::map<std::string, std::string> entries;
	FileStream *fileStream;
	BufferedReader *bufferedReader;

public:
	ConfigFileReader(const char *path) {
		this->fileStream = new FileStream(path, "r");
		this->bufferedReader = new BufferedReader(this->fileStream);

		this->parse();
	}

	~ConfigFileReader() {
		delete this->bufferedReader;
		delete this->fileStream;
	}

	void parse() {
		char buffer[1024];
		while(true) {
			ssize_t bytesRead = this->bufferedReader->readLine(buffer, sizeof(buffer));
			if(bytesRead == -1) break; // EOF
			if(bytesRead == 0) continue; // Empty line

			std::string line(buffer, bytesRead);

			const char delim[] = " = ";

			size_t pos = line.find(delim); // TODO: Need to verify: is the space necessary?
			if(pos == std::string::npos) continue; // TODO: Maybe throw an error?

			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + sizeof(delim) - 1);

			this->entries[key] = value;
		}
	}
};