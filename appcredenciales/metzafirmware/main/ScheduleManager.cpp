#include "ScheduleManager.h"

void checkSchedules() {
  DateTime now = rtc.now();
  int currentMinutes = now.hour() * 60 + now.minute();
  int currentDay = now.dayOfTheWeek();

  if (!autoMode) return; // IGNORAR CALENDARIO SI NO ESTAMOS EN AUTO GLOBAL
  
  for (int i = 0; i < 4; i++) {
    
    bool anyScheduleSaysOn = false;
    bool anyScheduleEnabled = false;

    for (int j = 0; j < MAX_SCHEDS; j++) {
      if (channelSchedules[i][j].enabled) {
        anyScheduleEnabled = true;
        if (channelSchedules[i][j].daysMask & (1 << currentDay)) {
          int onM = channelSchedules[i][j].onHour * 60 + channelSchedules[i][j].onMinute;
          int offM = channelSchedules[i][j].offHour * 60 + channelSchedules[i][j].offMinute;

          if (onM < offM) {
            if (currentMinutes >= onM && currentMinutes < offM) anyScheduleSaysOn = true;
          } else {
            if (currentMinutes >= onM || currentMinutes < offM) anyScheduleSaysOn = true;
          }
        }
      }
    }

    if (anyScheduleEnabled) {
      if (anyScheduleSaysOn != lastSchedState[i]) {
        relayStatus[i] = anyScheduleSaysOn;
        digitalWrite(RELAY_PINS[i], relayStatus[i] ? HIGH : LOW);
        lastSchedState[i] = anyScheduleSaysOn;
        SerialBT.printf("CH%d=%s\n", i+1, relayStatus[i] ? "ON" : "OFF");
      }
    } else {
      lastSchedState[i] = false;
    }
  }
}
