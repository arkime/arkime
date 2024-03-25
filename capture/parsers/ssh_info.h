/* Copyright 2023 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define MAX_SSH_BUFFER 8196
#define MAX_LENS 200

typedef struct {
    char       buf[2][MAX_SSH_BUFFER];
    int32_t    len[2];
    uint16_t   packets[2];
    uint8_t    packets200[2];
    uint16_t   counts[2][2];
    uint16_t   lens[2][MAX_LENS]; // Keep up to MAX_LENS packet lengths
    uint8_t    done;
    uint8_t    doneRS;
} SSHInfo_t;
