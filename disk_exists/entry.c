/* Modified by Sliver Armory contributors for bounded input formatting. */

#include <windows.h>
#include "../beacon.h"

static BOOL format_remote_name(
    const char *hostname,
    int hostname_size,
    char *remote_name,
    SIZE_T remote_name_size
)
{
    SIZE_T hostname_length = 0;

    if (hostname == NULL || hostname_size <= 0 || remote_name_size < 3) {
        return FALSE;
    }
    while (hostname_length < (SIZE_T)hostname_size && hostname[hostname_length] != '\0') {
        hostname_length++;
    }
    if (hostname_length == 0 || hostname_length >= (SIZE_T)hostname_size ||
        hostname_length > remote_name_size - 3) {
        return FALSE;
    }

    remote_name[0] = '\\';
    remote_name[1] = '\\';
    for (SIZE_T index = 0; index < hostname_length; index++) {
        remote_name[index + 2] = hostname[index];
    }
    remote_name[hostname_length + 2] = '\0';
    return TRUE;
}

VOID go(
    IN PCHAR Buffer,
    IN ULONG Length
)
{
    datap parser = {0};
    const char* hostname = NULL;
    const char* path = NULL;
    int hostname_size = 0;
    BeaconDataParse(&parser, Buffer, Length);
    hostname = BeaconDataExtract(&parser, &hostname_size);
    path = BeaconDataExtract(&parser, NULL);

    char remoteName[256];
    // Prepend "\\" for UNC format
    if (!format_remote_name(hostname, hostname_size, remoteName, sizeof(remoteName))) {
        BeaconPrintf(CALLBACK_ERROR, "Hostname is missing, malformed, or too long.");
        return;
    }
    if (path == NULL || path[0] == '\0') {
        BeaconPrintf(CALLBACK_ERROR, "Path is required.");
        return;
    }

    HKEY hRemoteBase = NULL;

    // Connect to remote registry
    LONG result = RegConnectRegistryA(
        remoteName,
        HKEY_USERS,
        &hRemoteBase
    );

    if (result != ERROR_SUCCESS) {
        BeaconPrintf(CALLBACK_ERROR, "RegConnectRegistry failed: 0x%08x. Are you a domain-joined user?", result);
        return;
    }

    HKEY hkResult;
    if (RegOpenKeyA(hRemoteBase, "", &hkResult) != ERROR_SUCCESS) {
        BeaconPrintf(CALLBACK_ERROR, "Could not open HKU");
        RegCloseKey(hRemoteBase);
        return;
    }

    // Save key
    result = RegSaveKeyA(
        hkResult,
        path,
        NULL
    );

    if (result == ERROR_ACCESS_DENIED) {
        BeaconPrintf(CALLBACK_OUTPUT, "[+] %s does NOT exist on %s.", path, hostname);
    } else if (result == ERROR_ALREADY_EXISTS) {
        BeaconPrintf(CALLBACK_OUTPUT, "[+] %s exists on %s.", path, hostname);
    } else if (result == ERROR_SUCCESS) {
        BeaconPrintf(CALLBACK_OUTPUT, "[+] %s did not exist and has been written to %s. Make sure to delete to leave no artifacts!", path, hostname);
    } else {
        BeaconPrintf(CALLBACK_ERROR, "Unknown error code: 0x%08x.", result);
    }

    // Cleanup
    RegCloseKey(hRemoteBase);
    RegCloseKey(hkResult);
};
