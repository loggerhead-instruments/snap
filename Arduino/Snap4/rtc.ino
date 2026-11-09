int RTCAddress = 0x68;

volatile byte ds_second;
volatile byte ds_minute;
volatile byte ds_hour;
volatile byte ds_day;
volatile byte ds_month;
volatile byte ds_year;

#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_DAY 86400
#define SECONDS_IN_YEAR 31536000
#define SECONDS_IN_LEAP 31622400

void setTime2(int thour,int tminute,int tsecond,int tday,int tmonth,int tyear)
{
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x00);  //starting address is 00
  Wire.write(((tsecond/10)<<4) | ((tsecond%10)&0x0F));  //seconds, 
  Wire.write(((tminute/10)<<4) | ((tminute%10)&0x0F));  //min
  Wire.write((((thour/10)&0x03)<<4) | ((thour)%10&0x0F)); //hour  0x20 will set to 12 hour AM/PM mode
  Wire.write(0x01); // day of week(1-7)
  Wire.write(((tday/10)<<4) | ((tday%10)&0x0F)); // date (1-31)
  Wire.write((tmonth/10)<<4 | ((tmonth%10)&0x0F)); //  month
  Wire.write((tyear/10)<<4 | ((tyear%10)&0x0F)); //  year 2010=0x0A
  int ecode=Wire.endTransmission(); //end transmission
//  Serial.println(ecode);

  // keep the backup clocks on the time that was just set
  tmElements_t tm;
  tm.Second = tsecond;
  tm.Minute = tminute;
  tm.Hour = thour;
  tm.Day = tday;
  tm.Month = tmonth;
  tm.Year = tyear + 30U;
  time_t newTime = makeTime(tm);
  if(internalRtcOK) Teensy3Clock.set(newTime);
  setSoftTime(newTime);
  syncDs = syncRtc = newTime;  // both clocks start again from the time just set
  lastDsTime = newTime;        // and the stuck check
  lastDsChange = millis();
  lastDsChangeRtc = internalRtcOK ? newTime : Teensy3Clock.get();
}

void set_alarm(int thour, int tminute)
{
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x07); //0x0B Alarm1
  Wire.write(0x00); //seconds
  Wire.write(((tminute/10)<<4) | ((tminute%10)&0x0F)); // Minutes
  Wire.write((((thour/10)&0x03)<<4) | ((thour)%10&0x0F)); //hour
  Wire.write(0x80); //alarm when hour minutes and seconds match
  Wire.endTransmission();

  Wire.beginTransmission(RTCAddress);
  Wire.write(0x0E); //Control register
  Wire.write(0x01); //enable Alarm 1 interrupt
  Wire.write(0x00); //clear existing interrupts
  Wire.endTransmission();
}

void reset_alarm()
{
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x0F); //Control register
  Wire.write(0x00); //clear existing interrupts
  Wire.endTransmission();
}

void disable_alarm()
{
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x0E); // Control register
  Wire.write(0x04); // INTCN=1, alarm interrupts off
  Wire.write(0x00); // clear status flags
  Wire.endTransmission();
}
boolean readRTC()
{
  // Serial.println("Read DS3231: ");
  int i = 0;
  byte buff[7];
  // Same transaction as the firmware that has always read the DS3231 (4.40 and earlier)
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x00);        //sends address to read from starting with seconds
  Wire.endTransmission(0); //end transmission

  Wire.requestFrom(RTCAddress, 7, 0);    // request 7 bytes from clock (seconds through Year)

  while(Wire.available() && i < 7)
  {
    buff[i] = Wire.read();  // receive one byte
    i++;
  }
  Wire.endTransmission();

  if (i==7)  // All bytes received?
  {
      ds_second=(10*(buff[0]>>4)) + ((buff[0]&0x0F)) ;//lower 4 bits is seconds, upper 3 bits are 10 seconds
      ds_minute=(10*(buff[1]>>4)) + ((buff[1]&0x0F)) ;//lower 4 bits is minutes, upper 3 bits are 10 minutes
      ds_hour=(10*((buff[2]>>4)&0x03))+((buff[2]&0x0F));  //bits 4 and 5 are 10 hours; bit 0-3 are hour
      ds_day=(10*(buff[4]>>4))+((buff[4]&0x0F)); 
      ds_month=(10*(buff[5]>>4))+((buff[5]&0x0F)); 
      ds_year=(10*(buff[6]>>4))+((buff[6]&0x0F));
      // reject values a failing chip or an idle bus (all 0xFF) can return
      if(ds_second>59 || ds_minute>59 || ds_hour>23 || ds_day<1 || ds_day>31 || ds_month<1 || ds_month>12) return 0;
     // t =RTCToUNIXTime(ds_year, ds_month, ds_day, ds_hour, ds_minute, ds_second);  
      tmElements_t tm;
      tm.Second = ds_second;
      tm.Minute = ds_minute;
      tm.Hour = ds_hour;
      tm.Day = ds_day;
      tm.Month = ds_month;
      tm.Year = ds_year + 30U;
      t = makeTime(tm);
      // Serial.println(t);
      return 1;
  }
  else return 0;
}

