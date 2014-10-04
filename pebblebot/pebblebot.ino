// BeanBot
// by Ben Harraway - http://www.gourmetpixel.com
// A simple demonsration for the LightBlue Bean by Punch Through Design
// This sketch looks for input into the scratch and moves a servo based on the scratch value
// This example code is in the public domain.

#include <Servo.h> 
 
Servo leftServo;  // create servo object to control a servo 
Servo rightServo;  // create servo object to control a servo 

uint16_t servoStill = 90;  
uint16_t previousLeftServoSpeed = 0;  
uint16_t previousRightServoSpeed = 0;

void setup() 
{ 
  leftServo.attach(0);  // attaches the servo on pin 2 to the servo object 
  rightServo.attach(1);  // attaches the servo on pin 3 to the servo object 
  
  // Initialize with motors stop
  Bean.setScratchNumber(1, servoStill);
  Bean.setScratchNumber(2, servoStill);
} 
 
 
void loop() 
{   
    uint16_t leftServoSpeed = Bean.readScratchNumber(1);
    uint16_t rightServoSpeed = Bean.readScratchNumber(2);

    if (leftServoSpeed != previousLeftServoSpeed) leftServo.write(leftServoSpeed);    
    if (rightServoSpeed != previousRightServoSpeed) rightServo.write(rightServoSpeed);    
    
    previousLeftServoSpeed = leftServoSpeed;
    previousRightServoSpeed = rightServoSpeed;
    
    Bean.sleep(100);
}
