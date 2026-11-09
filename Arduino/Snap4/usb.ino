// USB serial commands, checked while the menu is showing. Used by tools/logger-setup.html.
//   R                 report the clock
//   T<seconds>        set the clock (seconds since 1970 UTC), then report it
//   G                 report device, clock, settings and status
//   S <name>=<value>  change one setting and save it; replies OK <name> or ERR <name>
//   C                 clear the restart and event counts, the stop flags and the clock-switch record
// Replies, one line each:
//   DEVICE <model> <firmware version>
//   TIME <seconds since 1970> <clock source> <firmware version>
//   SETTINGS <name>=<value> ...
//   STATUS <name>=<value> ...

char serialCmd[40];
int serialCmdLen = 0;

void checkSerialCommands(){
  while(Serial.available()){
    char c = Serial.read();
    if(c != '\n' && c != '\r'){
      if(serialCmdLen < (int) sizeof(serialCmd) - 1) serialCmd[serialCmdLen++] = c;
      continue;
    }
    serialCmd[serialCmdLen] = 0;
    serialCmdLen = 0;

    switch(serialCmd[0]){
      case 'T':
      {
        time_t newTime = strtoul(&serialCmd[1], NULL, 10);
        if(newTime < TIME_2026) break;  // not a valid time
        setTime2(hour(newTime), minute(newTime), second(newTime), day(newTime), month(newTime), year(newTime) - 2000U);
        dsOscStopped = false;  // time is set again
        autoStartTime = getTeensy3Time(0);  // so the jump in time does not trigger autostart
        reportTime();
        break;
      }
      case 'R':
        reportTime();
        break;
      case 'G':
        Serial.print("DEVICE Snap4 ");
        Serial.println(codeVersion);
        reportTime();
        reportSettings();
        reportStatus();
        break;
      case 'S':
        setSetting(&serialCmd[1]);
        break;
      case 'C':
        EEPROM.write(EE_SD_RESTARTS, 0);
        EEPROM.write(EE_LOW_BATT_STOP, 0);
        EEPROM.write(EE_CARD_FULL_STOP, 0);
        EEPROM.write(EE_WDOG_RESETS, 0);
        EEPROM.write(EE_CLOCK_SWITCH, 0);
        EEPROM.write(EE_AUDIO_RESTARTS, 0);
        EEPROM.write(EE_CLOCK_GAPS, 0);
        Serial.println("OK clear");
        break;
    }
  }
}

void reportTime(){
  t = getTeensy3Time(0);
  Serial.print("TIME ");
  Serial.print((uint32_t) t);
  if(clockSource == CLOCK_DS3231) Serial.print(" DS3231 ");
  else if(clockSource == CLOCK_INTERNAL) Serial.print(" INTERNAL ");
  else Serial.print(" LPTMR ");
  Serial.println(codeVersion);
}

void reportSettings(){
  // Snap: no chan= (always mono), so the setup page does not offer Channels
  Serial.printf("SETTINGS rec_dur=%lu rec_int=%lu rate=%d gain=%d batpacks=%d delay=%d mode=%d start_h=%d start_m=%d end_h=%d end_m=%d cutoff=%d sd_sleep=%d\n",
    rec_dur, rec_int, isf, gainSetting, (int) nBatPacks, (int) delayDays, recMode,
    (int) startHour, (int) startMinute, (int) endHour, (int) endMinute, (int) voltCutoff, (int) sdSleep);
}

void reportStatus(){
  calcDeployment();
  pinMode(vSense, INPUT);
  Serial.printf("STATUS volt=%.2f free_mb=%lu total_mb=%lu file_mb=%.1f files=%lu power_days=%.1f mem_days=%.1f gain_db=%.1f internal_rtc=%d osc_stopped=%d sd_restarts=%d low_batt_stop=%d card_full_stop=%d",
    readVoltage(), freeMB[0], cardTotalMB, fileMB, filesPerCard[0],
    powerSeconds / 86400.0, totalSecondsMemory / 86400.0, gainDb,
    (int) internalRtcOK, (int) dsOscStopped,
    (int) EEPROM.read(EE_SD_RESTARTS), (int) EEPROM.read(EE_LOW_BATT_STOP), (int) EEPROM.read(EE_CARD_FULL_STOP));
  boolean clockSwitch = (EEPROM.read(EE_CLOCK_SWITCH) == 1);
  Serial.printf(" wdog_resets=%d audio_restarts=%d clock_gaps=%d clk_switch=%d clk_switch_time=%lu\n",
    (int) EEPROM.read(EE_WDOG_RESETS), (int) EEPROM.read(EE_AUDIO_RESTARTS), (int) EEPROM.read(EE_CLOCK_GAPS),
    (int) clockSwitch, clockSwitch ? (unsigned long) readEEPROMlong(EE_CLOCK_SWITCH_TIME) : 0UL);
}

// Ranges match the menu
void setSetting(char *arg){
  while(*arg == ' ') arg++;
  char *eq = strchr(arg, '=');
  if(eq == NULL){
    Serial.println("ERR");
    return;
  }
  *eq = 0;
  long v = atol(eq + 1);
  boolean ok = true;

  if(!strcmp(arg, "rec_dur") & (v >= 1) & (v <= 3600)) rec_dur = v;
  else if(!strcmp(arg, "rec_int") & (v >= 0) & (v <= 86400)) rec_int = v;
  else if(!strcmp(arg, "rate") & (v >= 0) & (v <= 8)) isf = v;
  else if(!strcmp(arg, "gain") & (v >= 0) & (v <= 15)){
    gainSetting = v;
    calcGain();
  }
  else if(!strcmp(arg, "batpacks") & (v >= 1) & (v <= 12)) nBatPacks = v;  // Snap: up to 12
  else if(!strcmp(arg, "delay") & (v >= 0) & (v <= 365)) delayDays = v;
  else if(!strcmp(arg, "mode") & (v >= 0) & (v <= 1)) recMode = v;
  else if(!strcmp(arg, "start_h") & (v >= 0) & (v <= 23)) startHour = v;
  else if(!strcmp(arg, "start_m") & (v >= 0) & (v <= 59)) startMinute = v;
  else if(!strcmp(arg, "end_h") & (v >= 0) & (v <= 23)) endHour = v;
  else if(!strcmp(arg, "end_m") & (v >= 0) & (v <= 59)) endMinute = v;
  else if(!strcmp(arg, "cutoff") & ((v == 0) | ((v >= 30) & (v <= 34)))) voltCutoff = v;
  else if(!strcmp(arg, "sd_sleep") & (v >= 0) & (v <= 1)) sdSleep = v;
  else ok = false;

  if(ok){
    writeEEPROM();
    autoStartTime = getTeensy3Time(0);  // counts as activity for the menu autostart
    Serial.print("OK ");
  }
  else Serial.print("ERR ");
  Serial.println(arg);
}
