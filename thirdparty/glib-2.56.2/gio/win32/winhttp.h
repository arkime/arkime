/*
 * Copyright (C) 2007 Francois Gouget
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __WINE_WINHTTP_H
#define __WINE_WINHTTP_H

#define WINHTTPAPI
#define BOOLAPI WINHTTPAPI BOOL WINAPI


typedef LPVOID HINTERNET;
typedef HINTERNET *LPHINTERNET;

#define INTERNET_DEFAULT_PORT           0
#define INTERNET_DEFAULT_HTTP_PORT      80
#define INTERNET_DEFAULT_HTTPS_PORT     443
typedef WORD INTERNET_PORT;
typedef INTERNET_PORT *LPINTERNET_PORT;

#define INTERNET_SCHEME_HTTP            1
#define INTERNET_SCHEME_HTTPS           2
typedef int INTERNET_SCHEME, *LPINTERNET_SCHEME;

/* flags for WinHttpOpen */
#define WINHTTP_FLAG_ASYNC                  0x10000000

/* flags for WinHttpOpenRequest */
#define WINHTTP_FLAG_ESCAPE_PERCENT         0x00000004
#define WINHTTP_FLAG_NULL_CODEPAGE          0x00000008
#define WINHTTP_FLAG_ESCAPE_DISABLE         0x00000040
#define WINHTTP_FLAG_ESCAPE_DISABLE_QUERY   0x00000080
#define WINHTTP_FLAG_BYPASS_PROXY_CACHE     0x00000100
#define WINHTTP_FLAG_REFRESH                WINHTTP_FLAG_BYPASS_PROXY_CACHE
#define WINHTTP_FLAG_SECURE                 0x00800000

#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY   0
#define WINHTTP_ACCESS_TYPE_NO_PROXY        1
#define WINHTTP_ACCESS_TYPE_NAMED_PROXY     3

#define WINHTTP_NO_PROXY_NAME               NULL
#define WINHTTP_NO_PROXY_BYPASS             NULL

#define WINHTTP_NO_REFERER                  NULL
#define WINHTTP_DEFAULT_ACCEPT_TYPES        NULL

#define WINHTTP_ERROR_BASE                  12000

/* The original WINE winhttp.h didn't contain symbolic names for the
 * error codes. However, the values of most of them are publicly
 * documented at
 * http://msdn.microsoft.com/en-us/library/aa383770(VS.85).aspx so
 * we can add them here.
 */
#define ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR 12178
#define ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT 12166
#define ERROR_WINHTTP_CANNOT_CALL_AFTER_OPEN 12103
#define ERROR_WINHTTP_CANNOT_CALL_AFTER_SEND 12102
#define ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN 12100
#define ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND 12101
#define ERROR_WINHTTP_CANNOT_CONNECT 12029
#define ERROR_WINHTTP_CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW 12183
#define ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED 12044
#define ERROR_WINHTTP_CONNECTION_ERROR 12030
#define ERROR_WINHTTP_HEADER_ALREADY_EXISTS 12155
#define ERROR_WINHTTP_HEADER_COUNT_EXCEEDED 12181
#define ERROR_WINHTTP_HEADER_NOT_FOUND 12150
#define ERROR_WINHTTP_HEADER_SIZE_OVERFLOW 12182
#define ERROR_WINHTTP_INCORRECT_HANDLE_STATE 12019
#define ERROR_WINHTTP_INCORRECT_HANDLE_TYPE 12018
#define ERROR_WINHTTP_INTERNAL_ERROR 12004
#define ERROR_WINHTTP_INVALID_OPTION 12009
#define ERROR_WINHTTP_INVALID_QUERY_REQUEST 12154
#define ERROR_WINHTTP_INVALID_SERVER_RESPONSE 12152
#define ERROR_WINHTTP_INVALID_URL 12005
#define ERROR_WINHTTP_LOGIN_FAILURE 12015
#define ERROR_WINHTTP_NAME_NOT_RESOLVED 12007
#define ERROR_WINHTTP_NOT_INITIALIZED 12172
#define ERROR_WINHTTP_OPERATION_CANCELLED 12017
#define ERROR_WINHTTP_OPTION_NOT_SETTABLE 12011
#define ERROR_WINHTTP_OUT_OF_HANDLES 12001
#define ERROR_WINHTTP_REDIRECT_FAILED 12156
#define ERROR_WINHTTP_RESEND_REQUEST 12032
#define ERROR_WINHTTP_RESPONSE_DRAIN_OVERFLOW 12184
#define ERROR_WINHTTP_SECURE_CERT_CN_INVALID 12038
#define ERROR_WINHTTP_SECURE_CERT_DATE_INVALID 12037
#define ERROR_WINHTTP_SECURE_CERT_REV_FAILED 12057
#define ERROR_WINHTTP_SECURE_CERT_REVOKED 12170
#define ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE 12179
#define ERROR_WINHTTP_SECURE_CHANNEL_ERROR 12157
#define ERROR_WINHTTP_SECURE_FAILURE 12175
#define ERROR_WINHTTP_SECURE_INVALID_CA 12045
#define ERROR_WINHTTP_SECURE_INVALID_CERT 12169
#define ERROR_WINHTTP_SHUTDOWN 12012
#define ERROR_WINHTTP_TIMEOUT 12002
#define ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT 12167
#define ERROR_WINHTTP_UNRECOGNIZED_SCHEME 12006
/* End of added error codes */

