#ifndef DASHBOARD_H
#define DASHBOARD_H

// Boot the dashboard UI (single-page overview).
// Call once from app_main() after LVGL is initialized and after the
// underlying NetSync (WiFi + UDP) task has been started.
void Dashboard_Start(void);

#endif