// Status register 0x0F, read as the firmware that has always read the DS3231 (4.40 and earlier)
int rtcStatus(){
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x0F);
  Wire.endTransmission();
  if(Wire.requestFrom(RTCAddress, 1) != 1) return 0xFF;  // no reply: treat as stopped
  int val = Wire.read();
  Wire.endTransmission();
  return val;
}

// Read DS3231 registers from 0x00. Returns the number of bytes read.
int readDSRegs(byte *reg, int count){
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x00);
  if(Wire.endTransmission(0) != 0) return 0;  // no stop: repeated start for the read
  int n = Wire.requestFrom(RTCAddress, count, 1);
  for(int i = 0; i < n; i++) reg[i] = Wire.read();
  return n;
}

// True if the DS3231 answers with a valid time and is counting
boolean ds3231Running(){
  if(!readRTC()) return false;
  time_t t1 = t;
  uint32_t startMillis = millis();
  while(millis() - startMillis < 2500){
    delay(50);
    if(!readRTC()) return false;
    if(t != t1) return true;
  }
  return false;
}

// Internal 32.768 kHz oscillator on, with a 26 pF load for the 12.5 pF crystal. Called first in
// setup(), so the crystal settles while the rest starts. The Teensy core starts the oscillator
// before setup() with its own load; the load is changed without stopping it. A CPU restart
// leaves it running, so the time is kept across resetFunc().
void rtcOscStart(){
  uint32_t rtcCR = RTC_CR_SC16P | RTC_CR_SC8P | RTC_CR_SC2P | RTC_CR_OSCE;
  if(RTC_CR != rtcCR) RTC_CR = rtcCR;
}

// Check that the internal RTC is counting
boolean internalRTCRunning(){
  rtcOscStart();  // in case it was stopped since
  if(!(RTC_SR & RTC_SR_TCE) || (RTC_SR & RTC_SR_TIF)) Teensy3Clock.set(DEFAULT_TIME); // start the counter
  uint32_t start = RTC_TSR;
  uint32_t startMillis = millis();
  while(millis() - startMillis < 3000){  // allow for crystal start-up
    if(RTC_TSR != start) return true;
    delay(10);
  }
  return false;
}

// Measure the 1 kHz LPO that clocks the LPTMR against the main crystal,
// so LPTMR sleeps can be counted in real seconds.
void calibrateLPO(){
  SIM_SCGC5 |= SIM_SCGC5_LPTIMER;
  LPTMR0_CSR = 0;
  LPTMR0_PSR = LPTMR_PSR_PBYP | LPTMR_PSR_PCS(1); // LPO, no prescaler
  LPTMR0_CMR = 499;                               // flag every 500 ticks
  LPTMR0_CSR = LPTMR_CSR_TEN;
  uint32_t flagTime[2];
  for(int n = 0; n < 2; n++){
    uint32_t start = millis();
    while(!(LPTMR0_CSR & LPTMR_CSR_TCF)){
      if(millis() - start > 1000){  // LPO not running: keep the last value
        LPTMR0_CSR = 0;
        return;
      }
    }
    flagTime[n] = micros();
    LPTMR0_CSR = LPTMR_CSR_TEN | LPTMR_CSR_TCF; // clear the flag
  }
  LPTMR0_CSR = 0;
  float hz = 500.0 * 1000000.0 / (float) (flagTime[1] - flagTime[0]);
  if(hz > 500.0 && hz < 2000.0) lpoHz = hz;
  Serial.print("LPO Hz ");
  Serial.println(lpoHz);
}