#define ERROR_WINHTTP_AUTODETECTION_FAILED     (WINHTTP_ERROR_BASE + 180)

typedef struct
{
    DWORD   dwStructSize;
    LPWSTR  lpszScheme;
    DWORD   dwSchemeLength;
    INTERNET_SCHEME nScheme;
    LPWSTR  lpszHostName;
    DWORD   dwHostNameLength;
    INTERNET_PORT nPort;
    LPWSTR  lpszUserName;
    DWORD   dwUserNameLength;
    LPWSTR  lpszPassword;
    DWORD   dwPasswordLength;
    LPWSTR  lpszUrlPath;
    DWORD   dwUrlPathLength;
    LPWSTR  lpszExtraInfo;
    DWORD   dwExtraInfoLength;
} URL_COMPONENTS, *LPURL_COMPONENTS;
typedef URL_COMPONENTS URL_COMPONENTSW;
typedef LPURL_COMPONENTS LPURL_COMPONENTSW;

typedef struct
{
    DWORD_PTR dwResult;
    DWORD dwError;
} WINHTTP_ASYNC_RESULT, *LPWINHTTP_ASYNC_RESULT;

typedef struct
{
    FILETIME ftExpiry;
    FILETIME ftStart;
    LPWSTR lpszSubjectInfo;
    LPWSTR lpszIssuerInfo;
    LPWSTR lpszProtocolName;
    LPWSTR lpszSignatureAlgName;
    LPWSTR lpszEncryptionAlgName;
    DWORD dwKeySize;
} WINHTTP_CERTIFICATE_INFO;

typedef struct
{
    DWORD dwAccessType;
    LPCWSTR lpszProxy;
    LPCWSTR lpszProxyBypass;
} WINHTTP_PROXY_INFO, *LPWINHTTP_PROXY_INFO;
typedef WINHTTP_PROXY_INFO WINHTTP_PROXY_INFOW;
typedef LPWINHTTP_PROXY_INFO LPWINHTTP_PROXY_INFOW;

typedef struct
{
    BOOL   fAutoDetect;
    LPWSTR lpszAutoConfigUrl;
    LPWSTR lpszProxy;
    LPWSTR lpszProxyBypass;
} WINHTTP_CURRENT_USER_IE_PROXY_CONFIG;

typedef VOID (CALLBACK *WINHTTP_STATUS_CALLBACK)(HINTERNET,DWORD_PTR,DWORD,LPVOID,DWORD);

typedef struct
{
    DWORD dwFlags;
    DWORD dwAutoDetectFlags;
    LPCWSTR lpszAutoConfigUrl;
    LPVOID lpvReserved;
    DWORD dwReserved;
    BOOL fAutoLogonIfChallenged;
} WINHTTP_AUTOPROXY_OPTIONS;

typedef struct
{
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
} HTTP_VERSION_INFO, *LPHTTP_VERSION_INFO;


