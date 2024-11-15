/**
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project.
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "networking/Socket.cpp"
#include "networking/AddressInfo.cpp"
#include "inspector.h"

class TCPSocket : public Socket {
public:
	AddressInfo *addressInfo;
	int fd;

public:
	TCPSocket() {
		this->fd = -1;
	}

	void connect(const char *hostname, int port) {
		// Prevent connecting to already connected socket
		if(this->fd != -1) {
			print_debug("Error: Socket is already connected");
			throw std::runtime_error("Socket is already connected");
		}

		// Resolve hostname
		std::vector<AddressInfo*> *infos = AddressInfo::resolve(hostname);
		if(infos == nullptr) {
			print_debug("Error: Failed to resolve host address info");
			// throw std::runtime_error(std::format("Failed to resolve host address '{}' ({})", hostname, errno));
			throw std::runtime_error("Failed to resolve host address info '" + std::string(hostname) + "'");
		}

		// Check if any address info was found
		if(infos->size() == 0) {
			print_debug("Error: Failed to resolve host address info");
			// throw std::runtime_error(std::format("Failed to resolve host address '{}' (no matching records found)", hostname));
			throw std::runtime_error("Failed to resolve host address info '" + std::string(hostname) + "' (no matching records found)");
		}

		print_debug("Address for %s:%d resolved with %ld candidates", hostname, port, infos->size());

		std::runtime_error lastException("Failed to connect to any address info");

		// Try to connect to each address info
		for(auto info : *infos) {
			// Statically overwrite the resolved port with user specified one
			info->setPort(port);

			try {
				// Try to connect to the server
				this->connect(info);
			} catch(const std::runtime_error &e) {
				// Save the last exception for later
				lastException = std::runtime_error(e.what());

				// Try next address info
				continue;
			}

			// Connection successful
			delete infos;
			return;
		}

		// All address infos failed
		delete infos;
		throw lastException;
	}

	void connect(AddressInfo *info, int timeout_ms = 30000) {
		// Prevent connecting to an already connected socket
		if(this->fd != -1) {
			print_debug("Error: Socket is already connected");
			throw std::runtime_error("Socket is already connected");
		}

		print_debug("Trying to connect to %s:%d...", info->getAddress(), info->getPort());

		// Create a socket descriptor
		int fd = socket((sa_family_t)info->getFamily(), SOCK_STREAM, 0);

		// Check if socket was created
		if(fd == -1) {
			print_debug("Error: Failed to create socket (%d)", errno);
			// throw std::runtime_error(std::format("Failed to create socket ({})", errno));
			throw std::runtime_error("Failed to create socket (" + std::to_string(errno) + ")");
		}

		// Set the socket to non-blocking mode
		int flags = fcntl(fd, F_GETFL, 0);
		if(flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
			::close(fd);
			print_debug("Error: Failed to set non-blocking mode (%d)", errno);
			// throw std::runtime_error(std::format("Failed to set non-blocking mode ({})", errno));
			throw std::runtime_error("Failed to set non-blocking mode (" + std::to_string(errno) + ")");
		}

		// Initiate the connection
		int ret = ::connect(fd, info->getSocketAddress(), info->getSize());
		if(ret == -1 && errno != EINPROGRESS) {
			print_debug("Error: Failed to initiate connection (%d)", errno);
			::close(fd);
			// throw std::runtime_error(std::format("Failed to initiate connection ({})", errno));
			throw std::runtime_error("Failed to initiate connection (" + std::to_string(errno) + ")");
		}

		// Use epoll to wait for the connection to complete
		int epoll_fd = epoll_create1(0);
		if(epoll_fd == -1) {
			::close(fd);
			print_debug("Error: Failed to create epoll instance (%d)", errno);
			// throw std::runtime_error(std::format("Failed to create epoll instance ({})", errno));
			throw std::runtime_error("Failed to create epoll instance (" + std::to_string(errno) + ")");
		}

		struct epoll_event event;
		event.events = EPOLLOUT;
		event.data.fd = fd;

		if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
			::close(fd);
			::close(epoll_fd);
			print_debug("Error: Failed to add socket to epoll (%d)", errno);
			// throw std::runtime_error(std::format("Failed to add socket to epoll ({})", errno));
			throw std::runtime_error("Failed to add socket to epoll (" + std::to_string(errno) + ")");
		}

		// Wait for the event with the specified timeout
		struct epoll_event events;
		int nfds = epoll_wait(epoll_fd, &events, 1, timeout_ms);

		// Check the result of epoll_wait
		if(nfds == 0) {
			print_debug("Error: Connection timed out (%dms)", timeout_ms);
			::close(fd);
			::close(epoll_fd);
			// throw std::runtime_error(std::format("Connection timed out after {}ms", timeout_ms));
			throw std::runtime_error("Connection timed out after " + std::to_string(timeout_ms) + "ms");
		} else if(nfds == -1) {
			print_debug("Error: epoll_wait failed (%d)", errno);
			::close(fd);
			::close(epoll_fd);
			// throw std::runtime_error(std::format("epoll_wait failed ({})", errno));
			throw std::runtime_error("epoll_wait failed (" + std::to_string(errno) + ")");
		}

		// Check if the socket is writable (indicates connection success)
		int so_error = 0;
		socklen_t len = sizeof(so_error);
		if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) == -1 || so_error != 0) {
			print_debug("Error: Connection failed (%d)", so_error);
			::close(fd);
			::close(epoll_fd);
			// throw std::runtime_error(std::format("Connection failed ({})", so_error));
			throw std::runtime_error("Connection failed (" + std::to_string(so_error) + ")");
		}

		// Connection successful
		::close(epoll_fd); // Close the epoll file descriptor
		fcntl(fd, F_SETFL, flags); // Restore the original blocking mode

		print_debug("Connected to %s:%d", info->getAddress(), info->getPort());
		this->fd = fd;
		this->addressInfo = info;
	}

	void write(const char *buffer, size_t size) {
		if(send(this->fd, buffer, size, 0) == -1) {
			print_debug("Error: Failed to send data (%d)", errno);
			// throw std::runtime_error(std::format("Failed to send data ({})", errno));
			throw std::runtime_error("Failed to send data (" + std::to_string(errno) + ")");
		}
	}

	size_t read(char *buffer, size_t size) {
		ssize_t bytes = recv(this->fd, buffer, size, 0);

		if(bytes == -1) {
			print_debug("Error: Failed to receive data (%d)", errno);
			// throw std::runtime_error(std::format("Failed to receive data ({})", errno));
			throw std::runtime_error("Failed to receive data (" + std::to_string(errno) + ")");
		}

		return bytes;
	}

	void close() {
		if(this->fd == -1) return;

		::close(this->fd);
		this->fd = -1;
	}
};
