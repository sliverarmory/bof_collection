/* Modified by Sliver Armory contributors for bounded inputs and safe failures. */

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
    const char* key = NULL;
    int hostname_size = 0;
    BeaconDataParse(&parser, Buffer, Length);
    hostname = BeaconDataExtract(&parser, &hostname_size);
    key = BeaconDataExtract(&parser, NULL);

    char remoteName[256];

    // Prepend "\\" for UNC format
    if (!format_remote_name(hostname, hostname_size, remoteName, sizeof(remoteName))) {
        BeaconPrintf(CALLBACK_ERROR, "Hostname is missing, malformed, or too long.");
        return;
    }
    if (key == NULL || key[0] == '\0') {
        BeaconPrintf(CALLBACK_ERROR, "Registry key is required.");
        return;
    }

    HKEY hRemoteBase = NULL;

    // Connect to remote registry
    LONG result = RegConnectRegistryA(
        remoteName,
        HKEY_LOCAL_MACHINE,
        &hRemoteBase
    );

    if (result != ERROR_SUCCESS) {
        BeaconPrintf(CALLBACK_ERROR, "RegConnectRegistry failed: 0x%08x. Are you a domain-joined user and is RRP running?", result);
        return;
    }

    // Sanity check: ensure that we are not able to open the key
    HKEY hkResult;
    if (RegOpenKeyA(hRemoteBase, key, &hkResult) == ERROR_SUCCESS) {
        BeaconPrintf(CALLBACK_ERROR, "Write access already available for %s. Exiting early...", key);
        RegCloseKey(hRemoteBase);
        RegCloseKey(hkResult);
        return;
    }

    // Delete key (must be relative to the opened hive)
    result = RegDeleteKeyA(
        hRemoteBase,
        key
    );

    if (result == ERROR_FILE_NOT_FOUND) {
        BeaconPrintf(CALLBACK_OUTPUT, "[+] %s does NOT exist on %s.", key, hostname);
    } else if (result == ERROR_ACCESS_DENIED){
        BeaconPrintf(CALLBACK_OUTPUT, "[+] %s exists on %s.", key, hostname);
    } else {
        BeaconPrintf(CALLBACK_ERROR, "Unknown error code: 0x%08x.", result);
    }

    // Cleanup
    RegCloseKey(hRemoteBase);
};
