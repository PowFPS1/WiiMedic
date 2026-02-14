/*
 * WiiMedic - storage_test.h
 * SD/USB storage benchmark and health check module
 */
#ifndef STORAGE_TEST_H
#define STORAGE_TEST_H

// Run the storage speed test
void run_storage_test(void);

// Get storage test report as string
void get_storage_test_report(char *buf, int bufsize);

#endif // STORAGE_TEST_H
