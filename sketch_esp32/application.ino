void application() {
  //workingState  - stan z serwera

  // serverTimeS  - czas
  // serverTimeH
  // serverTimeM
  // serverTimeS

  //tempValue1 - wiadomosc do serwera

  /*
  0 - OFF
  1 - rano
  2 - dzienne
  3 - wieczorne
  4 - nocne
  */


  if (previousWoorkingState != workingState) {
    Serial.print("zmiana statusu z: ");
    Serial.print(previousWoorkingState);
    Serial.print(" na: ");
    Serial.println(workingState);
    previousWoorkingState=workingState;
    tempValue1 = workingState;
    switch (workingState) {
      case 0:
        lightMode(0);
        break;
      case 1:
        lightMode(1);
        break;
      case 2:
        lightMode(2);
        break;
      case 3:
        lightMode(3);
        break;
      case 4:
        lightMode(4);
        break;
    }
  }
}