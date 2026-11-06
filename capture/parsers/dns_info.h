/* dns_info.h  -- DNS structures shared by parsers/dns.c and plugins
 *
 * Copyright 2023 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once


typedef enum dns_type {
    DNS_RR_A          =   1,
    DNS_RR_NS         =   2,
    DNS_RR_CNAME      =   5,
    DNS_RR_PTR        =  12,
    DNS_RR_MX         =  15,
    DNS_RR_TXT        =  16,
    DNS_RR_AAAA       =  28,
    DNS_RR_SRV        =  33,
    DNS_RR_DS         =  43,
    DNS_RR_RRSIG      =  46,
    DNS_RR_NSEC       =  47,
    DNS_RR_HTTPS      =  65,
    DNS_RR_CAA        = 257
} DNSType_t;

typedef enum dns_class {
    CLASS_IN      =     1,
    CLASS_CS      =     2,
    CLASS_CH      =     3,
    CLASS_HS      =     4,
    CLASS_NONE    =   254,
    CLASS_ANY     =   255,
    CLASS_UNKNOWN = 65280
} DNSClass_t;

typedef enum dns_svcb_param_key {
    SVCB_PARAM_KEY_ALPN      = 1,
    SVCB_PARAM_KEY_PORT      = 3,
    SVCB_PARAM_KEY_IPV4_HINT = 4,
    SVCB_PARAM_KEY_IPV6_HINT = 6
} DNSSVCBParamKey_t;

typedef enum dns_result_record_type {
    RESULT_RECORD_ANSWER          =     1,    /* Answer or Prerequisites Record */
    RESULT_RECORD_AUTHORITATIVE   =     2,    /* Authoritative or Update Record */
    RESULT_RECORD_ADDITIONAL      =     3,    /* Additional Record */
    RESULT_RECORD_UNKNOWN         =     4,    /* Unknown Record*/
} DNSResultRecordType_t;

typedef struct dns_answer_mxrdata {
    uint16_t preference;
    char    *exchange;
} DNSMXRData_t;

typedef struct dns_answer_srvrdata {
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    char    *target;
} DNSSRVRData_t;

typedef struct dns_answer_svcbrdata_field_value {
    struct dns_answer_svcbrdata_field_value  *t_next, *t_prev;
    DNSSVCBParamKey_t                         key;
    void                                     *value;
} DNSSVCBRDataFieldValue_t;

typedef struct {
    struct dns_answer_svcbrdata_field_value  *t_next, *t_prev;
    int                                       t_count;
} DNSSVCBRDataFieldValueHead_t;

typedef struct dns_answer_svcbrdata {
    uint16_t                       priority;
    char                          *dname;
    DNSSVCBRDataFieldValueHead_t   fieldValues;
} DNSSVCBRData_t;

typedef struct dns_answer_caadata {
    char   *tag;
    char   *value;
    uint8_t flags;
} DNSCAARData_t;

typedef struct dns_answer_rrsigdata {
    char    *signerName;
    uint32_t originalTTL;
    uint32_t expiration;
    uint32_t inception;
    uint16_t typeCovered;
    uint16_t keyTag;
    uint8_t  algorithm;
    uint8_t  labels;
} DNSRRSIGRData_t;

typedef struct dns_answer_nsecdata {
    char *nextDomainName;
    char *typeList;
} DNSNSECRData_t;

typedef struct dns_answer_dsdata {
    char    *digest;
    uint16_t keyTag;
    uint8_t  algorithm;
    uint8_t  digestType;
} DNSDSRData_t;

typedef struct dns_answer {
    struct dns_answer  *t_next, *t_prev;
    union {
        char            *cname;
        char            *ptrdname;
        DNSMXRData_t    *mx;
        DNSSRVRData_t   *srv;
        char            *nsdname;
        uint32_t         ipA;
        struct in6_addr *ipAAAA;
        GPtrArray       *txts;
        DNSCAARData_t   *caa;
        DNSSVCBRData_t  *svcb;
        DNSRRSIGRData_t *rrsig;
        DNSNSECRData_t  *nsec;
        DNSDSRData_t    *ds;
    };
    char                *class;
    char                *type;
    char                *name;
    uint32_t             ttl;
    uint16_t             type_id; // Only used for choosing correct RDATA in union
    uint8_t              section; // DNSResultRecordType_t
} DNSAnswer_t;

typedef struct {
    struct dns_answer *t_next, *t_prev;
    int                t_count;
} DNSAnswerHead_t;

typedef struct {
    char    *class;
    char    *type;
    char    *opcode;
    char    *hostname;
    uint16_t type_id;
    uint16_t class_id;
    uint16_t packet_uid;
    uint8_t  opcode_id;
} DNSQuery_t;

typedef struct dns {
    struct dns            *t_next, *t_prev;
    uint32_t               t_hash;
    short                  t_bucket;
    DNSAnswerHead_t        answers;
    DNSQuery_t             query;
    ArkimeStringHashStd_t  hosts;
    ArkimeStringHashStd_t *nsHosts;
    ArkimeStringHashStd_t *mxHosts;
    ArkimeStringHashStd_t *punyHosts;
    GHashTable            *ips;
    GHashTable            *nsIPs;
    GHashTable            *mxIPs;
    char                  *rcode;
    struct timeval         queryTime;
    struct timeval         responseTime;
    int8_t                 rcode_id;
    uint8_t                headerFlags;
    uint8_t                queryFlags;
    uint8_t                responseFlags;
} DNS_t;

typedef struct {
    struct dns *t_next, *t_prev;
    int         t_count;
} DNSHead_t;
