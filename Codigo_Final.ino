#include <BLEDevice.h> //CONFIGURACION DE BLE
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <EEPROM.h>
#include "esp_sleep.h"   //PARA EL SUENO PROFUNDO
#include "driver/gpio.h"

float Z1;//BRAZO DERECHO TEMPERATURA PROMEDIO MEDIDA
float Z2;//BRAZO IZQUIERDO TEMPERATURA PROMEDIO MEDIDA
float Z3;//PECHO DERECHO TEMPERATURA PROMEDIO MEDIDA
float Z4;//PECHO IZQUIERDO TEMPERATURA PROMEDIO MEDIDA
float Z5;//ESPALDA TEMPERATURA PROMEDIO MEDIDA
float t1;//BRAZO DERECHO TEMPERATURA DESEADA
float t2;//BRAZO IZQUIERDO TEMPERATURA DESEADA
float t3;//PECHO DERECHO TEMPERATURA DESEADA
float t4;//PECHO IZQUIERDO TEMPERATURA DESEADA
float t5;//ESPALDA TEMPERATURA DESEADA
#define brder 8//BRAZO DERECHO
#define brizq 9 //BRAZO IZQUIERDO
#define pechoder 10//PECHO DERECHO
#define pechoizq 20//PECHO IZQUIERDO
#define espalda 21//ESPALDA
#define s1 5// ON/OFF (MANTENER 1 SEGUNDO = APAGAR)
#define s2 6// TEMPERATURA FIJA 1
#define s3 7// TEMPERATURA FIJA 2
#define PRESIONADO HIGH   //NIVEL DEL PIN CUANDO PRESIONAS EL BOTON (SI TU BOTON CONECTA A GND, PON LOW)
#define NIVEL_DESPERTAR ESP_GPIO_WAKEUP_GPIO_HIGH   //DEBE COINCIDIR CON LO DE ARRIBA (LOW -> ESP_GPIO_WAKEUP_GPIO_LOW)
#define TEMPMIN 20      //TEMPERATURA MINIMA QUE SE PUEDE PEDIR
#define TEMPMAX 50      //TEMPERATURA MAXIMA QUE SE PUEDE PEDIR
#define TEMP_BOTON2 30
//TEMPERATURA FIJA DEL BOTON 2
#define TEMP_BOTON3 45  //TEMPERATURA FIJA DEL BOTON 3
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCaracteristica = nullptr;
volatile bool celularConectado = false;

// ============================================================
// FUNCIONES
// ============================================================

// DICE SI UNA ZONA DEBE CALENTAR (SI LA LECTURA ES IMPOSIBLE, NUNCA CALIENTA)
bool calienta(float medida, float deseada) {
  if (medida < 2 || medida > 55) return false;   //SENSOR DESCONECTADO, EN CORTO O MUY CALIENTE
  return medida < deseada;                        //SI LA MEDIDA ES MENOR A LA DESEADA, CALIENTA
}

// PROMEDIO DE 15 MEDICIONES DE UN LM35 (DEVUELVE GRADOS)
float leerLM35(int pin) {
  float suma = 0;
  for (int r = 0; r < 15; r++) {
    suma = suma + analogReadMilliVolts(pin);   //MILIVOLTIOS YA CALIBRADOS
    delayMicroseconds(200);
  }
  return suma / 15 / 10;                          //EL LM35 DA 10 mV POR CADA GRADO
}

// ESCRIBE EN EL MONITOR SERIE UNA LINEA POR ZONA Y MARCA LOS ERRORES
void lineaZona(String nombre, float medida, float deseada) {
  String estado = "  apagada";
  if (medida < 2) estado = "  <<< ERROR: SENSOR DESCONECTADO O EN CORTO (zona apagada)";
  else if (medida > 55) estado = "  <<< ERROR: LECTURA MUY ALTA (zona apagada)";
  else if (calienta(medida, deseada)) estado = "  CALENTANDO";
  Serial.println(nombre + " medida " + String(medida, 1) + " deseada " + String(deseada, 1) + estado);
}

void mostrarEstado() {
  Serial.println("----- ESTADO -----");
  lineaZona("Z1 BRAZO DER", Z1, t1);
  lineaZona("Z2 BRAZO IZQ", Z2, t2);
  lineaZona("Z3 PECHO DER", Z3, t3);
  lineaZona("Z4 PECHO IZQ", Z4, t4);
  lineaZona("Z5 ESPALDA  ", Z5, t5);
  Serial.println(celularConectado ? "CELULAR: conectado" : "CELULAR: no conectado");
}

