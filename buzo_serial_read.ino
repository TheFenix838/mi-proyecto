#include <BLEDevice.h> //CONFIGURACION DE BLE
#include <BLEServer.h>
#include <BLEUtils.h>
#include <EEPROM.h>
float Z1;//BRAZO DERECHO TEMPERATURA PROMEDIO MEDIDA
float Z2;//BRAZO IZQUIERDO TEMPERATURA PROMEDIO MEDIDA
float Z3;//PECHO DERECHO TEMPERATURA PROMEDIO MEDIDA
float Z4;//PECHO IZQUIERDO TEMPERATURA PROMEDIO MEDIDA
float Z5;//ESPALDA TEMPERATURA PROMEDIO MEDIDA
float z1;//BRAZO DERECHO MEDICION
float z2;//BRAZO IZQUIERDO MEDICION
float z3;;//PECHO DERECHO MEDICION
float z4;//PECHO IZQUIERDO MEDICION
float z5;//ESPALDA MEDICION
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
#define s1 5// ON/OFF
#define s2 6// TEMPERATURA 1
#define s3 7// TEMPERATURA 2
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ============================================================
// NUEVO: FUNCIONES PARA LEER Y ESCRIBIR EN EL MONITOR SERIE
// ============================================================

// NUEVO: imprime una linea con el estado de UNA zona y marca los errores
void lineaZona(const char* nombre, float medida, float deseada) {
  String estado = "";
  if (medida < 2) estado = "  <<< ERROR: sensor desconectado o en corto (la zona se esta CALENTANDO!)";
  else if (medida > 55) estado = "  <<< ERROR: lectura demasiado alta, revisa el sensor";
  else if (medida < deseada) estado = "  CALENTANDO";
  else estado = "  apagada";
  Serial.println(String(nombre) + " medida " + String(medida, 1) + " deseada " + String(deseada, 1) + estado);
}

// NUEVO: imprime el estado de las 5 zonas
void mostrarEstado() {
  Serial.println("----- ESTADO -----");
  lineaZona("Z1 BRAZO DER ", Z1, t1);
  lineaZona("Z2 BRAZO IZQ ", Z2, t2);
  lineaZona("Z3 PECHO DER ", Z3, t3);
  lineaZona("Z4 PECHO IZQ ", Z4, t4);
  lineaZona("Z5 ESPALDA   ", Z5, t5);
}

// NUEVO: interpreta mensajes como "S3=35" (cambia la zona 3 a 35) o "?" (muestra el estado)
// Sirve igual si el mensaje llega por Bluetooth o lo escribes en el Monitor Serie
void procesarComando(String t) {
  t.trim();
  if (t.length() == 0) return;

  if (t == "?") {
    mostrarEstado();
    return;
  }

  int igual = t.indexOf('=');
  if (igual < 2 || (t[0] != 'S' && t[0] != 's')) {
    Serial.println("ERROR: comando no reconocido -> " + t);
    return;
  }

  int zona = t.substring(1, igual).toInt();
  String textoValor = t.substring(igual + 1);
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
  if (valor < 20 || valor > 50) {
    Serial.println("ERROR: valor fuera de rango (20 a 50) -> " + String(valor, 1));
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

// NUEVO: lee lo que escribes en el Monitor Serie (con "Nueva linea" activado)
void leerSerial() {
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    Serial.println("Serie recibido: " + linea);
    procesarComando(linea);
  }
}

// Se ejecuta cada vez que el celular escribe algo
class MisCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *caracteristica) override {
    auto datos = caracteristica->getValue();

    String texto = "";
    for (size_t i = 0; i < datos.length(); i++) {
      texto += (char)datos[i];
    }

