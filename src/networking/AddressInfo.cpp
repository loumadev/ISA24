/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project (adopted from the IPK23 project).
 * @copyright Copyright (c) 2023-2024
 */

#pragma once

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#include <vector>
#include <exception>
// #include <format>

#include "inspector.h"

class AddressInfo {
public:
	enum class Family : int {
		IPv4 = AF_INET,
		IPv6 = AF_INET6,
		UNSPECIFIED = AF_UNSPEC
	};

private:
	const char *address = nullptr;
	int port = 0;
	Family family = Family::IPv4;
	size_t size = 0;
	struct sockaddr_in _address;

public:
	AddressInfo() {}

	AddressInfo(int port) {
		this->setAddressInfo(port);
	}

	AddressInfo(const char *address, int port, Family family = Family::IPv4) {
		this->setAddressInfo(address, port, family);
	}

	AddressInfo(struct sockaddr_in address, size_t size = sizeof(address)) {
		this->setAddressInfo(address, size);
	}

	~AddressInfo() {
		// // delete this->address;
	}

	void setAddressInfo(const char *address, int port, Family family = Family::IPv4) {
		this->address = address;
		this->port = port;
		this->family = family;
		this->size = sizeof(this->_address);

		memset(&this->_address, 0, sizeof(this->_address));
		this->_address.sin_family = (int)this->family;
		this->_address.sin_port = htons(this->port);
		if(inet_pton((int)this->family, this->address, &this->_address.sin_addr) <= 0) {
			print_debug("Error: Invalid address (%d)", errno);
			// throw std::runtime_error(std::format("Invalid address ({})", errno));
			throw std::runtime_error("Invalid address (" + std::to_string(errno) + ")");
		}
	}

	void setAddressInfo(struct sockaddr_in address, size_t size = sizeof(address)) {
		this->address = inet_ntoa(address.sin_addr);
		this->port = ntohs(address.sin_port);
		this->family = (Family)address.sin_family;
		this->size = size;

		memset(&this->_address, 0, sizeof(this->_address));
		this->_address.sin_family = address.sin_family;
		this->_address.sin_port = address.sin_port;
		this->_address.sin_addr = address.sin_addr;
	}

	void setAddressInfo(in_addr_t address, int port) {
		this->address = inet_ntoa(*(struct in_addr*)&address);
		this->port = port;
		this->family = Family::IPv4;
		this->size = sizeof(this->_address);

		memset(&this->_address, 0, sizeof(this->_address));
		this->_address.sin_family = (int)this->family;
		this->_address.sin_port = htons(this->port);
		this->_address.sin_addr.s_addr = address;
	}

	void setAddressInfo(int port) {
		struct sockaddr_in address;
		address.sin_family = (int)Family::IPv4;
		address.sin_port = htons(port);
		address.sin_addr.s_addr = INADDR_ANY;
		this->setAddressInfo(address);
	}

	const char* getAddress() {
		return this->address;
	}

	int getPort() {
		return this->port;
	}

	void setPort(int port) {
		this->port = port;
		this->_address.sin_port = htons(port);
	}

	Family getFamily() {
		return this->family;
	}

	size_t getSize() {
		return this->size;
	}

	struct sockaddr* getSocketAddress() {
		return (struct sockaddr*)&this->_address;
	}

	static std::vector<AddressInfo*>* resolve(const char *hostname, Family family = Family::UNSPECIFIED, int socketType = SOCK_STREAM) {
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = (int)family;
		hints.ai_socktype = socketType;

		// Resolve hostname
		if(getaddrinfo(hostname, NULL, &hints, &res) != 0) {
			print_debug("Error: getaddrinfo error (%d)", errno);
			return nullptr;
		}

		// Collect address infos
		std::vector<AddressInfo*> *addressInfos = new std::vector<AddressInfo*>();
		for(struct addrinfo *p = res; p != NULL; p = p->ai_next) {
			AddressInfo *addressInfo = new AddressInfo();
			addressInfo->setAddressInfo(*(struct sockaddr_in*)p->ai_addr, p->ai_addrlen);
			addressInfos->push_back(addressInfo);
		}

		// Free address info structure
		freeaddrinfo(res);

		return addressInfos;
	}
};