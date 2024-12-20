#include <AccelStepper.h>
#include <Servo.h> ///////////pendiente gripper
#include <math.h>
#include <TimerOne.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
unsigned long previousMillis = 0; // Para almacenar el último tiempo en que se ejecutó printVal
const long interval = 1000;
#define limitSwitch1 11
#define limitSwitch2 10
#define limitSwitch3 A3
#define limitSwitch4 9

#define motorPin1 36
#define motorPin2 37
#define sensorInicial 45
#define sensorFinal 47
#define servoPin 49
AccelStepper stepper1(1, 2, 5); // (driver, STEP, DIR)
AccelStepper stepper2(1, 3, 6);
AccelStepper stepper3(1, 4, 7); //////// PENDIENTE GRIPPER
AccelStepper stepper4(1, 12, 13);
Servo gripperServo; 
Servo conveyorServo;

// Puntos en grados
int conveyorBeltPosition[4] = {10, 23, 0, 120}; // Banda transportadora en grados
const int numPoints = 8;
int points[numPoints][4] = {
  {0, 105, 0, 85}, // Punto 1 en grados
  {4, 115, 0, 85}, // Punto 2 en grados
  {-10, 130, 0, 85}, // Punto 3 en grados
  {-8, 115, 0, 85}, // Punto 4 en grados
  {0, 105, 0, 100}, // Punto 5 en grados
  {4, 115, 0, 100}, // Punto 6 en grados
  {-10, 130, 0, 100}, // Punto 7 en grados
  {-8, 115, 0, 100}  // Punto 8 en grados
};

double x = 10.0;
double y = 10.0;
double L1 = 228; // L1 = 228mm
double L2 = 136.5; // L2 = 136.5mm
double theta1, theta2, phi, z;

int stepper1Position, stepper2Position, stepper3Position, stepper4Position;

const float theta1AngleToSteps = 44.444444;
const float theta2AngleToSteps = 35.555555;
const float phiAngleToSteps = 10;
const float zDistanceToSteps = 100;

byte inputValue[5];
int k = 0;
bool paused = false;
bool startCommandReceived = false;
bool stopCommandReceived = false;
String content = "";
int data[10];

int theta1Array[100];
int theta2Array[100];
int phiArray[100];
int zArray[100];
int gripperArray[100];
int positionsCounter = 0;

volatile bool OBJVAL = false;
volatile bool toggle = false; // Declarar toggle como variable global

void setup() {
  Serial.begin(115200);

  if (tcs.begin()) {
    Serial.println("Sensor TCS34725 iniciado correctamente.");
  } else {
    Serial.println("Error al iniciar el sensor TCS34725. Verifique la conexión.");
    while (1); // Detener el programa si no se puede iniciar el sensor
  }

  pinMode(limitSwitch1, INPUT_PULLUP);
  pinMode(limitSwitch2, INPUT_PULLUP);
  pinMode(limitSwitch3, INPUT_PULLUP);
  pinMode(limitSwitch4, INPUT_PULLUP);

  // Stepper motors max speed
  stepper1.setMaxSpeed(4000);
  stepper1.setAcceleration(2000);
  stepper2.setMaxSpeed(4000);
  stepper2.setAcceleration(2000);
  stepper3.setMaxSpeed(4000);
  stepper3.setAcceleration(2000);
  stepper4.setMaxSpeed(4000);
  stepper4.setAcceleration(2000);
  
  gripperServo.attach(53, 600, 2500);
  data[6] = 100;
  gripperServo.write(data[6]);
  conveyorServo.write(180); // Posición inicial del servo


  delay(1000);
  data[5] = 100;
  homing();


  digitalWrite(motorPin1, HIGH);
  digitalWrite(motorPin2, LOW);
}

