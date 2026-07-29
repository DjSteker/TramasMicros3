/*
 * TramasMicros3.ino
 * 
 * 
 * Created on: 24 jul 2026
 * Author: DjSteker
 */

#include "TramasMicros3.h"

#define USER_MAX_TASKS 10

TramasMicros3* userTasks[USER_MAX_TASKS] = { nullptr };
uint8_t userTaskCount = 0;

// Almacenamiento para los mensajes de las tareas
char taskMessages[USER_MAX_TASKS][32];

// Funciones de tarea individuales (sin captura)
void taskFunction0() {
  Serial.println(taskMessages[0]);
}
void taskFunction1() {
  Serial.println(taskMessages[1]);
}
void taskFunction2() {
  Serial.println(taskMessages[2]);
}
void taskFunction3() {
  Serial.println(taskMessages[3]);
}
void taskFunction4() {
  Serial.println(taskMessages[4]);
}
void taskFunction5() {
  Serial.println(taskMessages[5]);
}
void taskFunction6() {
  Serial.println(taskMessages[6]);
}
void taskFunction7() {
  Serial.println(taskMessages[7]);
}
void taskFunction8() {
  Serial.println(taskMessages[8]);
}
void taskFunction9() {
  Serial.println(taskMessages[9]);
}

// Array de punteros a funciones
void (*taskFunctions[])(void) = {
  taskFunction0, taskFunction1, taskFunction2, taskFunction3, taskFunction4,
  taskFunction5, taskFunction6, taskFunction7, taskFunction8, taskFunction9
};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  SystemTimer::init();

  Serial.println(F("=== Control de tareas por Serial ==="));
  Serial.println(F("Comandos:"));
  Serial.println(F("  add <ms> <mensaje>   – Crea una tarea periódica"));
  Serial.println(F("  del <indice>         – Elimina la tarea con ese índice"));
  Serial.println(F("  list                 – Muestra todas las tareas activas"));
  Serial.println(F("  free                 – Memoria libre"));
}

void loop() {
  TaskScheduler::checkAll();

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) {
      return;
    }

    int firstSpace = input.indexOf(' ');
    String cmd = (firstSpace > 0) ? input.substring(0, firstSpace) : input;
    cmd.toLowerCase();

    if (cmd == "add") {
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

          // Crear tarea con función global
          TramasMicros3* t = new TramasMicros3(intervalMs * 1000UL, taskFunctions[userTaskCount]);
          TaskScheduler::registerTask(t);
          userTasks[userTaskCount++] = t;

          Serial.print(F("Tarea añadida, índice: "));
          Serial.println(userTaskCount - 1);
        } else {
          Serial.println(F("Uso: add <ms> <mensaje>"));
        }
      } else {
        Serial.println(F("Uso: add <ms> <mensaje>"));
      }

    } else if (cmd == "del") {
      int index = input.substring(firstSpace + 1).toInt();
      if (index >= 0 && index < userTaskCount && userTasks[index] != nullptr) {
        TramasMicros3* t = userTasks[index];
        TaskScheduler::removeTask(t);
        delete t;
        userTasks[index] = nullptr;

        // Compactar arrays
        for (uint8_t i = index; i < userTaskCount - 1; i++) {
          userTasks[i] = userTasks[i + 1];
          strcpy(taskMessages[i], taskMessages[i + 1]);
        }
        userTasks[--userTaskCount] = nullptr;
        taskMessages[userTaskCount][0] = '\0';

        Serial.print(F("Tarea eliminada, índice: "));
        Serial.println(index);
      } else {
        Serial.println(F("Índice inválido"));
      }

    } else if (cmd == "list") {
      Serial.println(F("--- Tareas activas ---"));
      for (uint8_t i = 0; i < userTaskCount; i++) {
        if (userTasks[i]) {
          Serial.print(i);
          Serial.print(F(": intervalo="));
          Serial.print(userTasks[i]->interval / 1000.0);
          Serial.print(F(" ms, activa="));
          Serial.print(userTasks[i]->active ? "SI" : "NO");
          Serial.print(F(" mensaje="));
          Serial.println(taskMessages[i]);
        }
      }
      Serial.println(F("----------------------"));

    } else if (cmd == "free") {
      extern unsigned int __heap_start, *__brkval;
      int v;
      int freeRam = (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
      Serial.print(F("RAM libre: "));
      Serial.print(freeRam);
      Serial.println(F(" bytes"));

    } else {
      Serial.println(F("Comando desconocido"));
    }
  }
}
