/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include "networking/AddressInfo.cpp"
#include "stream/Writable.cpp"
#include "stream/Readable.cpp"


class Socket : public Writable, public Readable {
public:
	virtual ~Socket() {}
	virtual void connect(const char *hostname, int port) = 0;
	virtual void close() = 0;
};