void loop() {
   static unsigned long lastSensorReadTime = 0;
  const unsigned long sensorReadInterval = 1000;
  motorAdelante();
  // Esperar el comando "START" para iniciar
  if (!startCommandReceived || stopCommandReceived) {
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command == "START") {
        startCommandReceived = true;
        stopCommandReceived = false;
        Serial.println("SISTEMA INICIADO");
      } else if (command == "STOP") {
        startCommandReceived = false;
        stopCommandReceived = true;
        Serial.println("SISTEMA DETENIDO");
      }
    }
    return; // No hacer nada hasta recibir el comando "START"
  }

  // Leer valores del sensor RGB periódicamente
  if (millis() - lastSensorReadTime >= sensorReadInterval) {
    lastSensorReadTime = millis();
    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    // Normalizar los valores RGB
    float sum = r + g + b;
    float red = r / sum;
    float green = g / sum;

    // Clasificar el color
    if (red > green && red > 0.45) {
      Serial.println("DEF");
      OBJVAL = false;
      conveyorServo.write(125); // Activar el servo para el cubo defectuoso
    } else if (green > red && green > 0.45) {
      Serial.println("VAL");
      conveyorServo.write(180); // Activar el servo para el cubo defectuoso
      OBJVAL = true;
    }
  }

  // Resto del código del bucle
  for (int i = 0; i < numPoints; i++) {
    // Esperar a que OBJVAL sea true para procesar el cubo
    while (!OBJVAL) {
      // Leer valores del sensor RGB periódicamente
      if (millis() - lastSensorReadTime >= sensorReadInterval) {
        lastSensorReadTime = millis();
        uint16_t r, g, b, c;
        tcs.getRawData(&r, &g, &b, &c);

        // Normalizar los valores RGB
        float sum = r + g + b;
        float red = r / sum;
        float green = g / sum;

        // Clasificar el color
        if (red > green && red > 0.45) {
          Serial.println("DEF");
          OBJVAL = false;
          conveyorServo.write(180); // Activar el servo para el cubo defectuoso
        } else if (green > red && green > 0.45) {
          Serial.println("VAL");
          OBJVAL = true;
          conveyorServo.write(125); // Activar el servo para el cubo defectuoso
        }
      }
      delay(100); // Espera un poco antes de verificar nuevamente
    }

    // Mover a la banda transportadora
    moveToPosition(conveyorBeltPosition);
    delay(1000); // Espera un segundo antes de tomar el cubo

    // Simular la acción de tomar el cubo
    gripperServo.write(180); // Cerrar el gripper
    delay(500); // Espera medio segundo para asegurar que el cubo está tomado
    moveToSafeZ();
    delay(100);
    // Mover al punto de destino
    moveToPosition(points[i]);
    delay(1000); 
    gripperServo.write(90); // Abrir el gripper
    delay(500); 
    Serial.print("P");
    Serial.println(i + 1);

    // Cambiar la bandera a false después de procesar el cubo
    OBJVAL = false;
    moveToSafeZ();
    delay(100);
    // Verificar si se debe pausar
    while (paused) {
      if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command.equals("CONTINUAR")) {
          paused = false;
          Serial.println("CONTINUAO");
        }
      }
    }

    // Verificar si se debe pausar después de cada punto
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command.equals("PAUSAR")) {
        paused = true;
        Serial.println("PAUSAO");
      }
    }
  }

  // Pausar al llegar al último punto y esperar el comando "REINICIAR"
  while (true) {
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command.equals("REINICIAR")) {
        break;
      }
    }
  }
}

void moveToPosition(int point[4]) {
  stepper1.moveTo(point[0] * theta1AngleToSteps);
  stepper2.moveTo(point[1] * theta2AngleToSteps);
  stepper3.moveTo(point[2] * phiAngleToSteps);
  stepper4.moveTo(point[3] * zDistanceToSteps);

  while (stepper1.distanceToGo() != 0 || 
         stepper2.distanceToGo() != 0 || 
         stepper3.distanceToGo() != 0 || 
         stepper4.distanceToGo() != 0) {
    stepper1.run();
    stepper2.run();
    stepper3.run();
    stepper4.run();
  }
}

void serialFlush() {
  while (Serial.available() > 0) {
    Serial.read();
  }
}

void moveToSafeZ() {
  stepper4.moveTo(165 * zDistanceToSteps);
  while (stepper4.distanceToGo() != 0) {
    stepper4.run();
  }
}

void homing() {
  // Homing Stepper4
  while (digitalRead(limitSwitch4) != 0) {
    stepper4.setSpeed(1500);
    stepper4.runSpeed();
    stepper4.setCurrentPosition(17000); // When limit switch pressed set position to 0 steps
  }
  delay(20);
  stepper4.moveTo(16500);
  while (stepper4.currentPosition() !=16500) {
    stepper4.run();
  }

  stepper3.moveTo(0);
  while (stepper3.currentPosition() != 0) {
    stepper3.run();
  }

  // Homing Stepper2
  while (digitalRead(limitSwitch2) != 0) {
    stepper2.setSpeed(-1300);
    stepper2.runSpeed();
    stepper2.setCurrentPosition(-5420); // When limit switch pressed set position to -5440 steps
  }
  delay(20);

  stepper2.moveTo(0);
  while (stepper2.currentPosition() != 0) {
    stepper2.run();
  }

  // Homing Stepper1
  while (digitalRead(limitSwitch1) != 0) {
    stepper1.setSpeed(-1200);
    stepper1.runSpeed();
    stepper1.setCurrentPosition(-3955); // When limit switch pressed set position to 0 steps
  }
  delay(20);
  stepper1.moveTo(0);
  while (stepper1.currentPosition() != 0) {
    stepper1.run();
  }
}




void motorAdelante() {
  digitalWrite(motorPin1, HIGH);
  digitalWrite(motorPin2, LOW);
}

void motorParar() {
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
}