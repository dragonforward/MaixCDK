// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.1
// LVGL version: 8.3.11
// Project name: SquareLine_Project

#ifndef _UI_EVENTS_H
#define _UI_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

void exitfuction(lv_event_t * e);
void recordclicked(lv_event_t * e);
void imgbtnrecordfunction(lv_event_t * e);
void stoprecordfunction(lv_event_t * e);
void pausefunction(lv_event_t * e);
void imgbtnstoprecordfunction(lv_event_t * e);
void imgbtnpausefunction(lv_event_t * e);
void playaudiocallback(lv_event_t * e);
void playaudioexit(lv_event_t * e);
void playvolumeclick(lv_event_t * e);
void playclickedscreen3(lv_event_t * e);
void playscreen3exit(lv_event_t * e);
void playvolumeslider(lv_event_t * e);
void showvolumeslider(lv_event_t * e);
void app_exit_imgbtn(lv_event_t * e);
void recordvolumeclick(lv_event_t * e);
void recordvolumeslider(lv_event_t * e);
void record_showvolumeslider(lv_event_t * e);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif