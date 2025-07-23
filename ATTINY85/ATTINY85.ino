#include <avr/sleep.h>
#include <avr/power.h>

// Definiciones de pines
#define REED_SWITCH PB3  // Pin sensor magnetico (PB3, Pin 2)
#define ESP_ON PB4     // Pin que enciende el ESP (PB4, Pin 3)

// Variable para almacenar el estado del pulsador
volatile bool buttonPressed = false;

void setup() {
  // Configura el pin como salida
  pinMode(ESP_ON, OUTPUT);
  digitalWrite(ESP_ON, LOW);

  // Configura el pin del reed switch como entrada con pull-up
  pinMode(REED_SWITCH, INPUT_PULLUP);

  // Habilita la interrupción PCINT para el pin PB3
  GIMSK |= (1 << PCIE);   // Habilita las interrupciones PCINT
  PCMSK |= (1 << PCINT3); // Habilita la interrupción en PB3 (PCINT3)
  sei();                  // Habilita las interrupciones globales

  ADCSRA &= ~ bit(ADEN); // Deshabilita el conversion analogico digital para reducir el consumo

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void loop() {
  if (buttonPressed) {
    // Activa el pin por 800 ms ya que es el tiempo necesario para que encienda el ESP8266
    digitalWrite(ESP_ON, HIGH);
    delay(800);
    digitalWrite(ESP_ON, LOW);

    // Reinicia la bandera
    buttonPressed = false;

    // Vuelve a dormir
    goToSleep();
  }else{
    // Vuelve a dormir
    goToSleep();
  }
}

// Función para entrar en modo de sueño profundo
void goToSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Modo de sueño profundo
  sleep_enable();                       // Habilita el modo de sueño
  sei();                                // Habilita las interrupciones globales
  sleep_cpu();                          // Entra en modo de sueño
  sleep_disable();                      // Deshabilita el modo de sueño al despertar
}

// Rutina de servicio de interrupción (ISR) para PCINT
ISR(PCINT0_vect) {
  buttonPressed = true;  // Cambia el estado de la bandera
}