    Serial.print("Recibido: ");
    Serial.println(texto);
    procesarComando(texto);   // NUEVO: ahora el mensaje SI cambia t1..t5
  }
};
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  EEPROM.begin(32);
  pinMode(brder,OUTPUT);//CONFIGURACION DE LOS PINES
  pinMode(brizq,OUTPUT);
  pinMode(pechoder,OUTPUT);
  pinMode(pechoizq,OUTPUT);
  pinMode(espalda,OUTPUT);
  pinMode(s1,INPUT);
  pinMode(s2,INPUT);
  pinMode(s3,INPUT);
  delay(2000);   // NUEVO: da tiempo a que el Monitor Serie se conecte
  BLEDevice::init("ESP32_Temp");
  BLEServer *servidor = BLEDevice::createServer();
  BLEService *servicio = servidor->createService(SERVICE_UUID);
  BLECharacteristic *caracteristica = servicio->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  caracteristica->setCallbacks(new MisCallbacks());
  caracteristica->setValue("Hola");
  servicio->start();
  BLEAdvertising *anuncio = BLEDevice::getAdvertising();
  anuncio->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("BLE listo. Escribe ? para ver el estado, o S1=30 para cambiar una zona.");
  t1=EEPROM.read(1);
  t2=EEPROM.read(2);
  t3=EEPROM.read(3);
  t4=EEPROM.read(4);
  t5=EEPROM.read(5);
  // NUEVO: muestra lo que habia en la memoria y avisa si no es valido
  Serial.println("Memoria: t1=" + String(t1,0) + " t2=" + String(t2,0) + " t3=" + String(t3,0) + " t4=" + String(t4,0) + " t5=" + String(t5,0));
  if (t1<20||t1>50||t2<20||t2>50||t3<20||t3>50||t4<20||t4>50||t5<20||t5>50) {
    Serial.println("AVISO: la memoria esta vacia o tiene valores no validos. El programa los limitara a 20-50 (un 255 pasa a 50!). Envia S1=..S5=.. para fijar tus valores.");
  }
  }

void loop() {   
leerSerial();   // NUEVO: revisa si escribiste algo en el Monitor Serie
for (int r1=0;r1<15; r1++){z1=z1+analogRead(0);};//PROMEDIO DE LAS MEDICIONES EN CADA SENSOR LM 35
Z1=z1/15;
for (int r2=0;r2<15; r2++){z2=z2+analogRead(1);};
Z2=z2/15;
for (int r3=0;r3<15; r3++){z3=z3+analogRead(2);};
Z3=z3/15;
for (int r4=0;r4<15; r4++){z4=z4+analogRead(3);};
Z4=z1/15;   // <<< ERROR: debe ser z4/15 (esta usando la medicion de la zona 1)
for (int r5=0;r5<15; r5++){z5=z5+analogRead(4);};
Z5=z5/15;
Z1=Z1*330/4095 ; //FORMULAS DE TEMPERATURA (CONVERSION VOLTAJE SALIDA LM35 A VALOR DIGITAL)
Z2=Z2*330/4095 ;
Z3=Z3*330/4095 ;
Z4=Z4*330/4095 ;
Z5=Z5*330/4095 ;
z1=0;
z2=0;
z3=0;
z4=0;
z5=0;

t1=constrain(t1,20,50); //LIMITE DE PERSONALIZACION DE TEMPERATURA
t2=constrain(t2,20,50);
t3=constrain(t3,20,50);
t4=constrain(t4,20,50);
t5=constrain(t5,20,50);
EEPROM.write(1,t1); //GUARDADO DE VALOR DE TEMPERATURA EN LA MEMORIA
EEPROM.write(2,t2);
EEPROM.write(3,t3);
EEPROM.write(4,t4);
EEPROM.write(5,t5);
EEPROM.commit(); //CONFIRMACION DE GUARDADO
if(Z1<t1){digitalWrite(brder,HIGH);} else{digitalWrite(brder,LOW);};//SI LA TEMPERATURA DESEADA DE UNA ZONA ES MAYOR A LA MEDIDA SE ACTIVA, EN LA ZONA CORRESPODIENTE EL CALEFACTOR
if(Z2<t2){digitalWrite(brizq,HIGH);} else{digitalWrite(brizq,LOW);};
if(Z3<t3){digitalWrite(pechoder,HIGH);} else{digitalWrite(pechoder,LOW);};
if(Z4<t4){digitalWrite(pechoizq,HIGH);} else{digitalWrite(pechoizq,LOW);};
if(Z5<t5){digitalWrite(espalda,HIGH);} else{digitalWrite(espalda,LOW);};

// NUEVO: cada 3 segundos muestra el estado de las 5 zonas
static unsigned long tEstado = 0;
if (millis() - tEstado >= 3000) {
  tEstado = millis();
  mostrarEstado();
}

}
