/* Modified by Sliver Armory contributors for checked output allocation. */

#pragma once
#include "../beacon.h"
#include <windows.h>

#ifndef bufsize
#define bufsize 8192
#endif

char *output __attribute__((section(".data"))) = 0;
WORD currentoutsize __attribute__((section(".data"))) = 0;
BOOL outputfailed __attribute__((section(".data"))) = FALSE;

#define intAlloc(size) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size))
#define intFree(addr) HeapFree(GetProcessHeap(), 0, (addr))

BOOL printoutput(BOOL done)
{
    if (output == NULL)
        return FALSE;

    if (currentoutsize > 0)
        BeaconOutput(CALLBACK_OUTPUT, output, currentoutsize);
    currentoutsize = 0;
    if (done) {
        intFree(output);
        output = NULL;
    } else {
        memset(output, 0, bufsize);
    }
    return TRUE;
}

BOOL bofstart(void)
{
    output = (char *)intAlloc(bufsize);
    currentoutsize = 0;
    outputfailed = output == NULL;
    return !outputfailed;
}

void internal_printf(const char *format, ...)
{
    int buffersize = 0;
    int transfersize = 0;
    char *curloc = NULL;
    char *intBuffer = NULL;
    va_list args;

    if (outputfailed || output == NULL)
        return;

    va_start(args, format);
    buffersize = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (buffersize < 0)
        return;

    /* +1: vsnprintf needs room for the null; using only buffersize truncates the last char (\n) to \0 */
    intBuffer = (char *)intAlloc((SIZE_T)buffersize + 1U);
    if (output == NULL || intBuffer == NULL) {
        outputfailed = TRUE;
        if (intBuffer != NULL)
            intFree(intBuffer);
        return;
    }
    va_start(args, format);
    if (vsnprintf(intBuffer, (SIZE_T)buffersize + 1U, format, args) < 0) {
        va_end(args);
        intFree(intBuffer);
        outputfailed = TRUE;
        return;
    }
    va_end(args);

    if (buffersize + currentoutsize < bufsize) {
        memcpy(output + currentoutsize, intBuffer, buffersize);
        currentoutsize += buffersize;
    } else {
        curloc = intBuffer;
        while (buffersize > 0) {
            transfersize = bufsize - currentoutsize;
            if (buffersize < transfersize)
                transfersize = buffersize;
            memcpy(output + currentoutsize, curloc, transfersize);
            currentoutsize += transfersize;
            if (currentoutsize == bufsize && !printoutput(FALSE)) {
                outputfailed = TRUE;
                break;
            }
            curloc += transfersize;
            buffersize -= transfersize;
        }
    }
    intFree(intBuffer);
}