void setSoftTime(time_t newTime){
  softTime = newTime;
  softMillis = millis();
}

time_t softNow(){
  uint32_t elapsed = millis() - softMillis;
  softTime += elapsed / 1000;
  softMillis += (elapsed / 1000) * 1000;  // keep the fraction of a second
  return softTime;
}

// Switch away from the DS3231 for the rest of the deployment. A switch during a deployment
// is recorded with its time for the setup page (the first one, until cleared).
void useBackupClock(){
  rtcFail = true;
  clockSource = internalRtcOK ? CLOCK_INTERNAL : CLOCK_LPTMR;
  disable_alarm();  // in case the DS3231 still answers, so it cannot hold INT low
  Serial.print("DS3231 failed, clock source ");
  Serial.println(clockSource);
  if((EEPROM.read(EE_DEPLOYED) == 1) & (EEPROM.read(EE_CLOCK_SWITCH) != 1)){
    EEPROM.write(EE_CLOCK_SWITCH, 1);
    writeEEPROMlong(EE_CLOCK_SWITCH_TIME, internalRtcOK ? Teensy3Clock.get() : softNow());
  }
}

// Called at each sync while the DS3231 is the clock. Both clocks must have moved on since the
// last sync. If the DS3231 has and the internal RTC has not, the internal RTC has stopped: stop
// using it, and sleeps wake on the LPTMR. If the two are over 10 s apart, count it; the DS3231
// is still taken as right.
void checkInternalRtc(time_t ds){
  if(syncDs == 0) return;  // no sync yet
  time_t rtc = Teensy3Clock.get();
  if(((long) (ds - syncDs) >= 5) && (rtc == syncRtc)){
    internalRtcOK = false;
    Serial.println("Internal RTC stopped");
    return;
  }
  if(abs((long) (rtc - ds)) > 10){
    eeCount(EE_CLOCK_GAPS);
    Serial.printf("Internal RTC %ld s from the DS3231\n", (long) (rtc - ds));
  }
}

// Internal RTC time in ms (wraps), to measure how long a sleep lasted
uint32_t rtcMillis(){
  uint32_t s, p;
  do{
    s = RTC_TSR;
    p = RTC_TPR;
  } while(s != RTC_TSR);
  return s * 1000 + ((p & 0x7FFF) * 1000) / 32768;
}

