void getRequest() {
  HTTPClient http;
  //server = HOST_NAME + PHP_FILE_NAME + tempQuery0 + tempValue0 + "&" + tempQuery1 + tempValue1;
  server = HOST_NAME + PHP_FILE_NAME + tempQuery1 + tempValue1;
  http.begin(server);
  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      Serial.println(payload);

      //tryb pracy
      byte firstSign = payload.indexOf("?serverWorkingState=") + 20;
      byte lastSign = firstSign + 1;
      String getText = "";
      for (byte i = firstSign; i < lastSign; i++) {
        getText = getText + payload[i];
      }
      workingState = getText.toInt();

      /***************************************/
      // Serial.println(workingState);
      // tempValue1=workingState;
      /***************************************/


      //godzina
      firstSign = payload.indexOf("?serverTime=") + 12;
      lastSign = firstSign + 5;
      getText = "";
      for (byte i = firstSign; i < lastSign; i++) {
        getText = getText + payload[i];
      }
      serverTime = getText.toInt();
      serverTimeH = serverTime/3600;
      serverTimeM = (serverTime-(serverTimeH*3600))/60;
      serverTimeS = (serverTime-(serverTimeH*3600)-(serverTimeM*60));
      // Serial.print(serverTimeH);Serial.print(":");
      // Serial.print(serverTimeM);Serial.print(":");
      // Serial.println(serverTimeS);
      // Serial.println(serverTime);


    } else {
      Serial.printf("HTTP GET... code: %d\n", httpCode);
    }
  } else {
    Serial.printf("HTTP GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}
