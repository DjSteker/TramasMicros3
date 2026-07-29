/*
 * TramasMicros3_Micros.ino
 * 
 * Ejemplo: Tareas COMPLETAMENTE dinámicas
 * - Sin predeclaraciones de funciones con índices fijos
 * - Array dinámico de punteros a función
 * - Cada tarea se crea y destruye en tiempo de ejecución
 * - Muestra tiempo en microsegundos
 * 
 * Created on: 24 jul 2026
 * Author: DjSteker
 */

#include "TramasMicros3.h"

#define LED_PIN LED_BUILTIN
#define MAX_TASKS 10

// ============================================================================
// ESTRUCTURA PARA TAREAS DINÁMICAS
// ============================================================================

struct DynamicTask {
  TramasMicros3* timer;
  char* message;
  uint8_t id;
  bool active;
  
  DynamicTask() : timer(nullptr), message(nullptr), id(0), active(false) {}
};

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

DynamicTask* taskList[MAX_TASKS] = { nullptr };
uint8_t taskCount = 0;
uint8_t nextTaskId = 1;

bool ledState = false;
TramasMicros3* ledTimer = nullptr;

// ============================================================================
// FUNCIÓN DISPATCH ÚNICA (recibe el slot como parámetro)
// ============================================================================

void taskDispatch(uint8_t slot) {
  if (slot < MAX_TASKS && taskList[slot] != nullptr && taskList[slot]->active) {
    Serial.print(F("[Tarea "));
    Serial.print(taskList[slot]->id);
    Serial.print(F("] "));
    Serial.print(taskList[slot]->message);
    Serial.print(F(" | tiempo: "));
    Serial.print(SystemTimer::getMicros());
    Serial.println(F(" µs"));
  }
}

// ============================================================================
// GENERADOR DE THUNKS: crea funciones únicas que llaman a taskDispatch con su slot
// ============================================================================

// Array de punteros a función (se llena dinámicamente)
void (*taskFunctions[MAX_TASKS])(void) = { nullptr };

// Macros para generar funciones thunk sin tener que escribirlas manualmente
// Cada thunk es una función sin captura que llama a taskDispatch con un índice fijo
#define MAKE_THUNK(n) \
  void thunk_##n() { taskDispatch(n); }

// Generamos los thunks necesarios (uno por cada posible slot)
MAKE_THUNK(0)
MAKE_THUNK(1)
MAKE_THUNK(2)
MAKE_THUNK(3)
MAKE_THUNK(4)
MAKE_THUNK(5)
MAKE_THUNK(6)
MAKE_THUNK(7)
MAKE_THUNK(8)
MAKE_THUNK(9)

// Array que mapea slot -> función thunk correspondiente
void (*thunkTable[MAX_TASKS])(void) = {
  thunk_0, thunk_1, thunk_2, thunk_3, thunk_4,
  thunk_5, thunk_6, thunk_7, thunk_8, thunk_9
};

// ============================================================================
// FUNCIONES DEL LED
// ============================================================================

void toggleLed() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  
  // Serial.print(F("[LED] "));
  // Serial.print(ledState ? F("ON") : F("OFF"));
  // Serial.print(F(" | tiempo: "));
  // Serial.print(SystemTimer::getMicros());
  // Serial.println(F(" µs"));
}

// ============================================================================
// FUNCIONES AUXILIARES
// ============================================================================

