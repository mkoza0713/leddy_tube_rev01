
void lightMode(byte mode) {
  digitalWrite(relay1, LOW);
  delay(20000);  //czekam zeby swietlowka zaczela od domyslnego
  switch (mode) {
    case 0:
      digitalWrite(relay1, LOW);
    break;
    case 1:  //swiecenie poranne
      digitalWrite(relay1, HIGH);
      delay(1000);
      digitalWrite(relay1, LOW);
      delay(1000);
      digitalWrite(relay1, HIGH);
      break;
    case 2:  //swiecenie dzienne
      digitalWrite(relay1, HIGH);
      delay(1000);
      break;
    case 3:  //swiecenie wieczorne
      digitalWrite(relay1, HIGH);
      delay(1000);
      digitalWrite(relay1, LOW);
      delay(1000);
      digitalWrite(relay1, HIGH);
      break;
    case 4:  //swiecenie nocne
      digitalWrite(relay1, HIGH);
      delay(1000);
      digitalWrite(relay1, LOW);
      delay(1000);
      digitalWrite(relay1, HIGH);
      delay(1000);
      digitalWrite(relay1, LOW);
      delay(1000);
      digitalWrite(relay1, HIGH);
      delay(1000);
      break;
  }
}