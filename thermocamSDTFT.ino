#include <tinyFAT.h>
#include <UTFT.h>
#include <UTFT_tinyFAT.h>
#include <UTouch.h>
#include <UTFT_Buttons.h>
#include <i2cmaster.h>
#include <Servo.h>


//-----------------THC--------------------------------------------------------------------
#define HORIZ_SERVO_PIN 8
#define VERT_SERVO_PIN 9
//This converts high and low bytes together and processes temperature, MSB is a error bit and is ignored for temps
double tempFactor = 0.02; // 0.02 degrees per LSB (measurement resolution of the MLX90614)
double tempData = 0x0000; // zero out the data
int dev = 0x5A<<1;
int data_low = 0;
int data_high = 0;
int pec = 0;
int currScanX = 0;
int mlr = 1275; //Middle point for LR Servo; only used if there is no existing calibration
int mud = 1750; //Middle point for UD Servo; only used if there is no existing calibration
int blr = 1600; //Left-Bottom point for LR Servo; only used if there is no existing calibration
int bud = 1550; //Left-Bottom point for UD Servo; only used if there is no existing calibration
int lines = 48;
int rows = 64;
int rawTemperaturesLine100[49]; //int = 36.6 *100 = 3660
Servo lr; //Servo for left-right movement
Servo ud; //Servo for up-down movement
//-------------------------TFT-------------------------------------------------
UTFT        myGLCD(TFT01_32,38,39,40,41); 
UTouch        myTouch(6,5,4,3,2);
UTFT_Buttons  myButtons(&myGLCD, &myTouch);
byte gradientArray[101][3] = {{ 5, 5, 190 },{ 13, 2, 178 },{ 14, 0, 175 },{ 18, 0, 172 },{ 22, 0, 169 },{ 26, 0, 166 },{ 30, 0, 163 },{ 35, 0, 160 },{ 40, 0, 156 },{ 45, 0, 152 },{ 51, 0, 149 },{ 56, 0, 144 },{ 62, 0, 140 },{ 68, 0, 136 },{ 75, 0, 131 },{ 81, 0, 126 },{ 88, 0, 122 },{ 94, 0, 117 },{ 101, 0, 112 },{ 108, 0, 107 },{ 114, 0, 102 },{ 121, 0, 97 },{ 128, 0, 92 },{ 135, 0, 87 },{ 141, 0, 83 },{ 147, 0, 79 },{ 152, 0, 75 },{ 158, 0, 70 },{ 164, 0, 66 },{ 170, 0, 62 },{ 175, 0, 58 },{ 181, 0, 54 },{ 186, 0, 50 },{ 192, 0, 46 },{ 197, 0, 42 },{ 202, 0, 38 },{ 207, 0, 35 },{ 212, 0, 31 },{ 217, 0, 28 },{ 221, 0, 24 },{ 226, 0, 21 },{ 230, 0, 18 },{ 234, 0, 15 },{ 238, 0, 12 },{ 242, 0, 10 },{ 245, 0, 7 },{ 248, 0, 5 },{ 251, 0, 3 },{ 254, 0, 1 },{ 255, 1, 0 },{ 255, 3, 0 },{ 255, 5, 0 },{ 255, 8, 0 },{ 255, 10, 0 },{ 255, 13, 0 },{ 255, 16, 0 },{ 255, 19, 0 },{ 255, 22, 0 },{ 255, 25, 0 },{ 255, 28, 0 },{ 255, 32, 0 },{ 255, 35, 0 },{ 255, 39, 0 },{ 255, 43, 0 },{ 255, 46, 0 },{ 255, 51, 0 },{ 255, 55, 0 },{ 255, 59, 0 },{ 255, 63, 0 },{ 255, 67, 0 },{ 255, 72, 0 },{ 255, 76, 0 },{ 255, 80, 0 },{ 255, 85, 0 },{ 255, 90, 0 },{ 255, 94, 0 },{ 255, 99, 0 },{ 255, 103, 0 },{ 255, 108, 0 },{ 255, 113, 0 },{ 255, 118, 0 },{ 255, 122, 0 },{ 255, 128, 0 },{ 255, 136, 0 },{ 255, 145, 0 },{ 255, 153, 0 },{ 255, 161, 0 },{ 255, 170, 0 },{ 255, 178, 0 },{ 255, 185, 0 },{ 255, 193, 0 },{ 255, 200, 0 },{ 255, 207, 0 },{ 255, 214, 0 },{ 255, 220, 0 },{ 255, 227, 0 },{ 255, 232, 0 },{ 255, 237, 0 },{ 255, 242, 0 },{ 255, 247, 0 }, { 255, 252, 17 } };
extern uint8_t SmallFont[];
extern uint8_t BigFont[];
//--------------------------SD--------------------------------------------------
byte sdRes;
word sdResult;
char newFileNameChars[] = "T999.THC";
char currFileNameChars[] = "T999.THC";
//-------------------------render-------------------------------------------------
int renderMaxT=-10000;
int renderMinT =10000;
//-------------------------global--------------------------------------------------
int stateMachine = 0; //0-main menu 1-scan 2-render 
int prevStateMachine = -1;
int buttonHome=0;
int buttonScan = 0;
int pressedButton = 0;
///////---------------------------------------------------------------------------------------------------------------

