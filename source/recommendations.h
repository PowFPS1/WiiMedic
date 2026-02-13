/*
 * WiiMedic - recommendations.h
 * Auto-detection and actionable recommendations engine
 */
#ifndef RECOMMENDATIONS_H
#define RECOMMENDATIONS_H

/* Run the full system checkup and display recommendations */
void run_recommendations(void);

/* Get recommendations report as string */
void get_recommendations_report(char *buf, int bufsize);

#endif /* RECOMMENDATIONS_H */
