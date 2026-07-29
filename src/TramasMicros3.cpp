/*
 * TramasMicros3.cpp
 * 
 * Implementación del temporizador y planificador de tareas
 * 
 * Created on: 24 jul 2026
 * Author: DjSteker
 */

#include "TramasMicros3.h"
#include <avr/interrupt.h>
#include <avr/io.h>

// ============================================================================
// SystemTimer - Implementación (Timer1, CTC, prescaler 8, OCR1A=999 -> 1 ms)
// ============================================================================

static volatile uint32_t g_micros_counter = 0;

ISR(TIMER1_COMPA_vect) {
  g_micros_counter += 1000;  // 1000 microsegundos
}

void SystemTimer::init() {
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC, prescaler 8
  OCR1A = 999;                          // Comparación a 999 -> 1 ms con prescaler 8 a 8 MHz
  TIMSK1 = (1 << OCIE1A);               // Habilita interrupción por comparación A
  sei();
}

uint32_t SystemTimer::getMicros() {
  uint32_t base;
  uint16_t tcnt;
  uint8_t oldSREG = SREG;

  cli();
  base = g_micros_counter;
  tcnt = TCNT1;

  // Si la interrupción está pendiente y el contador ya ha avanzado poco,
  // significa que debemos sumar el ciclo completo
  if ((TIFR1 & (1 << OCF1A)) && (tcnt < (OCR1A / 2))) {
    base += (OCR1A + 1);
  }
  SREG = oldSREG;

  return base + (uint32_t)tcnt;  // Cada tick del timer equivale a 1 µs (con prescaler 8 y 8 MHz)
}

uint32_t SystemTimer::getMillis() {
  return getMicros() / 1000;
}

// ============================================================================
// TramasMicros3 - Implementación
// ============================================================================

TramasMicros3::TramasMicros3(unsigned long intervl, void (*function)(void))
  : active(true), previous(0), interval(intervl), execute(function) {
}

TramasMicros3::TramasMicros3(unsigned long prev, unsigned long intervl, void (*function)(void), bool enable)
  : active(enable), previous(prev), interval(intervl), execute(function) {
}

void TramasMicros3::reset() {
  previous = SystemTimer::getMicros();
}

void TramasMicros3::disable() {
  active = false;
}

void TramasMicros3::enable() {
  active = true;
}

void TramasMicros3::setInterval(unsigned long intervl) {
  interval = intervl;
}

void TramasMicros3::check() {
  if (active && (SystemTimer::getMicros() - previous >= interval)) {
    previous = SystemTimer::getMicros();
    if (execute) {
      execute();
    }
  } else if (active && SystemTimer::getMicros() < previous) {
    unsigned long elapsed = (0xFFFFFFFF - previous) + SystemTimer::getMicros() + 1;
    if (elapsed >= interval) {
      previous = SystemTimer::getMicros();
      if (execute) {
        execute();
      }
    }
  }
}

bool TramasMicros3::isDue() {
  if (!active) {
    return false;
  }
  unsigned long now = SystemTimer::getMicros();
  if (now - previous >= interval) {
    previous = now;
    return true;
  }
  if (now < previous) {
    unsigned long elapsed = (0xFFFFFFFF - previous) + now + 1;
    if (elapsed >= interval) {
      previous = now;
      return true;
    }
  }
  return false;
}

// ============================================================================
// TaskScheduler - Implementación
// ============================================================================

TramasMicros3 *TaskScheduler::tasks[MAX_TIMED_TASKS] = { nullptr };
uint8_t TaskScheduler::taskCount = 0;

void TaskScheduler::registerTask(TramasMicros3 *task) {
  if (taskCount < MAX_TIMED_TASKS) {
    tasks[taskCount++] = task;
  }
}

bool TaskScheduler::removeTask(TramasMicros3 *task) {
  for (uint8_t i = 0; i < taskCount; i++) {
    if (tasks[i] == task) {
      tasks[i] = nullptr;  // Marcar como eliminado (hueco)
      return true;
    }
  }
  return false;
}

void TaskScheduler::checkAll() {
  for (uint8_t i = 0; i < taskCount; i++) {
    if (tasks[i]) {  // Solo ejecuta tareas no eliminadas
      tasks[i]->check();
    }
  }
}