int getRawTemperature100(){
	i2c_start_wait(dev+I2C_WRITE);
	i2c_write(0x07);	
	// read
	i2c_rep_start(dev+I2C_READ);
	data_low = i2c_readAck(); //Read 1 byte and then send ack
	data_high = i2c_readAck(); //Read 1 byte and then send ack
	pec = i2c_readNak();
	i2c_stop();	
	
	// This masks off the error bit of the high byte, then moves it left 8 bits and adds the low byte.
	tempData = (double)(((data_high & 0x007F) << 8) + data_low);
	tempData = (tempData * tempFactor)-0.01;
	
	int celcius100 = (tempData - 273.15)*100;
	delay(50);	
	return celcius100;
}


void scan() {
	int yPos = bud;
	int yInc = ((mud - bud) * 2) / (lines - 1);
	int xPos = blr;
	int xInc = ((blr - mlr) * 2) / (rows - 1);
	currScanX = 0;	
	
	sdRes=file.initFAT();
	if (file.exists(newFileNameChars)) file.delFile(newFileNameChars);	
	file.create(newFileNameChars);
	delay(100); 	
	
	myGLCD.setBackColor(VGA_SILVER);
	myGLCD.setColor(VGA_BLACK);	
	myGLCD.setFont(BigFont);	
	myGLCD.print(newFileNameChars,CENTER, 128);
	myGLCD.setColor(VGA_RED);		
	myGLCD.fillRect(0,53,319,53);
	myGLCD.fillRect(0,101,319,101);
	
	sdRes=file.openFile(newFileNameChars, FILEMODE_TEXT_WRITE);

	ud.writeMicroseconds(yPos);
	lr.writeMicroseconds(xPos);
	for (int x = 0; x < rows; x++) { 	    
		for (int y = 0; y < lines; y++) {
			rawTemperaturesLine100[y]	= (int)getRawTemperature100();
			
			if (y != lines - 1) {
				yPos += yInc;
				ud.writeMicroseconds(yPos);        
			}
		}
		if (x != rows - 1) {
			xPos -= xInc;
			lr.writeMicroseconds(xPos);
		}
		// Move fairly quickly so that we don't scan the latent temperature of objects in the middle.
		// Don't move too fast to cause a lot of jiggle.
		for (int i = 0; i < (lines - 1); i++) {
			yPos -= yInc;
			ud.writeMicroseconds(yPos);
			delay(2);
		}    
		saveTemperaturePixelToSD(rawTemperaturesLine100, lines);
		currScanX++;
	}  
	if (sdRes==NO_ERROR)
	{   		
		file.closeFile();
	}	
	
}


void saveTemperaturePixelToSD(int *rawTemperaturesLine100, int length){	
	char temperatureChars[16];      	
	for (int idx = 0; idx < length; idx++) {
		itoa(rawTemperaturesLine100[idx],temperatureChars,DEC);    
		if (sdRes==NO_ERROR)
		{    
			file.writeLn(temperatureChars);              		
		}       
	}	
	myGLCD.setColor(VGA_RED);		
	myGLCD.fillRect(0,53,currScanX*5+4,101);
}



void makeNewFilename(){
	int newFileNameIndex = 1;
	char sdTextBuffer[32];
	sdRes=file.initFAT();
	if (file.exists("FILES.SYS"))
	{  
		sdRes=file.openFile("FILES.SYS", FILEMODE_TEXT_READ);
		if (sdRes==NO_ERROR)
		{
			sdResult=0;
			//while ((sdResult!=EOF) and (sdResult!=FILE_IS_EMPTY)) {
			sdResult=file.readLn(sdTextBuffer, 32);
			newFileNameIndex += atoi(sdTextBuffer);				
			newFileNameChars[0] = 'T'; //(newFileNameIndex/1000)%10 + '0'; 
			newFileNameChars[1] = (newFileNameIndex/100)%10 + '0'; 
			newFileNameChars[2] = (newFileNameIndex/10)%10 + '0'; 
			newFileNameChars[3] = newFileNameIndex%10 + '0';  
			newFileNameChars[4] = '.';
			newFileNameChars[5] = 'T';
			newFileNameChars[6] = 'H';	
			newFileNameChars[7] = 'C';			
			//}			
			file.closeFile();
		}		
	}
	sdRes=file.initFAT(); 
	if (file.exists("FILES.SYS")){
		file.delFile("FILES.SYS");	
	}
	file.create("FILES.SYS");
	sdRes=file.openFile("FILES.SYS", FILEMODE_TEXT_WRITE);
	if (sdRes==NO_ERROR)
	{   
		char newFileNameIndexChars[] = "0000";
		itoa(newFileNameIndex,newFileNameIndexChars,DEC);
		file.writeLn(newFileNameIndexChars);		
		file.closeFile();
	}		
}



