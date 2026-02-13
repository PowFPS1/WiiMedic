/*
 * WiiMedic - system_info.h
 * System information display module
 */
#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

// Run the system information display
void run_system_info(void);

// Get system info as formatted string for reports
// buf must be at least 2048 bytes
void get_system_info_report(char *buf, int bufsize);

#endif // SYSTEM_INFO_H
