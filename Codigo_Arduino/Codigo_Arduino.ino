#include <SPI.h>
#include <Ethernet.h>
#include <TimerOne.h> 
#include <StringSplitter.h>
#include <Arduino.h>
#include <math.h>

// ---------------- CONFIGURACIÓN DE PINES ----------------
const int PIN_STEP = 2;
const int PIN_DIR  = 3;
const int PIN_EN   = 5; 
// Usamos Pin 6 para el calentador (Evita conflicto con Timer1)
const int PWM_pin  = 6; 
 
// ---------------- VARIABLES MOTOR Y EXTRUSOR ----------------
volatile bool estadoStep = LOW; 
bool motorActivo = false;
float frecuencia = 0;

// Constantes físicas (Ajusta según tu extrusor)
const float D_gear = 11.0;             
const int pasos_por_rev = 200;        
const int microstepping = 16;         
const float STEPS_PER_MM = (pasos_por_rev * microstepping) / (PI * D_gear);

// Velocidad para extrusión manual (botones HMI)
float manual_freq = 400.0f; 

// --------- CONFIGURACIÓN DE TIEMPOS Y SEGURIDAD ----------------
const unsigned long TIEMPO_MAX_RECUPERACION = 120000; // 120s antes de ERROR FATAL

// FILTRO ANTI-RUIDO (Evita paradas por picos eléctricos)
const unsigned long TIEMPO_FILTRO_RUIDO = 30000; // 30s de tolerancia a ruido
unsigned long inicioFalloRuido = 0; 

// Variables de estado de error
unsigned long inicioFalloTemp = 0;           
bool errorFatal = false;       

// --------- CONTROL DE TEMPERATURA -----------------------------
float tempTolerance = 10.0f;          // Tolerancia +/- 10 grados
unsigned long stableTime_ms = 10000;  // 10 Segundos estable antes de "RDY"
unsigned long tempInRangeSince = 0;
bool tempReady = false;
float currentTemperature = 0.0f; 

// Termistor (100k NTC 3950)
const int TERM_PIN = A2;
const float R1 = 100000.0f; 
const float c1 = 0.7146522542e-03;
const float c2 = 2.177331303e-04;
const float c3 = 0.8690570296e-07;

// ---------------- PID PARAMS (ACTUALIZADOS AL CÓDIGO FUNCIONAL) -----------------
float default_print_temp = 225.0f; 
float set_temperature = 0.0f;      
float temp_offset = 0.0f; 
unsigned long sampleTime_ms = 250; 

// PID (Valores tomados del código que funciona)
float Kp = 90.0f;   
float Ki = 1.0f;   
float Kd = 40.0f;   

float integralTerm = 0.0f;
float previousError = 0.0f;
unsigned long lastTime = 0;
float integralMin = -200.0f, integralMax = 200.0f; 

// ---------------- RED Y TELEMETRÍA ----------------
byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };
byte IP[]       = { 200, 126, 19, 237 };
byte GATEWAY[]  = { 200, 126, 19, 193 };
byte SUBNET[]   = { 255, 255, 255, 192 };
EthernetServer server(4012);
EthernetClient client; 

bool enviarDatosContinuos = false;
unsigned long lastReportTime = 0;
const long reportInterval = 500; 
bool msgReadySent = false;

// --- Prototipos ---
float readThermistorC();
float convertirMyMM(String datoStr);
float calcularFrecuenciaExtrusor(float v_tcp_mm_s);
void stepPulse();
void procesarComando(String cmd);
String formatearNumero(float val); 
void enviarPaqueteDatos(int estadoCode);
void apagarSistemaFatal();

