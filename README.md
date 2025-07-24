# Sensor WiFi para Puertas, Cortinas y Ventanas con ATTiny85 y Wemos D1 Mini

Este proyecto presenta un sensor magnético Wi-Fi autónomo diseñado para el monitoreo eficiente y de larga duración del estado (abierto/cerrado) de puertas, ventanas, o cualquier acceso. Su ingeniería se centra en la optimización del consumo energético, lo que permite una operación prolongada sin necesidad de recarga frecuente, haciéndolo ideal para aplicaciones de seguridad, domótica o monitoreo industrial donde la fiabilidad y la autonomía son críticas.

El sistema combina la eficiencia de un ATTiny85 para la gestión de eventos locales y la conectividad del Wemos D1 Mini (ESP8266) para la transmisión de eventos a un servidor HTTP o plataforma IoT.

Tanto el ATTiny85 como el Wemos D1 Mini son programables, lo que permite:
- Configurar la forma en que se detectan los eventos (apertura/cierre).
- Personalizar la forma de envío (por ejemplo, hacer solicitudes HTTP a tu propio sistema).
- Además, el ATTiny85 puede ser modificado para trabajar en base a intervalos de tiempo, utilizando librerías como <a href="https://github.com/connornishijima/TinySnore">TinySnore</a>, mejorando aún más la eficiencia energética y permitiendo distintas estrategias de notificación.

