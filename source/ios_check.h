/*
 * WiiMedic - ios_check.h
 * IOS installation audit module
 */
#ifndef IOS_CHECK_H
#define IOS_CHECK_H

// Run the IOS installation scan
void run_ios_check(void);

// Get IOS check report as string
void get_ios_check_report(char *buf, int bufsize);

#endif // IOS_CHECK_H
