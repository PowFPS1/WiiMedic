/*
 * WiiMedic - controller_test.h
 * Controller diagnostics module
 */
#ifndef CONTROLLER_TEST_H
#define CONTROLLER_TEST_H

// Run the controller diagnostic test
void run_controller_test(void);

// Get controller test report as string
void get_controller_test_report(char *buf, int bufsize);

#endif // CONTROLLER_TEST_H
