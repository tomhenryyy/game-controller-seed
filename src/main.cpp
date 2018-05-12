/* Game Controller */
/*Thomas Henry W15005194*/
#include <mbed.h>
#include <EthernetInterface.h>
#include <rtos.h>
#include <mbed_events.h>
#include <FXOS8700Q.h>
#include <C12832.h>

/* display */
C12832 lcd(D11, D13, D12, D7, D10);
/* event queue and thread support */
Thread dispatch;
EventQueue periodic;
/* Accelerometer */
I2C i2c(PTE25, PTE24);
FXOS8700QAccelerometer acc(i2c, FXOS8700CQ_SLAVE_ADDR1);
/* Input from Potentiometers */
AnalogIn  left(A0);
AnalogIn right(A1);
DigitalIn stick(A3);
PwmOut speaker(D6);

enum{
    RED,GREEN,BLUE
};
DigitalOut RGB[] = {
    DigitalOut(PTB22, 1),DigitalOut(PTE26, 1),DigitalOut(PTC21, 1)
};

void lightOff(int rgb){
    RGB[rgb].write(1);
}
void lightOn(int rgb){
    RGB[rgb].write(0);
}
//variables about the status of the lander
bool isFlying = true;
bool isCrashed = false;
float percentFuelRemaining = 0.0;
float altitude = 0.0;
float orientation  =0.0;
float horizontalVelocity = 0.0;
float verticalVelocity = 0.0;
    //Variables about how much roll and throttle is applied
float rollRate = 0.0;
float throttle = 0.0;
//Getting the user Input
void user_input(void){
    //motion_data_units_t a;
    //acc.getAxis(a);
    // decide on what roll rate -1..+1 to ask for
    throttle = left.read()*100;
    //decide on what throttle setting 0..100 to ask for
    rollRate = (0.5 - right.read())*1.5;
        if(stick == 1){
            throttle = 100.0;
        }
}
//Setting the IP address - NEEDS TO BE HARDCODED
SocketAddress lander("192.168.90.3",65200);
SocketAddress dash("192.168.90.3",65250);
EthernetInterface eth;
UDPSocket udp;
//Function for communications with the lander
void communications(void){
    SocketAddress source;
    //Formatting the message to send to the Lander
    char buffer [512];
        user_input();
    sprintf(buffer, "command:!\r\nthrottle:%.2f\r\nroll:%.2f\r\n", throttle, rollRate);
    printf(buffer);
//Sending and receiving messages
            udp.sendto( lander, buffer, strlen(buffer));
            nsapi_size_or_error_t  n =
             udp.recvfrom(&source, buffer, sizeof(buffer));
             buffer[n]='\0';
    //Splitting messages into lines
    char *nextLine, *line;
    for(
          line = strtok_r(buffer, "\r\n", &nextLine);
          line !=NULL;
          line = (strtok_r(NULL,"\r\n", &nextLine))){
//Split the message into key value pairs
  char *key, *value;
      key = strtok(line, ":");
      value = strtok(NULL, ":");
      //Converting value strings into state variables
      if (strcmp(key, "altitude") ==0){
        altitude = atof(value);
      }
      if(strcmp(key, "fuel")==0){
        percentFuelRemaining = atof(value);
      }
      if(strcmp(key, "flying") ==0){
        isFlying = bool(value);
      }
      if(strcmp(key, "crashed")==0){
        isCrashed = bool(value);
      }
      if(strcmp(key, "orientation") ==0){
        orientation = atoi(value);
      }
      if(strcmp(key, "Vx") ==0){
        horizontalVelocity = atoi(value);
      }
      if(strcmp(key, "Vy") ==0){
        verticalVelocity = atoi(value);
      }
    }
}
//communications with the dashboard
void dashboard(void){
  //Making and formnatting the message
        char buffer [512];
        sprintf(buffer, "command:!\naltitude:%.2f\nfuel:%.2f\norientation:%.2f\nthrottle:%.2f\n", altitude, percentFuelRemaining, orientation, throttle);
  //Send the message to the dashboard
                udp.sendto( dash, buffer, strlen(buffer));

}

int main() {
    //connection : can take a few seconds
    printf("connecting \n");
    eth.connect();
    // write obtained IP address to serial monitor
    const char *ip = eth.get_ip_address();
    printf("IP address is: %s\n", ip ? ip : "No IP");
    //open udp for communications on the ethernet
    udp.open( &eth);
    printf("lander is on %s/%d\n",lander.get_ip_address(),lander.get_port() );
    printf("dash   is on %s/%d\n",dash.get_ip_address(),dash.get_port() );
      //periodic tasks
        periodic.call_every(40, communications);
        periodic.call_every(40, dashboard);
    dispatch.start( callback(&periodic, &EventQueue::dispatch_forever) );
    while(1) {
            char buffer[512];
            //Display Y Velocity, Fuel, Altitude and X Velocity on LCD
                sprintf(buffer, "Y Velocity: %.2f\nFuel: %.2f Altitude:%.2f\nX Velocity%.2f\n", verticalVelocity, percentFuelRemaining, altitude, horizontalVelocity);
                lcd.locate(0,0);
                lcd.printf(buffer);
                //if fuerl is below 20% play a sound
                float f;
                if(percentFuelRemaining<20){
                    for (f = 20.0; f<20.0e3; f+=100){
                        speaker.period(1.0/f);
                        speaker.write(0.5);
                        wait(0.1);

                    }
                }
                //Setting LED to RED, YELLOW AND GREEN Depending on
                //remaining fuel
                if (percentFuelRemaining>75){
                lightOn(GREEN);
                  lightOff(RED);
                  lightOff(BLUE);

                }
                else if (percentFuelRemaining < 75 && percentFuelRemaining>30){
                  lightOn(RED);
                }
                else{
                  lightOff(GREEN);
                }
                }
        wait(0.1);
    }
