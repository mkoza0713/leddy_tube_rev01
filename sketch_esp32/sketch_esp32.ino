/***********HTML**************/
#include <WiFi.h>
#include <HTTPClient.h>

const char ssid[] = "UPC7AA1BAF";
const char password[] = "Ra5xvvsxcpdj";

String HOST_NAME = "https://pivvwsrxfv.cfolks.pl";  // REPLACE WITH YOUR PC's IP ADDRESS
String PHP_FILE_NAME = "/connectEspDbIdDevice.php";         //REPLACE WITH YOUR PHP FILE NAME
String server = "";

String tempQuery1 = "?deviceWorkingState=";
String tempValue1 = "3";


unsigned int serverTime;
int serverTimeH;
int serverTimeM;
int serverTimeS;

/*
0 - light OFF
1 - morning state
2 - day-state
3 - afternoon state
4 - night state
*/


byte workingState = 0;  //tryb pracy
byte previousWoorkingState = 0;
long refreshTime = 600000;


#define relay1 17


void setup() {
  Serial.begin(115200);

  /***********HTML**************/
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP:");
  Serial.println(WiFi.localIP());


  
  pinMode(relay1, OUTPUT);

}

void loop() {
    getRequest();
    application();
    delay(refreshTime);

}
