/*
 * WiiMedic - network_test.h
 * Network connectivity test module
 */
#ifndef NETWORK_TEST_H
#define NETWORK_TEST_H

// Run the network connectivity test
void run_network_test(void);

// Get network test report as string
void get_network_test_report(char *buf, int bufsize);

#endif // NETWORK_TEST_H