// INTERPRETA MENSAJES COMO "S3=35" (ZONA 3 A 35 GRADOS) O "?" (MUESTRA EL ESTADO)
// FUNCIONA IGUAL SI EL MENSAJE LLEGA POR BLUETOOTH O LO ESCRIBES EN EL MONITOR SERIE
void procesarComando(String orden) {
  orden.trim();
  if (orden.length() == 0) return;

  if (orden == "?") {
    mostrarEstado();
    return;
  }

  int igual = orden.indexOf('=');
  if (igual < 2 || (orden[0] != 'S' && orden[0] != 's')) {
    Serial.println("ERROR: comando no reconocido -> " + orden);
    return;
  }

  int zona = orden.substring(1, igual).toInt();
  String textoValor = orden.substring(igual + 1);
  textoValor.trim();
  float valor = textoValor.toFloat();

  if (zona < 1 || zona > 5) {
    Serial.println("ERROR: zona fuera de rango (1 a 5) -> " + String(zona));
    return;
  }
  if (textoValor.length() == 0 || (valor == 0 && textoValor[0] != '0')) {
    Serial.println("ERROR: el valor no es un numero -> " + textoValor);
    return;
  }
  if (valor < TEMPMIN || valor > TEMPMAX) {
    Serial.println("ERROR: valor fuera de rango (" + String(TEMPMIN) + " a " + String(TEMPMAX) + ") -> " + String(valor, 1));
    return;
  }

  switch (zona) {
    case 1: t1 = valor; break;
    case 2: t2 = valor; break;
    case 3: t3 = valor; break;
    case 4: t4 = valor; break;
    case 5: t5 = valor; break;
  }
  Serial.println("OK: zona " + String(zona) + " deseada " + String(valor, 1));
}

// LEE LO QUE ESCRIBES EN EL MONITOR SERIE (ACTIVA "NUEVA LINEA" ABAJO)
void leerSerial() {
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    Serial.println("Serie recibido: " + linea);
    procesarComando(linea);
  }
}

// ARMA EL TEXTO DE UNA ZONA PARA EL CELULAR. EJEMPLO: "Z1=24.3,35.0,1"
// (ZONA = MEDIDA , DESEADA , 1 SI ESTA CALENTANDO O 0 SI NO)
String datosZona(int zona) {
  float m = 0, d = 0;
  switch (zona) {
    case 1: m = Z1; d = t1; break;
    case 2: m = Z2; d = t2; break;
    case 3: m = Z3; d = t3; break;
    case 4: m = Z4; d = t4; break;
    case 5: m = Z5; d = t5; break;
  }
  int h = calienta(m, d) ? 1 : 0;
  return "Z" + String(zona) + "=" + String(m, 1) + "," + String(d, 1) + "," + String(h);
}

// MANDA AL CELULAR UNA ZONA CADA VEZ QUE SE LLAMA (ASI LAS 5 SE ACTUALIZAN EN 2 SEGUNDOS)
void enviarAlCelular() {
  static int zonaEnvio = 1;
  if (!celularConectado) return;
  String mensaje = datosZona(zonaEnvio);
  pCaracteristica->setValue(mensaje.c_str());
  pCaracteristica->notify();
  Serial.println("ENVIADO AL CELULAR: " + mensaje);   //BORRA ESTA LINEA SI TE MOLESTA EN EL MONITOR
  zonaEnvio++;
  if (zonaEnvio > 5) zonaEnvio = 1;
}

// PONE LAS 5 ZONAS A LA MISMA TEMPERATURA
void ponerTodas(float valor) {
  t1 = valor;
  t2 = valor;
  t3 = valor;
  t4 = valor;
  t5 = valor;
}

// SUENO PROFUNDO: APAGA LOS CALEFACTORES Y SE DUERME. EL BOTON ON/OFF LO DESPIERTA
void dormir() {
  Serial.println("BOTON ON/OFF 1 SEGUNDO: APAGANDO (SUENO PROFUNDO). PRESIONA ON/OFF PARA ENCENDER");
  digitalWrite(brder, LOW);
  digitalWrite(brizq, LOW);
  digitalWrite(pechoder, LOW);
  digitalWrite(pechoizq, LOW);
  digitalWrite(espalda, LOW);
  gpio_hold_en((gpio_num_t)brder);        //MANTIENE LAS SALIDAS EN LOW DURANTE EL SUENO
  gpio_hold_en((gpio_num_t)brizq);
  gpio_hold_en((gpio_num_t)pechoder);
  gpio_hold_en((gpio_num_t)pechoizq);
  gpio_hold_en((gpio_num_t)espalda);
  gpio_deep_sleep_hold_en();

  unsigned long inicio = millis();        //ESPERA A QUE SUELTES EL BOTON (SI NO, DESPERTARIA AL INSTANTE)
  while (digitalRead(s1) == PRESIONADO && millis() - inicio < 3000) {
    delay(10);
  }

  esp_deep_sleep_enable_gpio_wakeup(1ULL << s1, NIVEL_DESPERTAR);
  Serial.flush();
  esp_deep_sleep_start();
}

