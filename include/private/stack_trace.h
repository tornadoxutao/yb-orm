#ifndef YB__UTIL__STACKTRACE__INCLUDED
#define YB__UTIL__STACKTRACE__INCLUDED

// Based on:
// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0

#include <iostream>
#if defined(__WIN32__) || defined(_WIN32) || defined(__CYGWIN__)
// TODO: implement
#else
#include <execinfo.h>
#include <cxxabi.h>
#include <stdlib.h>
#endif

/** Print a demangled stack backtrace of the caller function to stream out. */
static inline void print_stacktrace(std::ostream &out,
	int max_frames = 100, int ignore_frames = 1)
{
#if defined(__WIN32__) || defined(_WIN32) || defined(__CYGWIN__)
    out << "<stack trace not implemented>\n";
#else
    // storage array for stack trace address data
    void *addrlist[max_frames + ignore_frames];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));

    if (addrlen == 0) {
        out << "<empty, possibly corrupt>\n";
        return;
    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char **symbollist = backtrace_symbols(addrlist, addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char *funcname = (char *)malloc(funcnamesize);

    // iterate over the returned symbol lines. may skip the first ones,
    // e.g. the first may be the address of this function.
    for (int i = ignore_frames; i < addrlen; i++)
    {
        char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

        // find parentheses and +address offset surrounding the mangled name:
        // ./module(function+0x15c) [0x8048a6d]
        for (char *p = symbollist[i]; *p; ++p)
        {
            if (*p == '(')
                begin_name = p;
            else if (*p == '+')
                begin_offset = p;
            else if (*p == ')' && begin_offset) {
                end_offset = p;
                break;
            }
        }

        if (begin_name && begin_offset && end_offset
            && begin_name < begin_offset)
        {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // mangled name is now in [begin_name, begin_offset) and caller
            // offset in [begin_offset, end_offset). now apply
            // __cxa_demangle():

            int status;
            char* ret = abi::__cxa_demangle(begin_name,
                                            funcname, &funcnamesize, &status);
            if (status == 0) {
                funcname = ret; // use possibly realloc()-ed string
                out << "  " << symbollist[i] << " : " << funcname
		    << "+" << begin_offset << "\n";
            }
            else {
                // demangling failed. Output function name as a C function with
                // no arguments.
		out << "  " << symbollist[i] << " : " << begin_name
		    << begin_name << "()+" << begin_offset << "\n";
            }
        }
        else
        {
            // couldn't parse the line? print the whole line.
            out << "  " << symbollist[i] << "\n";
        }
    }

    free(funcname);
    free(symbollist);
#endif
}

#endif // YB__UTIL__STACKTRACE__INCLUDED
