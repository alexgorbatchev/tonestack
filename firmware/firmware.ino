/* DAP Bass enhance example SGTL5000 only

This example code is in the public domain
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <ADC.h>

#define ADJUSTTONE 0
#define ADJUSTVOL 1


const int myInput = AUDIO_INPUT_LINEIN;
// const int myInput = AUDIO_INPUT_MIC;
const int linelevel[] = {31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13};
const float vpp[] = {1.16,1.22,1.29,1.37,1.44,1.53,1.62,1.71,1.8,1.91,2.02,2.14,2.26,2.39,2.53,2.67,2.83,2.98,3.16};
// Create the Audio components.  These should be created in the
// order data flows, inputs/sources -> processing -> outputs
//

const int adcVol = A7;
const int adcTreble = A6;
const int adcMid = A3;
const int adcBass = A2;

ADC *adc = new ADC();

AudioInputI2S       audioInput;         // audio shield: mic or line-in
AudioOutputI2S      audioOutput;        // audio shield: headphones & line-out


//de sinus test
//AudioSynthWaveformSine   sine1;          //xy=105,22

// Create an object to control the audio shield.
// 
AudioControlSGTL5000 audioShield;
//AudioMixer4              mixer1;         //xy=262,35

//fft stuff
//AudioAnalyzeFFT256       fft256_1;       //xy=271,49
//AudioConnection          patchCord1(adc1, fft256_1);

//peak stuff
AudioAnalyzePeak         peak1;          //xy=352,74
//AudioAnalyzePeak         peak2;          //xy=358,112

//ORIG
//AudioConnection          patchCord1(audioInput, 0, peak1, 0);
// Create Audio connections between the components
//
AudioConnection c1(audioInput, 0, audioOutput, 0); // left passing through
AudioConnection c2(audioInput, 1, audioOutput, 1); // right passing through

//sin test
//AudioConnection          patchCord1(audioInput, 0, mixer1, 1);
//AudioConnection          patchCord2(audioInput, 1, audioOutput, 1);
//A/udioConnection          patchCord3(sine1, 0, mixer1, 0);
//AudioConnection          patchCord4(mixer1, 0, audioOutput, 0);

//AudioConnection          patchCord2(audioInput, 1, peak2, 0);

void setup() {
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(4);
  // Enable the audio shield and set the output volume.
  audioShield.enable();
  audioShield.inputSelect(myInput);
  audioShield.volume(0.0);
  // just enable it to use default settings.
  audioShield.audioPostProcessorEnable();
  //audioShield.enhanceBassEnable(); // all we need to do for default bass enhancement settings.
  //audioShield.enhanceBass((float)lr_level,(float)bass_level);
  
  //celloaudioShield.enhanceBass((float)0.5,(float)0.5);
  // audioShield.enhanceBass((float)lr_level,(float)bass_level,(uint8_t)hpf_bypass,(uint8_t)cutoff);
  // please see http://www.pjrc.com/teensy/SGTL5000.pdf page 50 for valid values for BYPASS_HPF and CUTOFF

  audioShield.eqSelect(3);//eq
  audioShield.eqBands(0.0,0.0,0.0,0.0,0.0);
  //audioShield.lineOutLevel(20, 20);
  //audioShield.muteHeadphone();
  audioShield.unmuteLineout();


  //sint test
  //mixer1.gain(0, 0.5);
  //mixer1.gain(1, 0.5);
  //sine1.amplitude(0.0);
  //sine1.frequency(440);


  //ADC stuff
  pinMode(adcVol, INPUT);
  pinMode(adcTreble, INPUT);
  pinMode(adcMid, INPUT);
  pinMode(adcBass, INPUT);
  
  adc->setReference(ADC_REF_3V3, ADC_0);
  adc->setAveraging(32);
  adc->setResolution(8);
  adc->setConversionSpeed(ADC_LOW_SPEED);
  adc->setSamplingSpeed(ADC_LOW_SPEED);
  
  //serial comm
  Serial.begin(38400);
  
}

elapsedMillis chgMsec=0;
float lastVol=0.0;
float vol=0.0;
float bass =0.0;
float midbass = 0.0;
float mid =0.0;
float midtreble=0.0;
float treble = 0.0;
float left = 0.0;
float right = 0.0;

float diff;
float sign;
int inByte;
int step;
int level=2;//default 

int readvol_old = 0;
int readbass_old = 0;
int readmid_old = 0;
int readtreble_old = 0;

float peakval=0.0;

//performance
float ACPU;
float AMEM;
float CPU;
float MEM;

/*
float sign(float parameter){
  if (parameter<0) return -1.0;
  else return 1.0;
}
*/

void adjust(float *parameter, float target, int function){
    //prevent plops, slowly reduce in 0.04 steps
    float difference;

    difference = *parameter - target;
    
    if (difference == 0.0) return;
    
    if (difference>0.0) sign = +1.0;
    else sign = -1.0;
  
    step = (int)abs(difference/0.04);

    for (int i=0; i<step;i++) {
      
      *parameter = *parameter - sign*0.04;
      if (function == ADJUSTTONE)
        audioShield.eqBands(bass, midbass, mid, midtreble, treble);
      if (function == ADJUSTVOL)
        audioShield.volume(vol);
      delay(10);
    }
}

