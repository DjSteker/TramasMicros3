/*
 * TramasMicros3.cpp
 * 
 * Implementación del temporizador y planificador de tareas
 * Con prescaler 1:1 para mayor resolución
 * Created on: 24 jul 2026
 * Author: DjSteker
 */

#ifndef F_CPU
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
#define F_CPU 16000000UL  // 16 MHz (Standard Arduino Uno, Mega, Leonardo)
#elif defined(__AVR_ATmega328__) || defined(__AVR_ATmega8__) || defined(__AVR_ATtiny85__)
#define F_CPU 8000000UL  // 8 MHz (Versiones a 8MHz o cristales internos)
#elif defined(__AVR_ATmega168__)
#define F_CPU 16000000UL  // O 8000000UL según versión, por defecto 16MHz
#else
#define F_CPU 16000000UL  // Valor por defecto seguro para la mayoría
#endif
#endif

#include "TramasMicros3.h"
#include <avr/interrupt.h>
#include <avr/io.h>

// ============================================================================
// SystemTimer - Implementación (Timer1, CTC, prescaler 1, OCR1A ajustado)
// ============================================================================

// Constantes para el timer con prescaler 1
// F_CPU depende de la placa (16 MHz para Arduino Uno, 8 MHz para 3.3V)

// Con prescaler 1, cada tick del timer = 1 ciclo de CPU
// Para 16 MHz: 1 tick = 62.5 ns
// Para 8 MHz:  1 tick = 125 ns
#define TIMER_PRESCALER 1
#define TICKS_PER_MICROSECOND (F_CPU / 1000000UL / TIMER_PRESCALER)

// OCR1A para generar interrupción cada 1000 µs (1 ms)
// Con prescaler 1 a 16 MHz: 16000 ticks = 1000 µs -> OCR1A = 15999
// Con prescaler 1 a 8 MHz:  8000 ticks = 1000 µs  -> OCR1A = 7999
#define OCR1A_VALUE ((F_CPU / 1000UL / TIMER_PRESCALER) - 1)

static volatile uint32_t g_micros_counter = 0;
static volatile uint16_t g_micros_fraction = 0;  // Para acumular fracciones de µs

ISR(TIMER1_COMPA_vect) {
  // Con prescaler 1 a 16 MHz: 16000 ticks = 1000 µs exactos
  g_micros_counter += 1000;
}

void SystemTimer::init() {
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // Modo CTC con prescaler 1
  TCCR1B = (1 << WGM12) | (1 << CS10);  // CTC, prescaler 1

  // OCR1A calculado según F_CPU
  OCR1A = OCR1A_VALUE;

  // Habilitar interrupción por comparación A
  TIMSK1 = (1 << OCIE1A);

  sei();

#ifdef DEBUG_TIMER
  Serial.print(F("Timer1 configurado: prescaler=1, OCR1A="));
  Serial.print(OCR1A);
  Serial.print(F(", ticks/µs="));
  Serial.println(TICKS_PER_MICROSECOND);
#endif
}

uint32_t SystemTimer::getMicros() {
  uint32_t base;
  uint16_t tcnt;
  uint8_t oldSREG = SREG;

  cli();
  base = g_micros_counter;
  tcnt = TCNT1;

  // Verificar si la interrupción está pendiente
  if ((TIFR1 & (1 << OCF1A)) && (tcnt < (OCR1A / 2))) {
    base += 1000;
  }
  SREG = oldSREG;

  // Convertir ticks del timer a microsegundos
  // tcnt contiene los ticks transcurridos desde la última interrupción
  uint32_t micros_fraction = (uint32_t)tcnt / TICKS_PER_MICROSECOND;

  return base + micros_fraction;
}

uint32_t SystemTimer::getMillis() {
  return getMicros() / 1000;
}

// ============================================================================
// TramasMicros3 - Implementación (sin cambios)
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
// TaskScheduler - Implementación (sin cambios)
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
      tasks[i] = nullptr;
      return true;
    }
  }
  return false;
}

void TaskScheduler::checkAll() {
  for (uint8_t i = 0; i < taskCount; i++) {
    if (tasks[i]) {
      tasks[i]->check();
    }
  }
}


/*
 * TramasMicros3.cpp
 * 
 * Implementación del temporizador y planificador de tareas
 * 
 * Created on: 24 jul 2026
 * Author: DjSteker
 */