int getFreeRam() {
  extern unsigned int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

int8_t findFreeSlot() {
  for (uint8_t i = 0; i < MAX_TASKS; i++) {
    if (taskList[i] == nullptr) {
      return i;
    }
  }
  return -1;
}

// ============================================================================
// GESTIÓN DE TAREAS
// ============================================================================

bool addTask(unsigned long intervalUs, const char* message) {
  if (intervalUs == 0 || message == nullptr || strlen(message) == 0) {
    Serial.println(F("Error: Parámetros inválidos"));
    return false;
  }
  
  int8_t slot = findFreeSlot();
  if (slot < 0) {
    Serial.println(F("Error: Máximo de tareas alcanzado"));
    return false;
  }
  
  // Crear estructura dinámica
  DynamicTask* newTask = new DynamicTask();
  if (newTask == nullptr) {
    Serial.println(F("Error: Sin memoria para crear tarea"));
    return false;
  }
  
  // Copiar mensaje a memoria dinámica
  newTask->message = new char[strlen(message) + 1];
  if (newTask->message == nullptr) {
    delete newTask;
    Serial.println(F("Error: Sin memoria para el mensaje"));
    return false;
  }
  strcpy(newTask->message, message);
  
  // Asignar ID y estado
  newTask->id = nextTaskId++;
  newTask->active = true;
  
  // Asignar la función thunk correspondiente al slot
  taskFunctions[slot] = thunkTable[slot];
  
  // Crear temporizador
  newTask->timer = new TramasMicros3(intervalUs, taskFunctions[slot]);
  if (newTask->timer == nullptr) {
    delete[] newTask->message;
    delete newTask;
    taskFunctions[slot] = nullptr;
    Serial.println(F("Error: Sin memoria para el temporizador"));
    return false;
  }
  
  // Registrar en el planificador
  TaskScheduler::registerTask(newTask->timer);
  
  // Guardar en la lista
  taskList[slot] = newTask;
  taskCount++;
  
  // Mostrar confirmación
  Serial.print(F("Tarea creada [ID="));
  Serial.print(newTask->id);
  Serial.print(F(", slot="));
  Serial.print(slot);
  Serial.print(F(", intervalo="));
  Serial.print(intervalUs);
  Serial.print(F(" µs"));
  if (intervalUs >= 1000) {
    Serial.print(F(" ("));
    Serial.print(intervalUs / 1000.0, 3);
    Serial.print(F(" ms)"));
  }
  Serial.print(F(", mensaje=\""));
  Serial.print(newTask->message);
  Serial.println(F("\"]"));
  
  Serial.print(F("RAM libre: "));
  Serial.print(getFreeRam());
  Serial.println(F(" bytes"));
  
  return true;
}

bool deleteTaskById(uint8_t taskId) {
  for (uint8_t i = 0; i < MAX_TASKS; i++) {
    if (taskList[i] != nullptr && taskList[i]->id == taskId) {
      DynamicTask* task = taskList[i];
      
      // Eliminar del planificador
      TaskScheduler::removeTask(task->timer);
      
      // Liberar memoria
      delete task->timer;
      delete[] task->message;
      delete task;
      
      // Limpiar slot
      taskList[i] = nullptr;
      taskFunctions[i] = nullptr;
      taskCount--;
      
      Serial.print(F("Tarea eliminada [ID="));
      Serial.print(taskId);
      Serial.print(F(", slot="));
      Serial.print(i);
      Serial.print(F("] - RAM libre: "));
      Serial.print(getFreeRam());
      Serial.println(F(" bytes"));
      
      return true;
    }
  }
  
  Serial.print(F("Error: Tarea con ID="));
  Serial.print(taskId);
  Serial.println(F(" no encontrada"));
  return false;
}

bool deleteTaskBySlot(uint8_t slot) {
  if (slot >= MAX_TASKS || taskList[slot] == nullptr) {
    Serial.println(F("Error: Slot inválido o vacío"));
    return false;
  }
  
  return deleteTaskById(taskList[slot]->id);
}

void deleteAllTasks() {
  uint8_t deletedCount = 0;
  for (uint8_t i = 0; i < MAX_TASKS; i++) {
    if (taskList[i] != nullptr) {
      DynamicTask* task = taskList[i];
      TaskScheduler::removeTask(task->timer);
      delete task->timer;
      delete[] task->message;
      delete task;
      taskList[i] = nullptr;
      taskFunctions[i] = nullptr;
      deletedCount++;
    }
  }
  taskCount = 0;
  
  Serial.print(F("Todas las tareas eliminadas ("));
  Serial.print(deletedCount);
  Serial.print(F(" tareas) - RAM libre: "));
  Serial.print(getFreeRam());
  Serial.println(F(" bytes"));
}

void listTasks() {
  Serial.println(F("\n╔══════════════════════════════════════════════════════╗"));
  Serial.println(F("║              TAREAS ACTIVAS                          ║"));
  Serial.println(F("╠══════════════════════════════════════════════════════╣"));
  
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_TASKS; i++) {
    if (taskList[i] != nullptr && taskList[i]->active) {
      Serial.print(F("║ Slot "));
      if (i < 10) Serial.print(' ');
      Serial.print(i);
      Serial.print(F(" | ID "));
      if (taskList[i]->id < 10) Serial.print(' ');
      Serial.print(taskList[i]->id);
      Serial.print(F(" | "));
      
      // Formatear microsegundos
      unsigned long us = taskList[i]->timer->interval;
      Serial.print(us);
      Serial.print(F(" µs"));
      
      // Espaciado
      int digits = 1;
      unsigned long temp = us;
      while (temp >= 10) { temp /= 10; digits++; }
      for (int s = digits; s < 10; s++) Serial.print(' ');
      
      Serial.print(F("| "));
      Serial.println(taskList[i]->message);
      count++;
    }
  }
  
  if (count == 0) {
    Serial.println(F("║         (sin tareas activas)                        ║"));
  }
  
  Serial.println(F("╚══════════════════════════════════════════════════════╝"));
  Serial.print(F("Total: "));
  Serial.print(count);
  Serial.print(F(" tareas | RAM libre: "));
  Serial.print(getFreeRam());
  Serial.println(F(" bytes\n"));
}