void reduce(float *parameter)
{
    //prevent plops, slowly reduce in 0.04 steps
    if (*parameter<0) sign = -1.0;
    else sign = 1.0;
  
    step = (int)abs(*parameter/0.04);

    for (int i=0; i<step;i++) {
      
      *parameter = *parameter - sign*0.04;
      audioShield.eqBands(bass, midbass, mid, midtreble, treble);
      delay(10);
    }
  
}

void defeat(){
  if (bass!=0.0) reduce(&bass);
  if (treble!=0.0) reduce(&treble);
  if (midbass!=0.0) reduce(&midbass);
  if (midtreble!=0.0) reduce(&midtreble);
  if (mid!=0.0) reduce(&mid);
}

void loop() {
  float target;
  // every 10 ms, check for adjustment
  if (chgMsec > 50) { // more regular updates for actual changes seems better.
    
    float vol1=analogRead(15)/1023.0;

    int readvol = (int)(adc->analogRead(adcVol));
    int readbass = (int)(adc->analogRead(adcBass, ADC_0));
    int readmid = (int)(adc->analogRead(adcMid, ADC_0));
    int readtreble = (int)(adc->analogRead(adcTreble, ADC_0));
    
    if (readvol!=readvol_old){
      //turned the volume knob
      //now take over from serial ctrl
      target = readvol/255.0;
      adjust(&vol, target, ADJUSTVOL);
      readvol_old = readvol;
    }

    if (readbass!=readbass_old){
      target = readbass/255.0*2.0-1.0;
      adjust(&bass, target, ADJUSTTONE);
      readbass_old = readbass;
    }

    if (readmid!=readmid_old){
      target = readmid/255.0*2.0-1.0;
      adjust(&mid, target, ADJUSTTONE);
      readmid_old = readmid;
    }


    if (readtreble!=readtreble_old){
      target = readtreble/255.0*2.0-1.0;
      adjust(&treble, target, ADJUSTTONE);
      readmid_old = readmid;
    }

  
    if (Serial.available()>0){
      inByte = Serial.read();
      if (inByte=='d'){
        defeat();
       }
      if (inByte=='D'){
        defeat();
        audioShield.audioProcessorDisable();
      }
      if (inByte=='e'){
        defeat();
        audioShield.audioPostProcessorEnable();
      }
      if (inByte == 'B'){
        bass += 0.04;
      }
      if (inByte == 'b'){
        bass -= 0.04;
      }
      if (inByte == 'N'){
        midbass += 0.04;
      }
      if (inByte == 'n'){
        midbass -= 0.04;
      }

      if (inByte == 'M'){
        mid += 0.04;
      }
      if (inByte == 'm'){
        mid -= 0.04;
      }
      if (inByte == 'Y'){
        midtreble += 0.04;
      }
      if (inByte == 'y'){
        midtreble -= 0.04;
      }

      if (inByte == 'T'){
        treble += 0.04;
      }
      if (inByte == 't'){
        treble -= 0.04;
      }
      if (inByte == 'V'){
        vol += 0.04;
      }
      if (inByte == 'v'){
        vol -= 0.04;
      }

      if (inByte == 'L'){
        level++;
        if (level>18) level=18;
        
        audioShield.lineOutLevel(linelevel[level]);
      }

      if (inByte == 'l'){
        level--;
        if (level<0) level=0;
        
        audioShield.lineOutLevel(linelevel[level]);
      }

      audioShield.eqBands(bass, midbass, mid, midtreble, treble);
      audioShield.volume(vol);

      //Some of the extra stuff

      
      if (peak1.available()){
        peakval = peak1.read();
      }
      
      
      ACPU = AudioProcessorUsage();
      AMEM = AudioMemoryUsage();
      CPU = peak1.processorUsage();
      
      Serial.print(bass);     //0
      Serial.print(":");
      Serial.print(midbass);  //1
      Serial.print(":");
      Serial.print(mid);      //2
      Serial.print(":");
      Serial.print(midtreble);//3
      Serial.print(":");
      Serial.print(treble);   //4
      Serial.print(":");
      Serial.print(right-left);//5
      Serial.print(":");
      Serial.print(vol);      //6
      Serial.print(":");
      Serial.print(vpp[level]);//7
      Serial.print(":");
      Serial.print(peakval);  //8
      Serial.print(":");
      Serial.print(CPU);     //9
      Serial.print(":");
      Serial.print(ACPU);     //10
      Serial.print(":");
      Serial.print(AMEM);   //11
      Serial.print(":");
      Serial.print(readbass);
      Serial.println();

    }

    /*
    if (mode == 1){
    
    if((vol1 - lastVol)>0.04){
      
      //audioShield.volume(vol1);
      bass += 0.04;//0.5 dB
      lastVol+=0.04;
      if (bass>1.0) bass=1.0;
      //lastVol=vol1;
      audioShield.eqBands(bass,treble);
      Serial.print("Bass increment ");
      Serial.println(bass);
    }
    if((lastVol-vol1)>0.04){
      
      //audioShield.volume(vol1);
      bass -= 0.04;//0.5 dB
      lastVol-=0.04;
      if (bass<0.0) bass=0.0;
      //lastVol=vol1;
      audioShield.eqBands(bass,treble);
      Serial.print("Bass decrement ");
      Serial.println(bass);

    }
    */
    chgMsec = 0;
    
    //audioShield.volume(0.7);
  }
}

