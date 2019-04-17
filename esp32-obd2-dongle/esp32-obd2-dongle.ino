byte ledState = LOW;

void setup() {
  // put your setup code here, to run once:
    pinMode(GPIO_NUM_33, OUTPUT);
}

void loop() {

    
    // put your main code here, to run repeatedly:
    vTaskDelay(100 * portTICK_PERIOD_MS);
    
    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(GPIO_NUM_33, ledState);
}