#ifdef __cplusplus
extern "C" {
#endif

BOOL        WINAPI WinHttpAddRequestHeaders(HINTERNET,LPCWSTR,DWORD,DWORD);
BOOL        WINAPI WinHttpDetectAutoProxyConfigUrl(DWORD,LPWSTR*);
BOOL        WINAPI WinHttpCheckPlatform(void);
BOOL        WINAPI WinHttpCloseHandle(HINTERNET);
HINTERNET   WINAPI WinHttpConnect(HINTERNET,LPCWSTR,INTERNET_PORT,DWORD);
BOOL        WINAPI WinHttpCrackUrl(LPCWSTR,DWORD,DWORD,LPURL_COMPONENTS);
BOOL        WINAPI WinHttpCreateUrl(LPURL_COMPONENTS,DWORD,LPWSTR,LPDWORD);
BOOL        WINAPI WinHttpGetDefaultProxyConfiguration(WINHTTP_PROXY_INFO*);
BOOL        WINAPI WinHttpGetIEProxyConfigForCurrentUser(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* config);
BOOL        WINAPI WinHttpGetProxyForUrl(HINTERNET,LPCWSTR,WINHTTP_AUTOPROXY_OPTIONS*,WINHTTP_PROXY_INFO*);
HINTERNET   WINAPI WinHttpOpen(LPCWSTR,DWORD,LPCWSTR,LPCWSTR,DWORD);

/* The sixth parameter to WinHttpOpenRequest was wrong in the original
 * WINE header. It should be LPCWSTR*, not LPCWSTR, as it points to an
 * array of wide strings.
 */
HINTERNET   WINAPI WinHttpOpenRequest(HINTERNET,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR*,DWORD);
BOOL        WINAPI WinHttpQueryAuthParams(HINTERNET,DWORD,LPVOID*);
BOOL        WINAPI WinHttpQueryAuthSchemes(HINTERNET,LPDWORD,LPDWORD,LPDWORD);
BOOL        WINAPI WinHttpQueryDataAvailable(HINTERNET,LPDWORD);
BOOL        WINAPI WinHttpQueryHeaders(HINTERNET,DWORD,LPCWSTR,LPVOID,LPDWORD,LPDWORD);
BOOL        WINAPI WinHttpReadData(HINTERNET,LPVOID,DWORD,LPDWORD);
BOOL        WINAPI WinHttpReceiveResponse(HINTERNET,LPVOID);
BOOL        WINAPI WinHttpSendRequest(HINTERNET,LPCWSTR,DWORD,LPVOID,DWORD,DWORD,DWORD_PTR);
BOOL        WINAPI WinHttpSetDefaultProxyConfiguration(WINHTTP_PROXY_INFO*);
BOOL        WINAPI WinHttpSetCredentials(HINTERNET,DWORD,DWORD,LPCWSTR,LPCWSTR,LPVOID);
BOOL        WINAPI WinHttpSetOption(HINTERNET,DWORD,LPVOID,DWORD);
WINHTTP_STATUS_CALLBACK WINAPI WinHttpSetStatusCallback(HINTERNET,WINHTTP_STATUS_CALLBACK,DWORD,DWORD_PTR);
BOOL        WINAPI WinHttpSetTimeouts(HINTERNET,int,int,int,int);
BOOL        WINAPI WinHttpTimeFromSystemTime(CONST SYSTEMTIME *,LPWSTR);
BOOL        WINAPI WinHttpTimeToSystemTime(LPCWSTR,SYSTEMTIME*);
BOOL        WINAPI WinHttpWriteData(HINTERNET,LPCVOID,DWORD,LPDWORD);

/* Additional definitions, from the public domain <wininet.h> in mingw */
#define ICU_ESCAPE 0x80000000
#define ICU_DECODE 0x10000000

/* A few constants I couldn't find publicly documented, so I looked up
 * their value from the Windows SDK <winhttp.h>. Presumably this falls
 * under fair use.
 */
#define WINHTTP_QUERY_CONTENT_LENGTH 5
#define WINHTTP_QUERY_CONTENT_TYPE 1
#define WINHTTP_QUERY_LAST_MODIFIED 11
#define WINHTTP_QUERY_STATUS_CODE 19
#define WINHTTP_QUERY_STATUS_TEXT 20

#define WINHTTP_QUERY_FLAG_SYSTEMTIME 0x40000000

#ifdef __cplusplus
}
#endif

#endif  /* __WINE_WINHTTP_H */
