// ============================================================
//  CNC Plotter
// ============================================================

#include <Arduino.h>
#include <math.h>

// ===== PINES =====
const int xPins[4] = {14, 27, 26, 25};
const int yPins[4] = {5, 17, 16, 4};
const int zPins[4] = {23, 22, 19, 18};

// Secuencia half-step
const int halfStep[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

int stepX = 0, stepY = 0, stepZ = 0;
int posX  = 0, posY  = 0;

const int SPEED_DELAY = 1500; // microsegundos entre pasos

// ============================================================
//  Funciones base
// ============================================================

// Avanza un paso en la dirección dada (dir > 0: adelante, dir < 0: atrás)
void stepMotor(const int pins[4], int &idx, int dir) {
  idx = (idx + (dir > 0 ? 1 : 7)) % 8; // avanza o retrocede en la secuencia
  for (int i = 0; i < 4; i++) digitalWrite(pins[i], halfStep[idx][i]);
  delayMicroseconds(SPEED_DELAY);
}

// Desactiva el motor para ahorrar energía y evitar calentamiento
void releaseMotor(const int pins[4]) {
  for (int i = 0; i < 4; i++) digitalWrite(pins[i], LOW);
}

// Baja el lápiz (motor Z hacia abajo)
void penDown() {
  for (int i = 0; i < 200; i++) stepMotor(zPins, stepZ, 1);
  releaseMotor(zPins);
}
// Sube el lápiz (motor Z hacia arriba)
void penUp() {
  for (int i = 0; i < 200; i++) stepMotor(zPins, stepZ, -1);
  releaseMotor(zPins);
}

// Desplazamiento a posición absoluta (lápiz arriba)
void goTo(int x, int y) {
  int dx = x - posX, dy = y - posY;
  // ax/ay: distancia absoluta a recorrer en cada eje
  int ax = abs(dx), ay = abs(dy);
  // sx/sy: dirección de movimiento (-1 o 1), mx/my: paso a dar (1 o -1)
  int sx = dx > 0 ? -1 : 1, sy = dy > 0 ? -1 : 1;
  int mx = dx > 0 ?  1 : -1, my = dy > 0 ?  1 : -1;
  while (true) {
    // Avanza un paso en X e Y según corresponda, hasta llegar a destino
    if (posX != x) { stepMotor(xPins, stepX, sx); posX += mx; }
    if (posY != y) { stepMotor(yPins, stepY, sy); posY += my; }
    if (posX == x && posY == y) break;
  }
  releaseMotor(xPins); releaseMotor(yPins);
}

// Traza línea recta a posición absoluta (lápiz abajo)
void drawLine(int x1, int y1) {
  int dx = x1 - posX, dy = y1 - posY;
  int ax = abs(dx), ay = abs(dy);
  int sx = dx > 0 ? -1 : 1, sy = dy > 0 ? -1 : 1;
  int mx = dx > 0 ?  1 : -1, my = dy > 0 ?  1 : -1;
  while (true) {
    if (posX != x1) { stepMotor(xPins, stepX, sx); posX += mx; }
    if (posY != y1) { stepMotor(yPins, stepY, sy); posY += my; }
    if (posX == x1 && posY == y1) break;
  }
  releaseMotor(xPins); releaseMotor(yPins);
}

// ============================================================
// Funciones de prueba
// ============================================================
void test_lissajous_denso(int cx, int cy, int ampX, int ampY) {
  // a y b primos entre sí → el trazo cierra exactamente
  // y cubre el espacio de forma uniforme sin repetir camino
  const int   a     = 7;
  const int   b     = 13;
  const float delta = M_PI / 4.0f;

  // Necesitamos lcm(a,b) períodos completos para cerrar la curva.
  // Con a=7, b=13 → lcm=91 → usamos 91*100 = 9100 pasos
  const int pasos = 9100;
  bool primero = true;

  for (int i = 0; i <= pasos; i++) {
    float t  = 2.0f * M_PI * i / pasos;
    int   nx = cx + (int)(ampX * sin(a * t + delta));
    int   ny = cy + (int)(ampY * sin(b * t));

    if (primero) { goTo(nx, ny); penDown(); primero = false; }
    else          drawLine(nx, ny);
  }
}



// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) {
    pinMode(xPins[i], OUTPUT);
    pinMode(yPins[i], OUTPUT);
    pinMode(zPins[i], OUTPUT);
  }

  delay(2000); // espera 2 segundos para preparar el plotter
  // centro del área 10000x10000, limitacion fisica del plotter
  int cx = 5000, cy = 5000;  


  test_lissajous_denso(cx, cy, 5000, 5000);


  penUp();  // levanta el lápiz al finalizar
  goTo(0, 0);  // vuelve al origen
  Serial.println("Dibujo terminado.");
}

void loop() {}