// ============================================================================
// PROCESAMIENTO DE COMANDOS
// ============================================================================

void processCommand(String& input) {
  input.trim();
  if (input.length() == 0) return;
  
  int firstSpace = input.indexOf(' ');
  String cmd = (firstSpace > 0) ? input.substring(0, firstSpace) : input;
  cmd.toLowerCase();
  
  if (cmd == "add") {
    int secondSpace = input.indexOf(' ', firstSpace + 1);
    if (secondSpace > 0) {
      unsigned long intervalUs = input.substring(firstSpace + 1, secondSpace).toInt();
      String message = input.substring(secondSpace + 1);
      addTask(intervalUs, message.c_str());
    } else {
      Serial.println(F("Uso: add <microsegundos> <mensaje>"));
    }
  }
  else if (cmd == "del") {
    if (firstSpace > 0) {
      uint8_t id = input.substring(firstSpace + 1).toInt();
      deleteTaskById(id);
    } else {
      Serial.println(F("Uso: del <id>"));
    }
  }
  else if (cmd == "delslot") {
    if (firstSpace > 0) {
      uint8_t slot = input.substring(firstSpace + 1).toInt();
      deleteTaskBySlot(slot);
    } else {
      Serial.println(F("Uso: delslot <slot>"));
    }
  }
  else if (cmd == "delall") {
    deleteAllTasks();
  }
  else if (cmd == "list") {
    listTasks();
  }
  else if (cmd == "free") {
    Serial.print(F("RAM libre: "));
    Serial.print(getFreeRam());
    Serial.println(F(" bytes"));
  }
  else if (cmd == "led") {
    String action = (firstSpace > 0) ? input.substring(firstSpace + 1) : "";
    action.toLowerCase();
    
    if (action == "on") {
      if (ledTimer == nullptr) {
        ledTimer = new TramasMicros3(500000, toggleLed);
        TaskScheduler::registerTask(ledTimer);
      }
      ledTimer->enable();
      Serial.println(F("LED activado"));
    }
    else if (action == "off") {
      if (ledTimer != nullptr) {
        ledTimer->disable();
        digitalWrite(LED_PIN, LOW);
        ledState = false;
      }
      Serial.println(F("LED desactivado"));
    }
    else if (action == "del") {
      if (ledTimer != nullptr) {
        TaskScheduler::removeTask(ledTimer);
        delete ledTimer;
        ledTimer = nullptr;
        digitalWrite(LED_PIN, LOW);
        ledState = false;
        Serial.println(F("LED eliminado"));
      }
    }
    else {
      Serial.println(F("Uso: led on|off|del"));
    }
  }
  else if (cmd == "help") {
    Serial.println(F("\n╔══════════════════════════════════════════════════════╗"));
    Serial.println(F("║     TramasMicros3 - Tareas Dinámicas v2              ║"));
    Serial.println(F("╠══════════════════════════════════════════════════════╣"));
    Serial.println(F("║ Comandos:                                            ║"));
    Serial.println(F("║  add <us> <msg>    - Crear tarea (microsegundos)     ║"));
    Serial.println(F("║  del <id>          - Eliminar por ID                 ║"));
    Serial.println(F("║  delslot <slot>    - Eliminar por slot               ║"));
    Serial.println(F("║  delall            - Eliminar TODAS las tareas       ║"));
    Serial.println(F("║  list              - Listar tareas                   ║"));
    Serial.println(F("║  free              - RAM libre                       ║"));
    Serial.println(F("║  led on/off/del    - Control LED                     ║"));
    Serial.println(F("║  help              - Esta ayuda                      ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════╝\n"));
  }
  else {
    Serial.println(F("Comando desconocido. 'help' para ayuda."));
  }
}

// ============================================================================
// SETUP Y LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  SystemTimer::init();
  
  Serial.println(F("\n╔══════════════════════════════════════════════════════╗"));
  Serial.println(F("║     TramasMicros3 - Sistema de Tareas Dinámicas      ║"));
  Serial.println(F("║     SIN predeclaraciones de funciones fijas           ║"));
  Serial.println(F("╚══════════════════════════════════════════════════════╝"));
  
  Serial.print(F("RAM libre inicial: "));
  Serial.print(getFreeRam());
  Serial.println(F(" bytes"));
  Serial.println(F("Escribe 'help' para ver los comandos disponibles.\n"));
}

void loop() {
  TaskScheduler::checkAll();
  
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    processCommand(input);
  }
}
