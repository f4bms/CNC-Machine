#define DEBUG_COMM 1

#if DEBUG_COMM
  #define DBG(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG(...)
#endif

#include <Arduino.h>
#include <math.h>

// ===== PINES MOTORES =====
const int xPins[4] = {14, 27, 26, 25};
const int yPins[4] = {5, 17, 16, 4};
const int zPins[4] = {23, 22, 19, 18};

// ===== PINES COMUNICACION =====
const int RX_PIN = 32;  // recibe datos desde la RPi (conectar a GPIO17 RPi)
const int TX_PIN = 33;  // manda ACK a la RPi     (conectar a GPIO27 RPi)

// ===== PROTOCOLO =====
#define BIT_DELAY_US  1000

#define CMD_GOTO      'G'
#define CMD_DRAW      'D'
#define CMD_PEN_DOWN  'P'
#define CMD_PEN_UP    'U'

// Secuencia half-step
const int halfStep[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

int stepX = 0, stepY = 0, stepZ = 0;
int posX  = 0, posY  = 0;

const int SPEED_DELAY = 1500;

// ============================================================
// Funciones base motores
// ============================================================

void stepMotor(const int pins[4], int &idx, int dir) {
  idx = (idx + (dir > 0 ? 1 : 7)) % 8;
  for (int i = 0; i < 4; i++) digitalWrite(pins[i], halfStep[idx][i]);
  delayMicroseconds(SPEED_DELAY);
}

void releaseMotor(const int pins[4]) {
  for (int i = 0; i < 4; i++) digitalWrite(pins[i], LOW);
}

void penDown() {
  for (int i = 0; i < 200; i++) stepMotor(zPins, stepZ, 1);
  releaseMotor(zPins);
}

void penUp() {
  for (int i = 0; i < 200; i++) stepMotor(zPins, stepZ, -1);
  releaseMotor(zPins);
}

void goTo(int x, int y) {
  int dx = x - posX, dy = y - posY;
  int ax = abs(dx), ay = abs(dy);
  int sx = dx > 0 ? -1 : 1, sy = dy > 0 ? -1 : 1;
  int mx = dx > 0 ?  1 : -1, my = dy > 0 ?  1 : -1;
  while (true) {
    if (posX != x) { stepMotor(xPins, stepX, sx); posX += mx; }
    if (posY != y) { stepMotor(yPins, stepY, sy); posY += my; }
    if (posX == x && posY == y) break;
  }
  releaseMotor(xPins); releaseMotor(yPins);
}

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
// Bit-banging: recepcion y envio
// ============================================================

// Recibe un byte por RX_PIN con timeout.
// Retorna el byte recibido o -1 si no llega nada en 100ms.
int bb_recv_byte() {
    unsigned long timeout = 100000;

    // esperar flanco bajante (inicio del start bit)
    while (digitalRead(RX_PIN) == HIGH) {
        if (timeout-- == 0) return -1;
        delayMicroseconds(1);
    }

    // esperar al CENTRO del start bit (mitad del periodo)
    delayMicroseconds(BIT_DELAY_US / 2);

    // verificar que sigue en LOW (confirma que es start bit real)
    if (digitalRead(RX_PIN) != LOW) return -1; // falso positivo, ignorar

    // ahora saltar un periodo completo para llegar al centro del bit 0
    delayMicroseconds(BIT_DELAY_US);

    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        if (digitalRead(RX_PIN)) byte |= (1 << i);
        delayMicroseconds(BIT_DELAY_US);
    }

    // esperar stop bit
    delayMicroseconds(BIT_DELAY_US / 2);

    return byte;
}// Manda un byte por TX_PIN
void bb_send_byte(uint8_t byte) {

  DBG("[TX] Enviando byte 0x%02X (%d '%c')\n",
      byte,
      byte,
      (byte >= 32 && byte <= 126) ? byte : '.');

  digitalWrite(TX_PIN, LOW);
  delayMicroseconds(BIT_DELAY_US);

  for (int i = 0; i < 8; i++) {

    int bit = (byte >> i) & 1;

    //DBG("[TX] Bit %d = %d\n", i, bit);

    digitalWrite(TX_PIN, bit);
    delayMicroseconds(BIT_DELAY_US);
  }

  digitalWrite(TX_PIN, HIGH);
  delayMicroseconds(BIT_DELAY_US);

  DBG("[TX] Byte enviado\n");
}

