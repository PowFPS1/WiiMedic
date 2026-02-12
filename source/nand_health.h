/*
 * WiiMedic - nand_health.h
 * NAND filesystem health check module
 */
#ifndef NAND_HEALTH_H
#define NAND_HEALTH_H

// Run the NAND health check display
void run_nand_health(void);

// Get NAND health report as string
void get_nand_health_report(char *buf, int bufsize);

#endif // NAND_HEALTH_H
