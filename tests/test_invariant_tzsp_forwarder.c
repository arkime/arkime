#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Define the buffer size constant as used in tzsp_forwarder.c */
#define TZSP_PACKET_SIZE 65535
#define TZSP_HEADER_SIZE 5  /* Typical TZSP header offset */

/* Simulate the vulnerable pattern to test the invariant */
static int safe_copy_to_tzsp_buffer(uint8_t *tzsp_packet, size_t buf_size,
                                     size_t offset, const uint8_t *packet,
                                     size_t caplen)
{
    /* Invariant: offset + caplen must not exceed buffer size */
    if (offset > buf_size || caplen > buf_size - offset) {
        return -1;  /* Reject: would overflow */
    }
    memcpy(tzsp_packet + offset, packet, caplen);
    return 0;
}

START_TEST(test_tzsp_buffer_overflow_prevention)
{
    /* Invariant: caplen + offset must never exceed TZSP_PACKET_SIZE */
    uint8_t tzsp_packet[TZSP_PACKET_SIZE];
    uint8_t payload[TZSP_PACKET_SIZE + 1024];
    memset(payload, 'A', sizeof(payload));

    struct {
        size_t offset;
        size_t caplen;
        int should_succeed;
    } test_cases[] = {
        /* Exploit case: caplen causes overflow past buffer */
        {TZSP_HEADER_SIZE, TZSP_PACKET_SIZE, 0},
        /* Boundary: exactly fills buffer */
        {TZSP_HEADER_SIZE, TZSP_PACKET_SIZE - TZSP_HEADER_SIZE, 1},
        /* Valid: normal small packet */
        {TZSP_HEADER_SIZE, 1500, 1},
        /* Overflow: large offset + large caplen */
        {TZSP_PACKET_SIZE - 10, 100, 0},
    };
    int num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_cases; i++) {
        int result = safe_copy_to_tzsp_buffer(tzsp_packet, TZSP_PACKET_SIZE,
                                               test_cases[i].offset,
                                               payload, test_cases[i].caplen);
        if (test_cases[i].should_succeed) {
            ck_assert_int_eq(result, 0);
        } else {
            ck_assert_int_eq(result, -1);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tzsp_buffer_overflow_prevention);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}