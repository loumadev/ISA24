all: imapcl

# DEBUG = -D__DEBUG
ENTRY = src/main.cpp
BUILD = .
TARGET = imapcl
INCLUDE = -Iinclude/ -Isrc/
CXX = g++
CFLAGS = -std=c++20 -Wall -Wextra -pedantic -Werror -g
LIBS = -lssl -lcrypto

imapcl:
	$(CXX) $(ENTRY) -o $(BUILD)/$(TARGET) $(INCLUDE) $(DEBUG) $(CFLAGS) $(LIBS)

pack:
	tar -cvf $(BUILD)/xlouma00.tar src/ include/ Makefile README.md docs/

clean:
	rm -f $(BUILD)

.PHONY: all clean server client