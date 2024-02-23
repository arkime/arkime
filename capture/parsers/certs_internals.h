/* Copyright 2023 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/******************************************************************************/
/*
 * Certs Info
 */

typedef struct {
    ArkimeStringHead_t  commonName; // 2.5.4.3
    ArkimeStringHead_t  orgName;    // 2.5.4.10
    ArkimeStringHead_t  orgUnit;    // 2.5.4.11
    char                orgUtf8;
} ArkimeCertInfo_t;

typedef struct arkime_certsinfo {
    uint64_t                 notBefore;
    uint64_t                 notAfter;
    ArkimeCertInfo_t         issuer;
    ArkimeCertInfo_t         subject;
    ArkimeStringHead_t       alt;
    uint8_t                 *serialNumber;
    short                    serialNumberLen;
    uint8_t                  hash[60];
    char                     isCA;
    const char              *publicAlgorithm;
    const char              *curve;
    GHashTable              *extra;
} ArkimeCertsInfo_t;

/******************************************************************************/
void certinfo_save(BSB *jbsb, ArkimeFieldObject_t *object, ArkimeSession_t *session);
void certinfo_free(ArkimeFieldObject_t *object);
uint32_t certinfo_hash(const void *key);
int certinfo_cmp(const void *keyv, const void *elementv);
