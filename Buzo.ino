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
  Serial.println("BLE listo, escribe algo desde el celular");
  t1=EEPROM.read(1);
  t2=EEPROM.read(2);
  t3=EEPROM.read(3);
  t4=EEPROM.read(4);
  t5=EEPROM.read(5);
  }

void loop() {   
for (int r1=0;r1<15; r1++){z1=z1+analogRead(0);};//PROMEDIO DE LAS MEDICIONES EN CADA SENSOR LM 35
Z1=z1/15;
for (int r2=0;r2<15; r2++){z2=z2+analogRead(1);};
Z2=z2/15;
for (int r3=0;r3<15; r3++){z3=z3+analogRead(2);};
Z3=z3/15;
for (int r4=0;r4<15; r4++){z4=z4+analogRead(3);};
Z4=z1/15;
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


}