void setup() {
  Serial.begin(115200);

  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_EN, OUTPUT);
  pinMode(PWM_pin, OUTPUT);
  pinMode(4, OUTPUT); 

  // NOTA: Se eliminó la configuración manual de TCCR1B para no interferir
  // con el Timer1 (Motor) ni asumir erróneamente control sobre el Pin 6 (Timer0).
  lastTime = millis();
  
  digitalWrite(PIN_EN, LOW);   
  digitalWrite(PIN_DIR, HIGH); 
  digitalWrite(4, HIGH); 
  analogWrite(PWM_pin, 255); // Apagado inicial (Lógica invertida)

  Ethernet.begin(MAC, IP, GATEWAY, SUBNET);
  server.begin();

  Timer1.initialize(1000000); 
  Timer1.attachInterrupt(stepPulse); 
  Timer1.stop(); 
  
  set_temperature = 0.0f; 
  Serial.println("SISTEMA COMPLETO V9. Pin PWM: 6");
}

void loop() {
  if (errorFatal) return; // Sistema bloqueado

  // 1. GESTIÓN DE CLIENTE
  EthernetClient newClient = server.available();
  if (newClient) {
    if (!client || !client.connected()) {
      client = newClient;
      Serial.println("Cliente Conectado");
    }
  }

  // 2. LECTURA DE COMANDOS
  if (client && client.connected() && client.available()) {
    String mensaje = client.readStringUntil('\n');
    mensaje.trim();
    if (mensaje.length() > 0) {
      Serial.println("Mensaje Recibido");
      Serial.println(mensaje);
      procesarComando(mensaje);
    }
  }

  // 3. CONTROL PID Y SEGURIDAD
  unsigned long now = millis();
  unsigned long dt_ms = now - lastTime;
  
  if (dt_ms < sampleTime_ms) {
    return;
  }
  
  float dt = dt_ms / 1000.0f;
  lastTime = now;

  currentTemperature = readThermistorC();
  float error = set_temperature - currentTemperature;

  // ----- Proporcional -----
  float P = Kp * error;

  // ----- Integral con anti-windup -----
  integralTerm += error * dt;
  if (integralTerm > integralMax) integralTerm = integralMax;
  if (integralTerm < integralMin) integralTerm = integralMin;
  float I = Ki * integralTerm;

  // ----- Derivada -----
  float derivative = (error - previousError) / dt;
  float D = Kd * derivative;

  float output = P + I + D;

  // Limitar PWM
  if (output < 0.0f) output = 0.0f;
  if (output > 255.0f) output = 255.0f;

  // Escritura al pin PWM (Lógica invertida para MOSFET canal P o relés activos en bajo)
  // Si usas un MOSFET canal N normal, cambia esto a: analogWrite(PWM_pin, (int)output);
  analogWrite(PWM_pin, (int)(255 - output));

  previousError = error;

    
// --- LÓGICA DE SEGURIDAD ---
  if (set_temperature> 215.0f) {
    bool enRango = abs(set_temperature - currentTemperature) <= tempTolerance;
    if (enRango) {
      //Serial.println("AQUI");
      inicioFalloRuido = 0;
      tempReady = true;
      inicioFalloTemp = 0;
      if (!msgReadySent && client && client.connected()) {
          client.println("CALIENTE"); 
          Serial.println("RDY enviado");
          msgReadySent = true;   
          }
      } else {
        tempInRangeSince = 0;
        // FILTRO DE RUIDO
        if (tempReady) {
          if (inicioFalloRuido == 0) inicioFalloRuido = now;
          if (now - inicioFalloRuido > TIEMPO_FILTRO_RUIDO) {
            tempReady = false; // Se confirma pérdida de temp
            msgReadySent = false;
           Serial.println("ALERTA: Temp perdida > 30s");
           }
        } else {
          tempReady = false;
          msgReadySent = false;
          }
      }
       // ACCIÓN SI NO ESTÁ LISTO (PAUSA/ERROR)
        if (!tempReady) {
            // Pausar motor si estaba andando
            if (motorActivo) {
               Timer1.stop();
               motorActivo = false;
               if(client && client.connected()) client.println("PAUSED TEMP LOW");
            }

            // Muerte súbita (50s sin recuperar)
            if (inicioFalloTemp == 0) inicioFalloTemp = now;
            if (now - inicioFalloTemp > TIEMPO_MAX_RECUPERACION) {
                errorFatal = true;
                apagarSistemaFatal(); 
                if(client && client.connected()) client.println("ERROR FATAL TEMP");
                Serial.println("ERROR FATAL");
            }
        }

      } else {
        // Si temp es 0, todo apagado pero sin error
        tempReady = false;
        tempInRangeSince = 0;
        inicioFalloTemp = 0;
        msgReadySent = false;
      }
  

  // 4. TELEMETRÍA (Para el HMI)
  if (enviarDatosContinuos && !errorFatal && (millis() - lastReportTime > reportInterval)) {
      if (client && client.connected()) {
          int code = motorActivo ? 11 : 0;
          enviarPaqueteDatos(code); 
      }
      lastReportTime = millis();
  }
}

