/*
 * TramasMicros3.ino
 * 
 * Created on: 24 jul 2026
 * Author: DjSteker
 */

#include "TramasMicros3.h"

#define LED_PIN LED_BUILTIN
#define USER_MAX_TASKS 10

// Variables para el LED
bool ledState = false;
TramasMicros3* ledTask = nullptr;          // Puntero a la tarea del LED
bool ledTaskActive = true;                 // Estado de la tarea LED

// Arrays para tareas dinámicas de usuario
TramasMicros3* userTasks[USER_MAX_TASKS] = { nullptr };
uint8_t userTaskCount = 0;

// Almacenamiento para los mensajes de las tareas
char taskMessages[USER_MAX_TASKS][32];

// ============================================================================
// FUNCIONES DE TAREA (sin captura, compatibles con punteros a función)
// ============================================================================

// Función para el parpadeo del LED
void toggleLed() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  //Serial.print(F("[LED] "));
  //Serial.println(ledState ? F("ON") : F("OFF"));
}

// Funciones para tareas de usuario con mensajes
void taskFunction0() { Serial.println(taskMessages[0]); }
void taskFunction1() { Serial.println(taskMessages[1]); }
void taskFunction2() { Serial.println(taskMessages[2]); }
void taskFunction3() { Serial.println(taskMessages[3]); }
void taskFunction4() { Serial.println(taskMessages[4]); }
void taskFunction5() { Serial.println(taskMessages[5]); }
void taskFunction6() { Serial.println(taskMessages[6]); }
void taskFunction7() { Serial.println(taskMessages[7]); }
void taskFunction8() { Serial.println(taskMessages[8]); }
void taskFunction9() { Serial.println(taskMessages[9]); }

// Array de punteros a funciones para asignación dinámica
void (*taskFunctions[])(void) = {
  taskFunction0, taskFunction1, taskFunction2, taskFunction3, taskFunction4,
  taskFunction5, taskFunction6, taskFunction7, taskFunction8, taskFunction9
};

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  // Configurar LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Inicializar temporizador del sistema
  SystemTimer::init();

  // Crear tarea del LED como tarea dinámica (puede eliminarse con "del 0")
  ledTask = new TramasMicros3(500000UL, toggleLed);  // 500 ms = 0.5 Hz
  TaskScheduler::registerTask(ledTask);

  // Mostrar menú de ayuda
  Serial.println(F("\n╔══════════════════════════════════════════════╗"));
  Serial.println(F("║     TramasMicros3 - Control de Tareas        ║"));
  Serial.println(F("╠══════════════════════════════════════════════╣"));
  Serial.println(F("║ Comandos disponibles:                        ║"));
  Serial.println(F("║  add <ms> <mensaje>  - Crear tarea periódica ║"));
  Serial.println(F("║  del <indice>        - Eliminar tarea        ║"));
  Serial.println(F("║  led on/off          - Activar/desactivar LED║"));
  Serial.println(F("║  list                - Listar tareas activas ║"));
  Serial.println(F("║  free                - Mostrar RAM libre     ║"));
  Serial.println(F("║  help                - Mostrar esta ayuda    ║"));
  Serial.println(F("╚══════════════════════════════════════════════╝\n"));

  Serial.print(F("RAM libre inicial: "));
  Serial.print(getFreeRam());
  Serial.println(F(" bytes\n"));
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================

void loop() {
  // Ejecutar todas las tareas registradas
  TaskScheduler::checkAll();

  // Procesar comandos del puerto serie
  if (Serial.available()) {
    processSerialCommand();
  }
}

// ============================================================================
// FUNCIONES AUXILIARES
// ============================================================================