// #include "TramasMicros3.h"
// #include <avr/interrupt.h>
// #include <avr/io.h>

// // ============================================================================
// // SystemTimer - Implementación (Timer1, CTC, prescaler 8, OCR1A=999 -> 1 ms)
// // ============================================================================

// static volatile uint32_t g_micros_counter = 0;

// ISR(TIMER1_COMPA_vect) {
//   g_micros_counter += 1000;  // 1000 microsegundos
// }

// void SystemTimer::init() {
//   TCCR1A = 0;
//   TCCR1B = 0;
//   TCNT1 = 0;
//   TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC, prescaler 8
//   OCR1A = 999;                          // Comparación a 999 -> 1 ms con prescaler 8 a 8 MHz
//   TIMSK1 = (1 << OCIE1A);               // Habilita interrupción por comparación A
//   sei();
// }

// uint32_t SystemTimer::getMicros() {
//   uint32_t base;
//   uint16_t tcnt;
//   uint8_t oldSREG = SREG;

//   cli();
//   base = g_micros_counter;
//   tcnt = TCNT1;

//   // Si la interrupción está pendiente y el contador ya ha avanzado poco,
//   // significa que debemos sumar el ciclo completo
//   if ((TIFR1 & (1 << OCF1A)) && (tcnt < (OCR1A / 2))) {
//     base += (OCR1A + 1);
//   }
//   SREG = oldSREG;

//   return base + (uint32_t)tcnt;  // Cada tick del timer equivale a 1 µs (con prescaler 8 y 8 MHz)
// }

// uint32_t SystemTimer::getMillis() {
//   return getMicros() / 1000;
// }

// // ============================================================================
// // TramasMicros3 - Implementación
// // ============================================================================

// TramasMicros3::TramasMicros3(unsigned long intervl, void (*function)(void))
//   : active(true), previous(0), interval(intervl), execute(function) {
// }

// TramasMicros3::TramasMicros3(unsigned long prev, unsigned long intervl, void (*function)(void), bool enable)
//   : active(enable), previous(prev), interval(intervl), execute(function) {
// }

// void TramasMicros3::reset() {
//   previous = SystemTimer::getMicros();
// }

// void TramasMicros3::disable() {
//   active = false;
// }

// void TramasMicros3::enable() {
//   active = true;
// }

// void TramasMicros3::setInterval(unsigned long intervl) {
//   interval = intervl;
// }

// void TramasMicros3::check() {
//   if (active && (SystemTimer::getMicros() - previous >= interval)) {
//     previous = SystemTimer::getMicros();
//     if (execute) {
//       execute();
//     }
//   } else if (active && SystemTimer::getMicros() < previous) {
//     unsigned long elapsed = (0xFFFFFFFF - previous) + SystemTimer::getMicros() + 1;
//     if (elapsed >= interval) {
//       previous = SystemTimer::getMicros();
//       if (execute) {
//         execute();
//       }
//     }
//   }
// }

// bool TramasMicros3::isDue() {
//   if (!active) {
//     return false;
//   }
//   unsigned long now = SystemTimer::getMicros();
//   if (now - previous >= interval) {
//     previous = now;
//     return true;
//   }
//   if (now < previous) {
//     unsigned long elapsed = (0xFFFFFFFF - previous) + now + 1;
//     if (elapsed >= interval) {
//       previous = now;
//       return true;
//     }
//   }
//   return false;
// }

// // ============================================================================
// // TaskScheduler - Implementación
// // ============================================================================

// TramasMicros3 *TaskScheduler::tasks[MAX_TIMED_TASKS] = { nullptr };
// uint8_t TaskScheduler::taskCount = 0;

// void TaskScheduler::registerTask(TramasMicros3 *task) {
//   if (taskCount < MAX_TIMED_TASKS) {
//     tasks[taskCount++] = task;
//   }
// }

// bool TaskScheduler::removeTask(TramasMicros3 *task) {
//   for (uint8_t i = 0; i < taskCount; i++) {
//     if (tasks[i] == task) {
//       tasks[i] = nullptr;  // Marcar como eliminado (hueco)
//       return true;
//     }
//   }
//   return false;
// }

// void TaskScheduler::checkAll() {
//   for (uint8_t i = 0; i < taskCount; i++) {
//     if (tasks[i]) {  // Solo ejecuta tareas no eliminadas
//       tasks[i]->check();
//     }
//   }
// }
