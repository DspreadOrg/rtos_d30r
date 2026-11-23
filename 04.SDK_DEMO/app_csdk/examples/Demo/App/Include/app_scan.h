/**********************************************************************************************************
 * File Name     : app_scan.h
 * Description   : Scan service (camera open/close/result polling)
 *
 **********************************************************************************************************/

#ifndef __APP_SCAN_H__
#define __APP_SCAN_H__

/* Open the camera and start preview */
int app_scan_start(void);

/* Close the camera and stop preview; safe to call repeatedly */
int app_scan_stop(void);

/* Non-blocking scan result fetch:
 *   1  barcode parsed, buf holds the barcode content, *plen is the length
 *   0  no result yet
 *   <0 error */
int app_scan_poll(unsigned char *buf, unsigned short *plen);

#endif /* __APP_SCAN_H__ */
