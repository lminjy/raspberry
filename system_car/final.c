#include <stdio.h>
#include <wiringPi.h>
#include <pcf8591.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "rgb_led.c"
#include "lcd.c"
#include "ultrasonic_ranging.c"
#include "mpu6050_accl.c"

#define		PCF     120
#define		DOpin	0
#define BuzzerPin      4
#define  RoAPin    23
#define  RoBPin    22
#define  SWPin     21

static volatile int globalCounter = 0 ;

unsigned char flag;
unsigned char Last_RoB_Status;
unsigned char Current_RoB_Status;

//rotary
void btnISR(void)
{
	globalCounter = 0;
}

void rotaryDeal(void)
{
	Last_RoB_Status = digitalRead(RoBPin);

	while(!digitalRead(RoAPin)){
		Current_RoB_Status = digitalRead(RoBPin);
		flag = 1;
	}

	if(flag == 1){
		flag = 0;
		if((Last_RoB_Status == 0)&&(Current_RoB_Status == 1)){
			globalCounter ++;	
		}
		if((Last_RoB_Status == 1)&&(Current_RoB_Status == 0)){
			globalCounter --;
		}
	}
}

void Print(int x)
{
	switch(x)
	{
		case 1:
			printf("***************\n"  );
			printf(  "* Not Raining *\n"  );
			printf(  "***************\n");
		break;
		case 0:
			printf("*************\n"  );
			printf(  "* Raining!! *\n"  );
			printf(  "*************\n");
		break;
		default:
			printf("**********************\n"  );
			printf(  "* Print value error. *\n"  );
			printf(  "**********************\n");
		break;
	}
}


int fdjia;
int acclX, acclY, acclZ;
int gyroX, gyroY, gyroZ;
double acclX_scaled, acclY_scaled, acclZ_scaled;
double gyroX_scaled, gyroY_scaled, gyroZ_scaled;

int read_word_2c(int addr)
{
  int val=0;
  val = wiringPiI2CReadReg8(fdjia, addr);
  val = val << 8;
  val += wiringPiI2CReadReg8(fdjia, addr+1);
  if (val >= 0x8000)
    val = -(65536 - val);
  
  return val;
}

int main()
{	
	if(wiringPiSetup() == -1){
		printf("setup wiringPi failed !");
		return 1;
	}
	// Setup pcf8591 on base pin 120, and address 0x48
	pcf8591Setup(PCF, 0x48);
	pinMode(DOpin, INPUT);
	
	ledInit();
	
	ultraInit();

	fdjia = wiringPiI2CSetup (0x68);
  	wiringPiI2CWriteReg8 (fdjia,0x6B,0x00);//disable sleep mode 
  	printf("set 0x6B=%X\n",wiringPiI2CReadReg8 (fdjia,0x6B));

	fd = wiringPiI2CSetup(LCDAddr);
	init();
	int lastrain=0;

	pinMode(SWPin, INPUT);
	pinMode(RoAPin, INPUT);
	pinMode(RoBPin, INPUT);
	pullUpDnControl(SWPin, PUD_UP);
	if(wiringPiISR(SWPin, INT_EDGE_FALLING, &btnISR) < 0){
		fprintf(stderr, "Unable to init ISR\n",strerror(errno));	
		return 1;
	}
	int rotary = 0;
	
	//BUZZER
	pinMode(BuzzerPin,  OUTPUT);	

	while(1) // loop forever
	{
		printf("\n-------------------\n");
		int light = analogRead(PCF + 0);
		// 光敏传感器和双色灯模块
		printf("Light Value: %d\n", light);
		if(light>100) {
			ledColorSet(0x00,0x00,0xff);   //blue
			// delay(200);
			printf("Open the font light.\n");
		} else {
			ledColorSet(0x00,0x00,0x00);
		}
		
		
		//raindetect & lcd 模块
		int rain = analogRead(PCF + 1);
		printf("RainValue: %d\n", rain);
		int notrain=1;
		if(rain<80) {
			notrain=0;
		}
		if(notrain!=lastrain) {
			clear();
		}
		lastrain=notrain;
		Print(notrain);

		
		//超声波测距和蜂鸣器模块
		float dis = disMeasure();
		printf("distance:%0.2f cm\n",dis);
		if(dis<10) {
			printf("Too close.\n");
			int delay_time=10*dis;
			digitalWrite(BuzzerPin, LOW);
			delay(delay_time);
			digitalWrite(BuzzerPin, HIGH);
			delay(delay_time);			
		}


		// 总里程
		rotaryDeal();
		if (rotary != globalCounter){
			printf("Mileage: %d\n", globalCounter);
			rotary = globalCounter;
		}
		lcd(notrain,rotary);


		//加速度模块
		acclX = read_word_2c(0x3B);
    	acclY = read_word_2c(0x3D);
    	acclZ = read_word_2c(0x3F);

    	acclX_scaled = acclX / 16384.0;
    	acclY_scaled = acclY / 16384.0;
    	acclZ_scaled = acclZ / 16384.0;
    
    	// printf("My acclX_scaled: %f\n", acclX_scaled);
    	// printf("My acclY_scaled: %f\n", acclY_scaled);
    	// printf("My acclZ_scaled: %f\n", acclZ_scaled);

   	 	printf("My X rotation: %f\n", get_x_rotation(acclX_scaled, acclY_scaled, acclZ_scaled));
    	printf("My Y rotation: %f\n", get_y_rotation(acclX_scaled, acclY_scaled, acclZ_scaled));
		float x_rotation=get_x_rotation(acclX_scaled, acclY_scaled, acclZ_scaled);
		float y_rotation=get_y_rotation(acclX_scaled, acclY_scaled, acclZ_scaled);

		if(abs(x_rotation)>45 || abs(y_rotation)>45) {
			digitalWrite(BuzzerPin, LOW);
			delay(50);
			printf("Be careful for Rollover!\n");
		} else {
			digitalWrite(BuzzerPin, HIGH);
			delay(50);
		}

	}
	return 0;
}
