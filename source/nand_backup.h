/*
 * WiiMedic - nand_backup.h
 * NAND backup detection and reminder system
 */
#ifndef NAND_BACKUP_H
#define NAND_BACKUP_H

/* Run the NAND backup check screen */
void run_nand_backup_check(void);

/* Get NAND backup status report as string */
void get_nand_backup_report(char *buf, int bufsize);

#endif /* NAND_BACKUP_H */
