String s;
void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  delay(500);
  digitalWrite(2, HIGH);
  digitalWrite(3, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:

  if (Serial.available() > 0) {
    s = Serial.readStringUntil('\r');
    Serial.println(s);
    if (s == "F_ON") {
      Serial.println("Fan on");
      digitalWrite(2, LOW);
    } else if (s == "F_OFF") {
      Serial.println("Fan Off");
      digitalWrite(2, HIGH);
    } else if (s == "L_ON") {
      Serial.println("Light on");
      digitalWrite(3, LOW);
    } else if (s == "L_OFF") {
      Serial.println("Light Off");
      digitalWrite(3, HIGH);
    }
  }

  delay(100);
}