void send_ack() {
  DBG("[ACK] Enviando ACK...\n");

  bb_send_byte('A');

  DBG("[ACK] ACK enviado\n");
}

void send_nack() {
  DBG("[ACK] Enviando NACK...\n");

  bb_send_byte('N');

  DBG("[ACK] NACK enviado\n");
}

// ============================================================
// Recepcion y ejecucion de comandos
// ============================================================

void recv_and_execute() {

  int cmd = bb_recv_byte();

  if (cmd < 0)
    return;

  DBG("\n========================\n");
  DBG("[CMD] Recibido comando '%c' (0x%02X)\n", cmd, cmd);

  int x = 0;
  int y = 0;


  // G y D traen coordenadas (4 bytes extra)
  if (cmd == CMD_GOTO || cmd == CMD_DRAW) {

    DBG("[CMD] Esperando coordenadas...\n");

    bool failed = false;

    int xh = bb_recv_byte();
    if (xh < 0) {
      DBG("[ERROR] xh timeout\n");
      failed = true;
    }
      

    int xl = bb_recv_byte();
    if (xl < 0) {
      DBG("[ERROR] xl timeout\n");
      failed = true;
      
    }

    int yh = bb_recv_byte();
    if (yh < 0) {
      DBG("[ERROR] yh timeout\n");
      failed = true;
    }

    int yl = bb_recv_byte();
    if (yl < 0) {
      DBG("[ERROR] yl timeout\n");
      failed = true;
    }

    if (failed) {
      send_nack();
      return;
    }

    DBG("[CMD] xh=%d xl=%d yh=%d yl=%d\n",
        xh, xl, yh, yl);

    x = (xh << 8) | xl;
    y = (yh << 8) | yl;

    DBG("[CMD] Coordenadas decodificadas x=%d y=%d\n",
        x, y);
  }

  Serial.printf("CMD=%c x=%d y=%d\n", cmd, x, y);

  DBG("[CMD] Ejecutando...\n");
  switch (cmd) {
    case CMD_GOTO:
      DBG("[MOVE] GOTO (%d,%d)\n", x, y);
      goTo(x, y);
      DBG("[MOVE] GOTO terminado\n");
      break;

    case CMD_DRAW:
        DBG("[DRAW] DRAW (%d,%d)\n", x, y);
        drawLine(x, y);
        DBG("[DRAW] DRAW terminado\n");
        break;

    case CMD_PEN_DOWN:
        DBG("[PEN] DOWN\n");
        penDown();
        DBG("[PEN] DOWN terminado\n");
        break;

    case CMD_PEN_UP:
        DBG("[PEN] UP\n");
        penUp();
        DBG("[PEN] UP terminado\n");
        break;
    default:
      Serial.println("Comando desconocido");
      send_nack();
      return;
  }

  send_ack();  // avisa a la RPi que terminó
}

// ============================================================
// Setup y Loop
// ============================================================

void setup() {
  Serial.begin(115200);

  // pines motores
  for (int i = 0; i < 4; i++) {
    pinMode(xPins[i], OUTPUT);
    pinMode(yPins[i], OUTPUT);
    pinMode(zPins[i], OUTPUT);
  }

  // pines comunicacion
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, HIGH);   // idle alto

  penUp(); // asegurar que la pluma inicia levantada

  Serial.println("ESP listo, esperando comandos...");
}

void loop() {

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint > 1000) {
    lastPrint = millis();

    Serial.printf(
        "[STATUS] RX=%d TX=%d POS=(%d,%d)\n",
        digitalRead(RX_PIN),
        digitalRead(TX_PIN),
        posX,
        posY
    );
  }

  recv_and_execute();  // se queda escuchando indefinidamente
}