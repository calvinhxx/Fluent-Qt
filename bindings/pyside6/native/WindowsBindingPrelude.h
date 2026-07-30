#ifndef FLUENTQT_PYSIDE6_WINDOWSBINDINGPRELUDE_H
#define FLUENTQT_PYSIDE6_WINDOWSBINDINGPRELUDE_H

// Python's Windows headers define the legacy RPC token `small` as a macro.
// Load those headers once, then remove the macro before FluentQt headers.
#include <sbkpython.h>
#include <Rpc.h>

#ifdef small
#undef small
#endif

#endif // FLUENTQT_PYSIDE6_WINDOWSBINDINGPRELUDE_H
