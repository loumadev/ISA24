/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <stddef.h>

class Readable {
public:
	virtual ~Readable() {}
	virtual size_t read(char *buffer, size_t size) = 0;
	virtual void close() = 0;
};