// REVISA LOS 3 BOTONES (SE LLAMA CADA 30 ms, ESO TAMBIEN EVITA EL REBOTE)
void leerBotones() {
  static bool s2Antes = false;
  static bool s3Antes = false;
  static bool soltado = false;              //EL BOTON ON/OFF SOLO CUENTA DESPUES DE HABERLO SOLTADO UNA VEZ
  static unsigned long inicioPulsado = 0;

  bool b1 = (digitalRead(s1) == PRESIONADO);
  bool b2 = (digitalRead(s2) == PRESIONADO);
  bool b3 = (digitalRead(s3) == PRESIONADO);

  // BOTON ON/OFF: MANTENERLO 1 SEGUNDO APAGA
  if (!b1) {
    soltado = true;
    inicioPulsado = 0;
  } else if (soltado) {
    if (inicioPulsado == 0) inicioPulsado = millis();
    if (millis() - inicioPulsado >= 1000) dormir();
  }

  // BOTON 2: TODAS LAS ZONAS A TEMP_BOTON2
  if (b2 && !s2Antes) {
    ponerTodas(TEMP_BOTON2);
    Serial.println("BOTON 2: todas las zonas a " + String(TEMP_BOTON2));
  }
  s2Antes = b2;

  // BOTON 3: TODAS LAS ZONAS A TEMP_BOTON3
  if (b3 && !s3Antes) {
    ponerTodas(TEMP_BOTON3);
    Serial.println("BOTON 3: todas las zonas a " + String(TEMP_BOTON3));
  }
  s3Antes = b3;
}

// SI EL VALOR GUARDADO EN LA MEMORIA NO ES VALIDO (MEMORIA VACIA) USA TEMPMIN
float memoriaValida(int numero, float valor) {
  if (valor < TEMPMIN || valor > TEMPMAX) {
    Serial.println("AVISO: zona " + String(numero) + " sin valor valido en memoria (" + String(valor, 0) + "), se usa " + String(TEMPMIN));
    return TEMPMIN;
  }
  return valor;
}

// ============================================================
// BLE
// ============================================================

// SE EJECUTA CUANDO EL CELULAR SE CONECTA O SE DESCONECTA
class MisServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *servidor) override {
    celularConectado = true;
    Serial.println("CELULAR CONECTADO");
  }
  void onDisconnect(BLEServer *servidor) override {
    celularConectado = false;
    Serial.println("CELULAR DESCONECTADO");
    BLEDevice::startAdvertising();   //VUELVE A ANUNCIARSE PARA PODER RECONECTAR
  }
};

// SE EJECUTA CADA VEZ QUE EL CELULAR ESCRIBE ALGO
class MisCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *caracteristica) override {
    auto datos = caracteristica->getValue();

    String texto = "";
    for (size_t i = 0; i < datos.length(); i++) {
      texto += (char)datos[i];
    }

    Serial.print("Recibido: ");
    Serial.println(texto);
    procesarComando(texto);   //AHORA EL MENSAJE SI CAMBIA t1..t5
  }
};

