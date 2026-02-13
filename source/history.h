/*
 * WiiMedic - history.h
 * Historical diagnostic tracking - save/compare snapshots over time
 */
#ifndef HISTORY_H
#define HISTORY_H

/* Run history viewer / comparison screen */
void run_history(void);

/* Save a snapshot of current diagnostic data (called from report gen) */
void history_save_snapshot(void);

#endif /* HISTORY_H */