// Check the clocks at power-up and pick the best one that works
void clockInit(){
  internalRtcOK = internalRTCRunning();
  calibrateLPO();

  boolean dsOK = readRTC();

  // Restarted during a deployment with the DS3231 stopped (stop flag set) or its time lost:
  // the internal RTC kept counting through the restart, so use it for the rest of the
  // deployment, and put its time back in the DS3231.
  if(dsOK & resumeDeployment & internalRtcOK){
    if((rtcStatus() & 0x80) || (t < TIME_2026)){
      dsOscStopped = true;
      reset_alarm();  // clears the stop flag
      time_t rtcNow = Teensy3Clock.get();
      setTime2(hour(rtcNow), minute(rtcNow), second(rtcNow), day(rtcNow), month(rtcNow), year(rtcNow) - 2000U);
      useBackupClock();
      t = Teensy3Clock.get();
      return;
    }
  }

  if(!dsOK) Serial.println("DS3231 does not answer");
  if(dsOK){
    if (t < TIME_2026) setTime2(16, 0, 0, 1, 11, 2026 - 2000U); // DEFAULT_TIME
    // Check DS3231 oscillator status
    if(rtcStatus() & 0x80){ // OSF bit set — oscillator stopped
      dsOscStopped = true;  // time may be wrong until it is set
      // Clear OSF flag and enable oscillator
      reset_alarm(); // clears status register 0x0F (including OSF)

      // Re-write time to kick oscillator
      readRTC();
      setTime2(hour(t), minute(t), second(t), day(t), month(t), year(t) - 2000U);

      cDisplay();
      display.println("DS3231 OSC");
      display.println("STOPPED");
      display.setTextSize(1);
      display.print("Clearing OSF...");
      display.display();
      delay(3000);

      dsOK = ds3231Running();
      cDisplay();
      display.println("DS3231 OSC");
      if(dsOK) display.println("RECOVERED");
      else display.println("FAILED");
      display.display();
      delay(3000);
    }
    else{
      dsOK = ds3231Running();
      if(!dsOK) Serial.println("DS3231 answers but is not counting");
    }
  }

  if(dsOK){
    clockSource = CLOCK_DS3231;
    disable_alarm();  // the internal RTC alarm wakes the logger; keep INT idle
    t = getTeensy3Time(1);  // sync teensy rtc to DS3231
    return;
  }

  useBackupClock();
  if(internalRtcOK) t = Teensy3Clock.get();
  else{
    if(softTime < TIME_2026) setSoftTime(DEFAULT_TIME);
    t = softNow();
  }
  cDisplay();
  display.println("RTC FAIL");
  display.setTextSize(1);
  display.println("Replace DS3231");
  if(internalRtcOK) display.println("Using internal clock");
  else display.println("Using LPO timer");
  display.println("Check date/time");
  display.display();
  delay(5000);
}

// Sleep until wakeTime. The internal RTC alarm wakes the logger; the DS3231 is only the time
// reference, read and synced to the internal RTC at each wake, so a faulty DS3231 cannot stop
// the logger waking. The LPTMR is a backup 2 s after the alarm. If the internal RTC is not
// running, the LPTMR is the alarm, in steps of up to a minute.
// sgtlOff: switch the SGTL5000 off while asleep (I2C needs it on).
// The watchdog is refreshed before and after the whole sleep, not between wakes: it is stopped
// while asleep, so only a sleep that keeps waking at once can run it out.
void sleepUntil(time_t wakeTime, boolean sgtlOff){
  watchdogRefresh();
  if(!internalRtcOK) calibrateLPO();
  while(1){
    long secs = wakeTime - getTeensy3Time(1);  // also syncs the internal RTC to the DS3231
    if(secs <= 0) break;

    long lpSecs = internalRtcOK ? secs + 2 : secs;
    if(lpSecs > 60) lpSecs = 60;
    uint32_t ticks = lpSecs * lpoHz;
    if(ticks > 65000) ticks = 65000;
    lptmr.setTimer(ticks);

    if(sgtlOff) digitalWrite(SGTL_EN, LOW);
    uint32_t rtcBefore = rtcMillis();
    int who;
    if(internalRtcOK){
      rtcAlarm.setRtcTimer(secs / 3600, (secs % 3600) / 60, secs % 60);
      who = Snooze.deepSleep(config_internal);
    }
    else who = Snooze.deepSleep(config_lptmr);
    /// ... sleeping ...
    if(sgtlOff) digitalWrite(SGTL_EN, HIGH); // power on so I2C works

    // Snooze adds the whole LPTMR period to millis() on every wake, counting its ticks as ms,
    // even when the RTC alarm woke the logger first. Set millis() to the time that passed: by the
    // internal RTC after an alarm wake (the alarm shows it is counting), otherwise by the LPO.
    uint32_t sleptMs;
    if(internalRtcOK & (who != LPTMR_WAKE)) sleptMs = rtcMillis() - rtcBefore;
    else sleptMs = (uint32_t) (ticks * 1000.0 / lpoHz);
    noInterrupts();
    systick_millis_count += sleptMs - ticks;  // unsigned wrap adds a negative difference
    interrupts();
    if(sleepDiags) wakeLogAdd(who, secs);  // printed at Record Start, once USB is back
  }
  watchdogRefresh();
}
