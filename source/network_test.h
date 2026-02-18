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

// Check if the network test has already been run in this session
bool has_network_test_run(void);

#endif // NETWORK_TEST_H
