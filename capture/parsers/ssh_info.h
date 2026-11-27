/* Copyright 2023 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define MAX_SSH_BUFFER (8192 + 4) // max record payload + 4-byte length field
#define MAX_LENS 200

#define SSH_KEX_UNKNOWN 0 // not negotiated yet, or gss-* and rsa* where 31 isn't the reply
#define SSH_KEX_GEX     1 // diffie-hellman-group-exchange-*, 31 is the group and 33 the reply
#define SSH_KEX_OTHER   2 // dh, ecdh, curve25519, sntrup, mlkem, 31 is the reply

typedef struct {
    char       buf[2][MAX_SSH_BUFFER];
    int32_t    len[2];
    uint16_t   packets[2];
    uint8_t    packets200[2];
    uint16_t   counts[2][2];
    uint16_t   lens[2][MAX_LENS]; // Keep up to MAX_LENS packet lengths
    uint8_t    done;
    uint8_t    newKeys;      // bit per direction that has sent SSH_MSG_NEWKEYS
    uint8_t    kex;          // negotiated kex, SSH_KEX_*
    uint8_t    doneRS;
    int8_t     listsWhich;   // side whose KEXINIT name-lists are held in lists, -1 none
    char      *lists[5];     // kex, host key, cipher, mac, compression (client to server)
} SSHInfo_t;
