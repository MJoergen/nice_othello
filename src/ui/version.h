/***************************************************************
 * version.h: the version of the build.
 *
 * Copyright (c) 2026 Michael Jørgensen. MIT license (see LICENSE).
 ***************************************************************/

#ifndef VERSION_H
#define VERSION_H

// The version of the build: its release tag, such as "V1.0", or what
// git describe says between tags, such as "V1.0-3-g55970b7". The
// Makefile writes it into build/gen/version.c (VERSION).
extern const char nice_othello_version[];

#endif // VERSION_H