// ============================================================
// SETUP Y LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   //NO ESPERAR SI NO HAY PC CONECTADA
  gpio_deep_sleep_hold_dis();               //LIBERA LOS PINES SI VIENE DE SUENO PROFUNDO
  gpio_hold_dis((gpio_num_t)brder);
  gpio_hold_dis((gpio_num_t)brizq);
  gpio_hold_dis((gpio_num_t)pechoder);
  gpio_hold_dis((gpio_num_t)pechoizq);
  gpio_hold_dis((gpio_num_t)espalda);
  analogReadResolution(12);
  EEPROM.begin(32);
  pinMode(brder,OUTPUT);//CONFIGURACION DE LOS PINES
  pinMode(brizq,OUTPUT);
  pinMode(pechoder,OUTPUT);
  pinMode(pechoizq,OUTPUT);
  pinMode(espalda,OUTPUT);
  digitalWrite(brder,LOW);//TODAS ARRANCAN APAGADAS
  digitalWrite(brizq,LOW);
  digitalWrite(pechoder,LOW);
  digitalWrite(pechoizq,LOW);
  digitalWrite(espalda,LOW);
  pinMode(s1,INPUT);
  pinMode(s2,INPUT);
  pinMode(s3,INPUT);
  delay(2000);   // NUEVO: da tiempo a que el Monitor Serie se conecte
  BLEDevice::init("ESP32_Temp");
  BLEServer *servidor = BLEDevice::createServer();
  servidor->setCallbacks(new MisServerCallbacks());
  BLEService *servicio = servidor->createService(SERVICE_UUID);
  pCaracteristica = servicio->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pCaracteristica->addDescriptor(new BLE2902());   //NECESARIO PARA PODER ENVIAR AL CELULAR
  pCaracteristica->setCallbacks(new MisCallbacks());
  pCaracteristica->setValue("Hola");
  servicio->start();
  BLEAdvertising *anuncio = BLEDevice::getAdvertising();
  anuncio->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("BLE listo. Escribe ? para ver el estado, o S1=30 para cambiar una zona.");
  t1=memoriaValida(1,EEPROM.read(1));
  t2=memoriaValida(2,EEPROM.read(2));
  t3=memoriaValida(3,EEPROM.read(3));
  t4=memoriaValida(4,EEPROM.read(4));
  t5=memoriaValida(5,EEPROM.read(5));
  Serial.println("Memoria: t1=" + String(t1,0) + " t2=" + String(t2,0) + " t3=" + String(t3,0) + " t4=" + String(t4,0) + " t5=" + String(t5,0));
  }

void loop() {   
leerSerial();   //REVISA SI ESCRIBISTE ALGO EN EL MONITOR SERIE
Z1=leerLM35(0);//PROMEDIO DE LAS MEDICIONES EN CADA SENSOR LM 35
Z2=leerLM35(1);
Z3=leerLM35(2);
Z4=leerLM35(3);
Z5=leerLM35(4);

t1=constrain(t1,TEMPMIN,TEMPMAX); //LIMITE DE PERSONALIZACION DE TEMPERATURA
t2=constrain(t2,TEMPMIN,TEMPMAX);
t3=constrain(t3,TEMPMIN,TEMPMAX);
t4=constrain(t4,TEMPMIN,TEMPMAX);
t5=constrain(t5,TEMPMIN,TEMPMAX);
EEPROM.write(1,t1); //GUARDADO DE VALOR DE TEMPERATURA EN LA MEMORIA (COMO NUMERO ENTERO)
EEPROM.write(2,t2);
EEPROM.write(3,t3);
EEPROM.write(4,t4);
EEPROM.write(5,t5);
EEPROM.commit(); //CONFIRMACION DE GUARDADO
if(calienta(Z1,t1)){digitalWrite(brder,HIGH);} else{digitalWrite(brder,LOW);};//SI LA TEMPERATURA DESEADA DE UNA ZONA ES MAYOR A LA MEDIDA SE ACTIVA, EN LA ZONA CORRESPODIENTE EL CALEFACTOR
if(calienta(Z2,t2)){digitalWrite(brizq,HIGH);} else{digitalWrite(brizq,LOW);};
if(calienta(Z3,t3)){digitalWrite(pechoder,HIGH);} else{digitalWrite(pechoder,LOW);};
if(calienta(Z4,t4)){digitalWrite(pechoizq,HIGH);} else{digitalWrite(pechoizq,LOW);};
if(calienta(Z5,t5)){digitalWrite(espalda,HIGH);} else{digitalWrite(espalda,LOW);};

// CADA CIERTO TIEMPO: BOTONES, MONITOR SERIE Y CELULAR
static unsigned long tBotones = 0;
static unsigned long tEstado = 0;
static unsigned long tEnvio = 0;
if (millis() - tBotones >= 30) {      //BOTONES CADA 30 ms
  tBotones = millis();
  leerBotones();
}
if (millis() - tEstado >= 3000) {     //ESTADO EN EL MONITOR SERIE CADA 3 s
  tEstado = millis();
  mostrarEstado();
}
if (millis() - tEnvio >= 400) {       //UNA ZONA AL CELULAR CADA 400 ms (LAS 5 EN 2 s)
  tEnvio = millis();
  enviarAlCelular();
}

}