void renderFindMaxMinT(){
	char sdTextBuffer[32];		
	int renderTemperature;
	sdRes=file.initFAT();
	if (file.exists(currFileNameChars))
	{  
		sdRes=file.openFile(currFileNameChars, FILEMODE_TEXT_READ);
		if (sdRes==NO_ERROR)
		{
			sdResult=0;
			while ((sdResult!=EOF) and (sdResult!=FILE_IS_EMPTY))
			{
				sdResult=file.readLn(sdTextBuffer, 32);
				if (sdResult!=FILE_IS_EMPTY)
				{
					renderTemperature = atoi(sdTextBuffer);	
					if(renderTemperature != 0){					
						if(renderMaxT < renderTemperature){ 
							renderMaxT = renderTemperature; 
						}
						if(renderMinT > renderTemperature){ 
							renderMinT = renderTemperature; 
						}   }        
				}        
			}      
			file.closeFile();
		}    
	} 	   
}

int renderConvertTemperatureToColorPos(int renderTemperature){
	return constrain(map(renderTemperature, renderMinT, renderMaxT, 0, 99),0,99); 
}

void renderDrawPixelTemperature(int renderTemperature, int x, int y){
	int renderPixelColorIndex;
	renderPixelColorIndex = renderConvertTemperatureToColorPos(renderTemperature);     
	myGLCD.setColor(gradientArray[renderPixelColorIndex][0], gradientArray[renderPixelColorIndex][1], gradientArray[renderPixelColorIndex][2]);
	//myGLCD.drawPixel(x,y);
	myGLCD.fillRect(x*5,y*5,x*5+4,y*5+4);
}

void renderResult(){	
	int x=0;
	int y=47;
	int renderTemperature = 0;
	char sdTextBuffer[32];
	sdRes=file.initFAT();
	if (file.exists(currFileNameChars))
	{  
		sdRes=file.openFile(currFileNameChars, FILEMODE_TEXT_READ);
		if (sdRes==NO_ERROR)
		{
			sdResult=0;
			while ((sdResult!=EOF) and (sdResult!=FILE_IS_EMPTY))
			{
				sdResult=file.readLn(sdTextBuffer, 32);
				if (sdResult!=FILE_IS_EMPTY)
				{
					renderTemperature = atoi((char*)sdTextBuffer);		  
					renderDrawPixelTemperature(renderTemperature,x,y);
					y--;
					if(y<0) { 
						x++; 
						y=47; 
					}
				}        
			}      
			file.closeFile();
		}    
	}
	myGLCD.setBackColor(VGA_WHITE);
	myGLCD.setColor(VGA_BLACK);	
	myGLCD.setFont(SmallFont);	
	myGLCD.printNumF(renderMinT/100.0,2,10, 226);
	myGLCD.print(currFileNameChars,140, 226);
	myGLCD.printNumF(renderMaxT/100.0,2,250, 226);
	
}

void setup()
{  	
    //Serial.begin(19200);
	i2c_init();	
	myGLCD.InitLCD();	
	myTouch.InitTouch();
	myTouch.setPrecision(PREC_MEDIUM); 
    myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_WHITE, VGA_RED, VGA_BLACK);	
	myButtons.setTextFont(BigFont);
	buttonScan = myButtons.addButton( 60,  30, 200,  30, "SCAN");	
	myButtons.setTextFont(SmallFont);
	buttonHome = myButtons.addButton( 303, 0, 16,  16, "X");	
	ud.attach(VERT_SERVO_PIN); //Attach servos
	lr.attach(HORIZ_SERVO_PIN); 
	ud.writeMicroseconds(mud); //Move servos to middle position
	lr.writeMicroseconds(mlr);	
}



void loop()
{ 	 
	if (myTouch.dataAvailable() == true)
	{
		pressedButton = myButtons.checkButtons();
		if (pressedButton==buttonScan)
		{
			stateMachine = 1;
		}
		else if(pressedButton==buttonHome){
			stateMachine = 0;
		}
	}
	

	//------------------ STATE MACHINE ---------------------------
	if(prevStateMachine != stateMachine){	
        //Serial.println(stateMachine,DEC);	
		myGLCD.clrScr();
		myGLCD.fillScr(VGA_SILVER);
		//HOME
		if(0==stateMachine){
			prevStateMachine = stateMachine;			
			myButtons.drawButton(buttonScan);			
		}
		//SCAN
		else if(1==stateMachine){
			prevStateMachine = stateMachine;
			myGLCD.setBackColor(VGA_SILVER);
			myGLCD.setColor(VGA_BLACK);	
			myGLCD.setFont(BigFont);
			myGLCD.print("SCAN START",CENTER,10);
			makeNewFilename();	
			delay(500);	
			scan();
			strcpy(currFileNameChars,newFileNameChars);
			stateMachine = 2;	            	
            myGLCD.print("SCAN READY",CENTER, 171);				
		}
		//RENDER
		else if(2==stateMachine){
			prevStateMachine = stateMachine;
			myGLCD.setBackColor(VGA_SILVER);
			myGLCD.setColor(VGA_BLACK);	
			myGLCD.setFont(BigFont);	
			myGLCD.print("RENDER START",CENTER, 10);
			renderFindMaxMinT();
			renderResult();	
			myButtons.drawButton(buttonHome);			
		}
		
	}
}