// ==========================================================
//             PROCESAMIENTO DE COMANDOS
// ==========================================================
void procesarComando(String cmd) {
  StringSplitter splitter(cmd, ' ', 2);
  if(splitter.getItemCount() < 1) return;

  String accionStr = splitter.getItemAtIndex(0);
  String valorStr = (splitter.getItemCount() >= 2) ? splitter.getItemAtIndex(1) : "0";
  int acci = accionStr.toInt();

  if (errorFatal && acci != 9) {
      if(client && client.connected()) client.println("SYSTEM BLOCKED");
      return;
  }

  switch(acci) {
    case 1: // IMPRIMIR
      //set_temperature = default_print_temp;
      if(tempReady) {
        float v_abb = convertirMyMM(valorStr); 
        frecuencia = calcularFrecuenciaExtrusor(v_abb*1.5);
        if (frecuencia > 1.0) { 
          long periodo_timer = 1000000.0 / (frecuencia * 2.0);
          digitalWrite(PIN_DIR, LOW);
          Timer1.setPeriod(periodo_timer);
          if(!motorActivo) {
            Timer1.start();
            motorActivo = true;
          }
          enviarDatosContinuos = true;
          enviarPaqueteDatos(11);
        } else if (frecuencia == 0){
          motorActivo = false;
          enviarDatosContinuos = true;
        }else {
          Timer1.stop(); 
          motorActivo = false;
          enviarDatosContinuos = false; 
          client.println("ERROR IDLE"); 
        }
      } else {
        Timer1.stop(); motorActivo = false;
        enviarDatosContinuos = true; 
        enviarPaqueteDatos(0); 
        client.println("WAITING TEMP");
      }
      break;

    case 2: // PRECALENTAR
      set_temperature = 210.0f;
      errorFatal = false; 
      inicioFalloTemp = 0; 
      client.println("OK PREHEAT 180");
      enviarDatosContinuos = true;
      break;

    case 3: // ENFRIAR
      set_temperature = 0.0f;
      Timer1.stop(); motorActivo = false;
      enviarDatosContinuos = true; 
      client.println("OK COOLING");
      break;

    case 4: // EXTRUIR MANUAL
      enviarDatosContinuos = true;
      if(readThermistorC() > 190.0) { 
        digitalWrite(PIN_DIR, LOW); 
        long periodo_manual = 1000000.0 / (manual_freq * 2.0);
        Timer1.setPeriod(periodo_manual); Timer1.start();
        motorActivo = true; 
        client.println("OK EXTRUDING MANUAL");
      } else {
        //Timer1.stop(); 
        //motorActivo = false; 
        client.println("ERROR COLD");
      }
      break;

    case 5: // RETRAER MANUAL
      enviarDatosContinuos = true;
      if(readThermistorC() > 190.0) {
        digitalWrite(PIN_DIR, HIGH); 
        long periodo_manual = 1000000.0 / (manual_freq * 2.0);
        Timer1.setPeriod(periodo_manual); Timer1.start();
        motorActivo = true; 
        client.println("OK RETRACTING MANUAL");
      } else {
        Timer1.stop(); motorActivo = false; client.println("ERROR COLD");
      }
      break;

    case 6: // STOP
      motorActivo = false; Timer1.stop();           
      //set_temperature = 0.0f; 
      enviarDatosContinuos = true; 
      errorFatal = false;
      client.println("OK STOPPED");
      break;

    // --- NUEVO: CASO 7 (FIJAR SETPOINT MANUALMENTE) ---
    case 7:
      {
        float nuevaTemp = valorStr.toFloat(); 
        // Limite seguridad 280 grados
        if (nuevaTemp >= 0 && nuevaTemp <= 280.0) {
            set_temperature = nuevaTemp;
            enviarDatosContinuos = true; 
            errorFatal = false; inicioFalloTemp = 0; tempReady = false; 
            client.println("OK NEW TEMP");
        } else {
            client.println("ERROR TEMP RANGE");
        }
      }
      break;
    case 8: 
    enviarDatosContinuos = true; 
    break;

    case 9: // EMERGENCIA
      apagarSistemaFatal();
      client.println("EMERGENCY RESET");
      // --- BLOQUE DE CORRECCIÓN ---
      client.flush();    // Esperar a que se envíen los datos de salida
      delay(200);        // Dar tiempo a la red
      client.stop();     // IMPRESCINDIBLE: Cerrar la conexión TCP activamente
      delay(500);        // Esperar a que el chip Ethernet procese el cierre
      // ----------------------------
      softReset();
      break;

    default:
      client.println("ERROR CMD");
      break;
  }
}