![alt text](https://raw.githubusercontent.com/LucasPifo/Sensor-magnetico-lowpower/refs/heads/main/Imagenes/Sensor%20completo.jpeg)

## 💡 Problemática que se buscaba resolver
- Monitorear en tiempo real la apertura y cierre de cortinas, puertas o cualquier acceso.
- Registrar el tiempo que permanecía abierta cada acceso.
- Funcionar con baterías para maximizar el tiempo de operación.
- Evitar instalaciones eléctricas complejas por cada sensor (sin necesidad de cableado electrico).
- Facilitar el montaje y desmontaje de los sensores de forma rápida y sencilla.

Este diseño permite cumplir todos esos requisitos, proporcionando una solución portátil, inalámbrica y de bajo consumo para entornos industriales exigentes.

## 🧠 Decisiones de diseño
Una alternativa viable para este proyecto hubiera sido utilizar simplemente una compuerta lógica para la detección de eventos, eliminando así la necesidad de programar dos microcontroladores.
Sin embargo, nuestro enfoque está más orientado a la programación y la escalabilidad, por lo que decidimos integrar un ATTiny85 adicional. Sí, el costo es ligeramente mayor, pero obtenemos mayor flexibilidad.
El ATTiny85 permite escalar el sistema fácilmente.
Gracias a la programación, no solo podemos usar interrupciones físicas en los pines, sino también programar el sistema para que trabaje por intervalos de tiempo (ej: cada X segundos revisar estado), sin modificar significativamente el circuito existente.
Esta decisión nos permite tener un sistema más inteligente y modular, adaptable a distintos proyectos futuros simplemente cambiando el firmware.

## 🔧 Diseño de hardware
Una de las prioridades en el diseño del circuito fue que los componentes fueran de montaje pasante (THT), con el fin de:
- Simplificar la soldadura.
- No requerir herramientas o materiales especializados más allá de un cautín, soldadura y pasta para soldar.

De esta manera, cualquier persona puede ensamblar el sensor de forma sencilla, sin necesidad de equipo costoso como estaciones de aire caliente.
Importante:
Todos los componentes son de tipo pasante, excepto un componente:
El `MOSFET AO3413`, que es de montaje superficial (SMD).

## 🛠️ Materiales
- 1x Wemos D1 Mini (ESP8266)
- 1x ATTiny85
- 1x Batería 18650
- 1x Porta batería para 18650
- 1x MOSFET AO3413 (SMD)
- 2x Diodos de propósito general
- 2x Transistores 2N3904
- 1x Pulsador (push button)
- 1x Reed Switch (sensor magnético)
- 1x Imán
- Resistencias: 220 Ω, 1 kΩ, 10 kΩ, 46 kΩ, 100 kΩ, 220 kΩ, 1 MΩ

## 🔌 Diagrama de conexión
![alt text](https://raw.githubusercontent.com/LucasPifo/Sensor-magnetico-lowpower/refs/heads/main/Imagenes/Esquema%20electronico.bmp)

## 🧩 PCB
![alt text](https://raw.githubusercontent.com/LucasPifo/Sensor-magnetico-lowpower/refs/heads/main/Imagenes/Dise%C3%B1o%20PCB.png)

## ⚡ Funcionamiento y Flujo
### Estado de Reposo (Ultra Bajo Consumo):

El ATtiny85 está en modo `SLEEP_MODE_PWR_DOWN`, con el ADC deshabilitado, y monitorea pasivamente el Reed Switch a través de una interrupción (`PCINT`). Su consumo es de `(0.1mA REED SWITCH CERRADO y 47uA REED SWITCH ABIERTO)`, asegurando que la batería 18650 dure por meses o incluso años en este estado.

El Wemos D1 Mini (ESP8266) y gran parte del circuito de enclavamiento están completamente desenergizados, sin consumir corriente alguna.

Detección de Evento y Activación:

Cuando el Reed Switch cambia de estado (se abre o se cierra), la interrupción se dispara en el ATtiny85.

El ATtiny85 "despierta" y envía un pulso `HIGH de 800 ms` a un pin (`ESP_ON`) conectado al Circuito de Enclavamiento. Este pulso es el tiempo crítico necesario para que el ESP8266 reciba energía y comience su proceso de arranque.

El Circuito de Enclavamiento, al recibir este pulso, se activa y comienza a suministrar energía de la batería 18650 al Wemos D1 Mini.

### Procesamiento y Comunicación del ESP8266:

El Wemos D1 Mini se enciende. Rápidamente:

Lee el estado actual del Reed Switch (confirmando el evento que lo despertó).

Lee el nivel de la batería 18650 a través de su pin analógico.

Intenta conectar a la red Wi-Fi configurada (utilizando credenciales guardadas en su memoria flash o entrando en modo de configuración si no hay).

Valida la conexión HTTPS al servidor de destino, incluyendo la gestión y actualización de la huella digital (fingerprint) del certificado SSL para una comunicación segura.

Construye un paquete JSON con la MAC del dispositivo, el porcentaje de batería, el tiempo que el ESP lleva encendido y una cola de eventos del sensor (registrando múltiples cambios si ocurrieron durante el corto período de actividad).

Envía este payload al servidor a través de una petición HTTPS POST con una API Key para autenticación.

Después de enviar los datos, el ESP8266 espera 2 segundos adicionales para detectar cualquier cambio de estado del sensor que pudiera haber ocurrido justo después del envío, asegurando que ningún evento se pierda. Si hay nuevos eventos, los envía inmediatamente.

### Retorno al Ultra Bajo Consumo:

Una vez que el Wemos D1 Mini ha completado la transmisión de datos (o después de un período de inactividad en modo de configuración), el ESP8266 baja un pin de control (`ESP_OFF`).

Este pin está conectado al Circuito de Enclavamiento, el cual interpreta esta señal para cortar completamente la alimentación al Wemos D1 Mini, devolviendo el sistema a su estado de ultra bajo consumo, con solo el ATtiny85 monitoreando el Reed Switch.

### 🌐 Configuración Wi-Fi (Modo Access Point)
Para la configuración inicial de la red Wi-Fi o en caso de fallo de conexión, el ESP8266 activa un Modo Access Point (AP):

`SSID Predeterminado: "Sensor_magnetico_lowpower"`
`Contraseña Predeterminada: "12345678"`

Interfaz Web: Una vez conectado a esta red, los usuarios pueden acceder a una página de configuración (servida desde la memoria LittleFS del ESP8266) visitando la IP del ESP (normalmente `192.168.4.1`).

Opciones de Configuración: La interfaz permite configurar:

SSID y Contraseña de la red Wi-Fi de destino.

Configuración de IP Fija o DHCP: Si se elige IP fija, se pueden introducir la IP, Gateway, Subred y hasta dos DNS. La opción de IP fija se resalta como un método para acelerar la conexión y, potencialmente, prolongar la vida de la batería al reducir el tiempo de activación del ESP.

Guardado y Reinicio: Al guardar la configuración, el ESP reinicia y intenta conectarse a la nueva red.

Botón de Reset: Mantener un botón físico (`RESET - D1`) presionado durante 5 segundos al encender el ESP borrará la configuración Wi-Fi guardada, forzando al dispositivo a entrar nuevamente en modo AP.

Temporizador de AP: El ESP en modo AP tiene un temporizador de 3 minutos. Si no se configura la Wi-Fi dentro de este tiempo, el ESP se apagará automáticamente para ahorrar energía, requiriendo una nueva activación manual para entrar en modo AP de nuevo.

## 💻 Guía de Programación y Carga de Firmware
Para poner en marcha el sensor, deberás cargar el firmware en ambos microcontroladores:

1. Programación del ATtiny85
El ATtiny85 se programa utilizando un programador ISP. Puedes configurar un Arduino UNO como un programador ISP para este propósito.

Configurar Arduino UNO como ISP:

En el Arduino IDE, abre el ejemplo `"ArduinoISP" (Archivo > Ejemplos > 11.ArduinoISP > ArduinoISP)`.

Sube este sketch a tu placa Arduino UNO.

Conexión del ATtiny85 al Arduino UNO (como ISP):

- Arduino UNO (Pin 13) ➡️ ATtiny85 (Pin 2 - SCK)
- Arduino UNO (Pin 12) ➡️ ATtiny85 (Pin 1 - MISO)
- Arduino UNO (Pin 11) ➡️ ATtiny85 (Pin 0 - MOSI)
- Arduino UNO (Pin 10) ➡️ ATtiny85 (Pin 5 - RESET)
- Arduino UNO (5V) ➡️ ATtiny85 (Pin 8 - VCC)
- Arduino UNO (GND) ➡️ ATtiny85 (Pin 4 - GND)

Carga del Firmware al ATtiny85:

Asegúrate de tener las ATtinyMicrocontrollers Boards instaladas en el Arduino IDE `(Herramientas > Placa > Gestor de Tarjetas...)`.

En Arduino IDE, ve a `Herramientas > Placa > ATtinyMicrocontrollers > ATtiny25/45/85`.

Selecciona Procesador: `ATtiny85`.

Asegúrate de que Programador: Arduino as ISP esté seleccionado.

Abre el archivo `ATTINY85/ATTINY85.ino`.

Haz clic en Programa > Subir Usando Programador.

2. Programación del Wemos D1 Mini (ESP8266)
El Wemos D1 Mini se programa directamente a través de su puerto USB, utilizando el Arduino IDE.

Configuración del Arduino IDE para ESP8266:

Si no lo has hecho, instala el soporte para placas ESP8266. Ve a `Archivo > Preferencias, y en "URLs Adicionales de Gestores de Tarjetas"`, añade: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`

Ve a `Herramientas > Placa > Gestor de Tarjetas...` y busca `"esp8266"` e instálalo.

Preparar la Memoria Flash con la Página Web:

Instala la herramienta ESP8266 LittleFS Data Upload en tu Arduino IDE (`Herramientas > Gestor de Librerías`).

En Herramientas, selecciona ESP8266 LittleFS Data Upload. Esto cargará `index.html` a la memoria flash del ESP8266.

Configuración de la Placa y Puerto:

En `Herramientas > Placa`, selecciona `LOLIN(WEMOS) D1 R2 & mini`.

Selecciona el puerto COM/USB correcto al que está conectado tu Wemos D1 Mini.

Carga del Firmware:

Abre el archivo `ESP8266/ESP8266.ino`.

Modifica `const char* HOST = ""` con la IP o dominio del servidor donde alojaras tu backend.

Haz clic en el botón Subir (flecha a la derecha).

## 🚀 Cómo Probar el Proyecto Completo (Sensor + API + Base de Datos)

1. Configuración de la Base de Datos (MySQL)
Este proyecto utiliza MySQL para almacenar los datos de las detecciones del sensor.
Crea la base de datos y la tabla detecciones ejecutando el script SQL (`api/db/query.sql`) en tu servidor MySQL (puedes usar phpMyAdmin, MySQL Workbench, o la línea de comandos):

2. Certificados SSL
Para que tu servidor Node.js funcione con HTTPS, necesitas un par de certificados SSL: una clave privada (`key.pem`) y un certificado (`cert.pem`). Para desarrollo local, puedes crear certificados autofirmados usando OpenSSL.

Abre tu terminal o línea de comandos.

Navega hasta la carpeta api.

Crea la carpeta certs:

Genera la clave privada (key.pem) y certificado autofirmado (cert.pem):

``` 
openssl req -x509 -newkey rsa:2048 -nodes -keyout key.pem -out cert.pem -days 365
``` 

Durante este proceso, se te pedirán algunos detalles (país, organización, etc.). Puedes rellenarlos o dejarlos en blanco, ya que son para un certificado de desarrollo. Una vez generados, estos archivos deben estar en la carpeta certs.

3. Variables de Entorno (.env)
El archivo .env almacena variables de configuración sensibles que no deben ser públicas ni subirse al control de versiones (como Git). En la raíz de tu carpeta api, crea el archivo `.env` con el siguiente contenido. Asegúrate de reemplazar los valores de ejemplo con los tuyos propios:

```
PORT_WEB=4430 # Puerto donde tu API escuchará (e.g., 4430)
DB_HOST="localhost" # Host de tu base de datos MySQL
DB_USER="root" # Usuario de tu base de datos MySQL
DB_PASSWORD= # Contraseña de tu base de datos MySQL (déjalo vacío si no tienes, pero NO recomendado para producción)
DB_PORT=3306 # Puerto de tu base de datos MySQL (por defecto 3306)
DB_NAME="sensores" # Nombre de la base de datos que creaste (e.g., "sensores")
API_KEY="api-key-secret" # ¡IMPORTANTE! Cambia esto por una clave secreta y única para autenticar tus sensores.
```

4. Instalación de Dependencias
Antes de ejecutar la API, debes instalar todas las librerías de Node.js que el proyecto utiliza.

Abre tu terminal o línea de comandos.

Navega hasta la raíz de tu carpeta api

Ejecuta el siguiente comando para instalar las dependencias:
```
npm install express helmet dotenv cors morgan mysql2 moment
```
Este comando descargará e instalará todas las librerías necesarias para el funcionamiento del servidor, la interacción con la base de datos y la gestión de fechas.

5. Ejecutar el Servidor API
Una vez que hayas completado los pasos anteriores (certificados, .env configurado e instalaciones de dependencias), puedes iniciar tu servidor Node.js.

Asegúrate de que estás en la raíz de tu carpeta api en tu terminal.

Ejecuta el siguiente comando:

```
node index.js
```
Si todo está configurado correctamente, verás un mensaje en tu consola indicando que el servidor HTTPS está en funcionamiento:

`Servidor HTTPS corriendo en https://localhost:4430`

6. Abre el archivo en tu navegador: Simplemente abre el archivo index.html directamente en tu navegador web (doble clic o arrástralo a la ventana del navegador).

## 🔋 Consumo (Ejemplo)

A continuación se presenta un ejemplo aproximado del consumo energético del dispositivo en una configuración típica:

- **Modo stand-by (reed switch cerrado):**  
  Consumo: `~0.1 mA`

- **Modo activo (ESP encendido):**  
  Consumo: `~80 mA`
  Duración activa estimada: `entre 6 y 12 segundos` (en condiciones óptimas con IP fija)

- **Promedio de activaciones por día:**  
  - 15 aperturas + 15 cierres = 30 activaciones  
  - Promedio de duración por activación: 7 segundos  
  - Tiempo total activo diario: 30 × 7s = 210 segundos = `3.5 minutos`

- **Consumo diario aproximado:**  
  - En modo activo:  
    80 mA × (3.5 / 60) horas = 4.67 mAh  
  - En modo stand-by (23.94 horas restantes):  
    0.1 mA × 23.94 horas = `2.394 mAh`

- **Consumo total diario:**  
  4.67 mAh + 2.394 mAh = `7.06 mAh`

- **Batería utilizada:**  
  `3000 mAh`

- **Autonomía estimada:**  
  3000 mAh / 7.06 mAh ≈ 425 días  
  Es decir, `aproximadamente 1 año y 2 meses` de funcionamiento continuo con una sola carga (en condiciones óptimas).

> ⚠️ Nota: Esta estimación puede variar dependiendo de la calidad de la señal WiFi, el estado de la batería, temperatura ambiente, y otros factores externos que pueden influir en los tiempos de conexión y consumo energético.

## ✨ Mejoras

El circuito es un prototipo funcional y se encuentra en la versión 0.1, por lo que aún hay diversas áreas que pueden ser optimizadas:

- **Tamaño:**  
  Se puede reemplazar la placa Wemos D1 Mini por un ESP8266 E12 y cambiar todos los componentes a montaje superficial (SMD). Esto reduciría considerablemente tanto el tamaño como el costo de fabricación, aunque aumentaría la complejidad del ensamblaje.

- **Compuerta XOR en lugar del ATTiny85:**  
  Si no se requiere cambiar el firmware, una alternativa es utilizar una compuerta lógica `74HC86 (XOR)` para la detección de cambios de estado en el reed switch. Esta opción reduciría significativamente el costo y el tamaño del circuito, ya que el ATTiny85 es más caro y voluminoso en comparación con una compuerta lógica (especialmente en versión SMD).

- **Batería:**  
  Se puede añadir un circuito adicional que permita la recarga de la batería, lo que evitaría tener que desmontar el sensor para cargarla manualmente.

- **Comunicación inalámbrica (ESP-NOW):**  
  Actualmente, la comunicación se realiza vía WiFi, lo cual puede no ser ideal en entornos industriales donde no hay cobertura WiFi completa. Una solución sería utilizar ESP-NOW para crear una red mallada entre sensores. Sin embargo, esto requeriría una planificación adecuada para asegurar que los nodos puedan retransmitir los datos hasta alcanzar un punto con acceso a servidor.

- **WiFi por LoRa:**  
  Otra opción más robusta sería integrar un módulo LoRa, permitiendo una comunicación de largo alcance entre sensores y gateway. Esta solución es ideal para entornos industriales grandes, aunque implicaría un aumento en el costo del dispositivo y un rediseño del circuito.

- **Actualización OTA:**  
  Al utilizar el ATTiny85, es posible programarlo con la librería [TinySnore](https://github.com/connornishijima/TinySnore) para que despierte al ESP8266 a una hora específica del día. Esto permitiría que el ESP consulte un endpoint de actualización y, en caso necesario, se actualice automáticamente vía OTA, facilitando la gestión y mantenimiento masivo de múltiples sensores en campo.

## 📚 Referencias y recursos utilizados

- [Circuito de encendido automatico con enclavamiento](https://randomnerdtutorials.com/power-saving-latching-circuit/)
- [Programar attiny usando ide arduino](https://www.electrosoftcloud.com/circuito-basico-y-programacion-de-attiny85/)
- [TinySnore: Librería para sleep profundo en ATTiny85](https://github.com/connornishijima/TinySnore)

![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-ESP8266-blue)
![Status](https://img.shields.io/badge/status-en%20desarrollo-yellow)
