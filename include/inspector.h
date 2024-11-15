/**
 * @file include/inspector.h
 * @author Jaroslav Louma <xlouma00@stud.fit.vutbr.cz>
 * @brief This file is part of the ISA24 project (adopted from the IFJ23 project).
 * @copyright Copyright (c) 2023-2024
 */

#include <stdio.h>

#include "colors.h"
#include "overload.h"

#ifndef INSPECTOR_H
#define INSPECTOR_H

#ifdef __DEBUG
	#define print_debug(format, ...) (printf(DARK_GREY "DEBUG: " __FILE__ ":%d: " format RESET "\n", __LINE__, ## __VA_ARGS__), fflush(stdout))
#else
	#define print_debug(format, ...)
#endif

#endif

/** End of file include/inspector.h **/