// ---------------- FUNCIONES AUXILIARES ----------------
void apagarSistemaFatal() {
  motorActivo = false; Timer1.stop();
  digitalWrite(PIN_EN, HIGH); 
  analogWrite(PWM_pin, 255);  
  enviarDatosContinuos = false;
}

String formatearNumero(float x) {
  String result;
  if (x >= 0) {
    if (x < 10)       result = "0000" + String(x, 2); 
    else if (x < 100) result = "000" + String(x, 2);
    else if (x < 1000) result = "00" + String(x, 2);
    else result = "0" + String(x, 2);
  } else {
    float absX = abs(x);
    if (absX < 10)       result = "-000" + String(absX, 2);
    else if (absX < 100) result = "-00" + String(absX, 2);
    else result = "-0" + String(absX, 2);
  }
  return result;
}

void enviarPaqueteDatos(int estadoCode) {
  String s_estado = formatearNumero((float)estadoCode);
  String s_tempActual = formatearNumero(currentTemperature);
  String s_setpoint = formatearNumero(set_temperature);
  client.print(s_estado + s_tempActual + s_setpoint);
  //client.println(s_estado + s_tempActual + s_setpoint);
}

void softReset() { asm volatile ("  jmp 0"); }

float convertirMyMM(String datoStr) { return datoStr.toFloat() * 1000.0; }

float readThermistorC() {
  int raw = analogRead(TERM_PIN);
  if (raw <= 0) raw = 1;        
  if (raw >= 1023) raw = 1022;

  float Vo = (float)raw;
  float R2 = R1 * (1023.0f / Vo - 1.0f);
  float logR2 = log(R2);
  float T = 1.0f / (c1 + c2*logR2 + c3*logR2*logR2*logR2); 
  T = T - 273.15f; // Celsius
  return T + temp_offset;
}

float calcularFrecuenciaExtrusor(float v_tcp_mm_s) {
    const float d_filamento = 1.75;      
    const float h = 0.40;                                                 
    const float w = 1.20;
    float k = (4.0 * h * w) / (PI * d_filamento * d_filamento);
    float v_ext = v_tcp_mm_s * k;
    float L = PI * D_gear; 
    return (v_ext / L) * (pasos_por_rev * microstepping);
}

void stepPulse() {
  if (motorActivo) {
    estadoStep = !estadoStep;
    digitalWrite(PIN_STEP, estadoStep);
  }
}