// Obtener RAM libre (para microcontroladores AVR)
int getFreeRam() {
  extern unsigned int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// Procesar comandos recibidos por Serial
void processSerialCommand() {
  String input = Serial.readStringUntil('\n');
  input.trim();
  if (input.length() == 0) {
    return;
  }

  // Separar comando del resto de argumentos
  int firstSpace = input.indexOf(' ');
  String cmd = (firstSpace > 0) ? input.substring(0, firstSpace) : input;
  cmd.toLowerCase();

  if (cmd == "add") {
    handleAddCommand(input, firstSpace);

  } else if (cmd == "del") {
    handleDeleteCommand(input, firstSpace);

  } else if (cmd == "led") {
    handleLedCommand(input, firstSpace);

  } else if (cmd == "list") {
    handleListCommand();

  } else if (cmd == "free") {
    Serial.print(F("RAM libre: "));
    Serial.print(getFreeRam());
    Serial.println(F(" bytes"));

  } else if (cmd == "help") {
    printHelp();

  } else {
    Serial.println(F("Comando desconocido. Escribe 'help' para ayuda."));
  }
}

// ============================================================================
// MANEJADORES DE COMANDOS
// ============================================================================

void handleAddCommand(String &input, int firstSpace) {
  int secondSpace = input.indexOf(' ', firstSpace + 1);
  if (secondSpace > 0) {
    unsigned long intervalMs = input.substring(firstSpace + 1, secondSpace).toInt();
    String message = input.substring(secondSpace + 1);

    if (intervalMs > 0 && message.length() > 0) {
      if (userTaskCount >= USER_MAX_TASKS) {
        Serial.println(F("Error: máximo de tareas alcanzado"));
        return;
      }

      // Copiar mensaje al almacenamiento global
      strncpy(taskMessages[userTaskCount], message.c_str(), 31);
      taskMessages[userTaskCount][31] = '\0';

      // Crear tarea dinámica en el heap
      TramasMicros3* t = new TramasMicros3(intervalMs * 1000UL, taskFunctions[userTaskCount]);
      if (t == nullptr) {
        Serial.println(F("Error: sin memoria para crear tarea"));
        return;
      }

      TaskScheduler::registerTask(t);
      userTasks[userTaskCount++] = t;

      Serial.print(F("Tarea creada [índice="));
      Serial.print(userTaskCount - 1);
      Serial.print(F(", intervalo="));
      Serial.print(intervalMs);
      Serial.print(F(" ms, mensaje=\""));
      Serial.print(message);
      Serial.println(F("\"]"));
    } else {
      Serial.println(F("Uso: add <ms> <mensaje>"));
    }
  } else {
    Serial.println(F("Uso: add <ms> <mensaje>"));
  }
}

void handleDeleteCommand(String &input, int firstSpace) {
  int index = input.substring(firstSpace + 1).toInt();

  // Caso especial: eliminar tarea LED
  if (index == -1 && ledTask != nullptr) {
    TaskScheduler::removeTask(ledTask);
    delete ledTask;
    ledTask = nullptr;
    ledTaskActive = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println(F("Tarea LED eliminada"));
    return;
  }

  // Eliminar tarea de usuario
  if (index >= 0 && index < userTaskCount && userTasks[index] != nullptr) {
    TramasMicros3* t = userTasks[index];
    TaskScheduler::removeTask(t);
    delete t;
    userTasks[index] = nullptr;

    // Compactar arrays para no dejar huecos
    for (uint8_t i = index; i < userTaskCount - 1; i++) {
      userTasks[i] = userTasks[i + 1];
      strcpy(taskMessages[i], taskMessages[i + 1]);
    }
    userTasks[--userTaskCount] = nullptr;
    taskMessages[userTaskCount][0] = '\0';

    Serial.print(F("Tarea eliminada [índice="));
    Serial.print(index);
    Serial.println(F("]"));
    Serial.print(F("RAM libre: "));
    Serial.print(getFreeRam());
    Serial.println(F(" bytes"));
  } else {
    Serial.print(F("Índice inválido. Usa 'del <0.."));
    Serial.print(userTaskCount - 1);
    Serial.println(F(">' o 'del -1' para el LED"));
  }
}

void handleLedCommand(String &input, int firstSpace) {
  String action = input.substring(firstSpace + 1);
  action.toLowerCase();

  if (action == "on") {
    if (ledTask == nullptr) {
      ledTask = new TramasMicros3(500000UL, toggleLed);
      TaskScheduler::registerTask(ledTask);
    }
    ledTask->enable();
    ledTaskActive = true;
    Serial.println(F("LED activado (parpadeo cada 500 ms)"));

  } else if (action == "off") {
    if (ledTask != nullptr) {
      ledTask->disable();
      digitalWrite(LED_PIN, LOW);
      ledState = false;
    }
    ledTaskActive = false;
    Serial.println(F("LED desactivado"));

  } else if (action == "del") {
    if (ledTask != nullptr) {
      TaskScheduler::removeTask(ledTask);
      delete ledTask;
      ledTask = nullptr;
      digitalWrite(LED_PIN, LOW);
      ledState = false;
      ledTaskActive = false;
      Serial.println(F("Tarea LED eliminada (memoria liberada)"));
    } else {
      Serial.println(F("No hay tarea LED que eliminar"));
    }

  } else if (action == "status") {
    Serial.print(F("LED: "));
    if (ledTask != nullptr) {
      Serial.print(F("activo, intervalo="));
      Serial.print(ledTask->interval / 1000.0);
      Serial.println(F(" ms"));
    } else {
      Serial.println(F("no existe"));
    }

  } else {
    Serial.println(F("Uso: led on|off|del|status"));
  }
}

void handleListCommand() {
  Serial.println(F("\n--- Tareas activas ---"));
  uint8_t totalTasks = 0;

  // Mostrar tarea LED si existe
  if (ledTask != nullptr) {
    Serial.print(F("[LED] "));
    Serial.print(F("intervalo="));
    Serial.print(ledTask->interval / 1000.0);
    Serial.print(F(" ms, activa="));
    Serial.print(ledTask->active ? F("SI") : F("NO"));
    Serial.print(F(", RAM="));
    Serial.print(sizeof(TramasMicros3));
    Serial.println(F(" bytes"));
    totalTasks++;
  }

  // Mostrar tareas de usuario
  for (uint8_t i = 0; i < userTaskCount; i++) {
    if (userTasks[i]) {
      Serial.print(F("["));
      Serial.print(i);
      Serial.print(F("] "));
      Serial.print(F("intervalo="));
      Serial.print(userTasks[i]->interval / 1000.0);
      Serial.print(F(" ms, activa="));
      Serial.print(userTasks[i]->active ? F("SI") : F("NO"));
      Serial.print(F(", mensaje=\""));
      Serial.print(taskMessages[i]);
      Serial.print(F("\", RAM="));
      Serial.print(sizeof(TramasMicros3));
      Serial.println(F(" bytes"));
      totalTasks++;
    }
  }

  if (totalTasks == 0) {
    Serial.println(F("  (ninguna tarea activa)"));
  }

  Serial.print(F("Total: "));
  Serial.print(totalTasks);
  Serial.print(F(" tareas, RAM usada: ~"));
  Serial.print(totalTasks * sizeof(TramasMicros3));
  Serial.println(F(" bytes"));
  Serial.println(F("----------------------\n"));
}

void printHelp() {
  Serial.println(F("\n╔══════════════════════════════════════════════╗"));
  Serial.println(F("║     TramasMicros3 - Control de Tareas        ║"));
  Serial.println(F("╠══════════════════════════════════════════════╣"));
  Serial.println(F("║ Comandos:                                    ║"));
  Serial.println(F("║  add 1000 Hola    - Tarea cada 1s            ║"));
  Serial.println(F("║  add 500 Test     - Tarea cada 0.5s          ║"));
  Serial.println(F("║  del 0            - Eliminar tarea índice 0  ║"));
  Serial.println(F("║  led on           - Activar parpadeo LED     ║"));
  Serial.println(F("║  led off          - Pausar parpadeo LED      ║"));
  Serial.println(F("║  led del          - Eliminar tarea LED       ║"));
  Serial.println(F("║  led status       - Estado del LED           ║"));
  Serial.println(F("║  list             - Listar tareas            ║"));
  Serial.println(F("║  free             - RAM libre                ║"));
  Serial.println(F("║  help             - Esta ayuda               ║"));
  Serial.println(F("╚══════════════════════════════════════════════╝\n"));
}
