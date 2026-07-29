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
// SystemTimer - Implementación (Timer1, CTC, prescaler configurable)
// ============================================================================

// Prescalers VÁLIDOS para Timer1 (16 bits): 1, 8, 64, 256, 1024
// (32, 128, 512... son válidos para Timer2, NO para Timer1)
#ifndef TIMER_PRESCALER
#define TIMER_PRESCALER 1
#endif

// Selección de bits CS12:CS10 según el prescaler elegido
#if TIMER_PRESCALER == 1
#define TIMER1_CS_BITS ((1 << CS10))
#elif TIMER_PRESCALER == 8
#define TIMER1_CS_BITS ((1 << CS11))
#elif TIMER_PRESCALER == 64
#define TIMER1_CS_BITS ((1 << CS11) | (1 << CS10))
#elif TIMER_PRESCALER == 256
#define TIMER1_CS_BITS ((1 << CS12))
#elif TIMER_PRESCALER == 1024
#define TIMER1_CS_BITS ((1 << CS12) | (1 << CS10))
#else
#error "TIMER_PRESCALER invalido para Timer1. Usa 1, 8, 64, 256 o 1024."
#endif

// Ticks de Timer1 por microsegundo, con el prescaler elegido
#define TICKS_PER_MICROSECOND (F_CPU / 1000000UL / TIMER_PRESCALER)

#if TICKS_PER_MICROSECOND == 0
#error "TIMER_PRESCALER demasiado alto para F_CPU: TICKS_PER_MICROSECOND=0 (division por cero en runtime)."
#endif

// OCR1A para generar interrupción cada 1000 µs (1 ms) exactos
#define OCR1A_VALUE ((F_CPU / 1000UL / TIMER_PRESCALER) - 1)

#if ((F_CPU / 1000UL) % TIMER_PRESCALER) != 0
#warning "F_CPU/1000 no es multiplo exacto de TIMER_PRESCALER: el periodo de 1ms tendra error de redondeo."
#endif



// // ============================================================================
// // SystemTimer - Implementación (Timer1, CTC, prescaler 1, OCR1A ajustado)
// // ============================================================================

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

  // Modo CTC (WGM12) + Bits del Prescaler seleccionados dinámicamente
  TCCR1B = (1 << WGM12) | TIMER1_CS_BITS;

  // OCR1A calculado exactamente para 1 ms (1000 µs)
  OCR1A = OCR1A_VALUE;

  // Habilitar interrupción por comparación A
  TIMSK1 = (1 << OCIE1A);

  sei();

#ifdef DEBUG_TIMER
  Serial.print(F("Timer1 configurado: prescaler="));
  Serial.print(TIMER_PRESCALER);
  Serial.print(F(", OCR1A="));
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

  // Si hay interrupción pendiente y TCNT1 se reinició a 0,
  // sumamos el intervalo de 1 ms completo que aún no se procesó en el ISR
  if ((TIFR1 & (1 << OCF1A)) && (tcnt < (OCR1A / 2))) {
    base += 1000;
  }
  SREG = oldSREG;

  // Fórmula universal precisa: µs = (ticks * prescaler) / (MHz)
  // Ejemplo a 16MHz y Prescaler 8: (tcnt * 8) / 16  ==> tcnt / 2
  uint32_t micros_fraction = ((uint32_t)tcnt * TIMER_PRESCALER) / (F_CPU / 1000000UL);

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


// // STOP Timer1 (TCCR1B)
// TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
//
// // Prescaler 1 Timer1 (TCCR1B)
// TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
// TCCR1B |= (1<<CS10);
//
// // Prescaler 8 Timer1 (TCCR1B)
// TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
// TCCR1B |= (1<<CS11);
//
// // Prescaler 64 Timer1 (TCCR1B)
// TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
// TCCR1B |= (1<<CS11)|(1<<CS10);
//
// // Prescaler 256 Timer1 (TCCR1B)
// TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
// TCCR1B |= (1<<CS12);
//
// // Prescaler 1024 Timer1 (TCCR1B)
// TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
// TCCR1B |= (1<<CS12)|(1<<CS10);
//
// STOP Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
//
// // Prescaler 1 Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
// TCCR2B |= (1<<CS20);
//
// // Prescaler 8 Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
// TCCR2B |= (1<<CS21);
//
// // Prescaler 32 Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
// TCCR2B |= (1<<CS21)|(1<<CS20);
//
// // Prescaler 64 Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
// TCCR2B |= (1<<CS22);
//
// // Prescaler 128 Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
// TCCR2B |= (1<<CS22)|(1<<CS20);
//
// // Prescaler 256 Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
// TCCR2B |= (1<<CS22)|(1<<CS21);
//
// // Prescaler 1024 Timer2 (TCCR2B)
// TCCR2B &= ~((1<<CS22)|(1<<CS21)|(1<<CS20));
// TCCR2B |= (1<<CS22)|(1<<CS21)|(1<<CS20);
//
// STOP Timer0 (TCCR0B)
// TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00));
//
// // Prescaler 1 Timer0 (TCCR0B)
// TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00));
// TCCR0B |= (1<<CS00);
//
// // Prescaler 8 Timer0 (TCCR0B)
// TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00));
// TCCR0B |= (1<<CS01);
//
// // Prescaler 64  Timer0 (TCCR0B)
// TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00));
// TCCR0B |= (1<<CS01)|(1<<CS00);
//
// // Prescaler 256 Timer0 (TCCR0B)
// TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00));
// TCCR0B |= (1<<CS02);
//
// // Prescaler 1024 Timer0 (TCCR0B)
// TCCR0B &= ~((1<<CS02)|(1<<CS01)|(1<<CS00));
// TCCR0B |= (1<<CS02)|(1<<CS00);
//
// Timer0: 8 bits (Valor máximo: 255)
// Timer1: 16 bits (Valor máximo: 65,535)
// Timer2: 8 bits (Valor máximo: 255)
// ADC: 10 bits (Valor máximo: 1,023)
//
//Timer2 (TCCR2B) 16bis
// Prescaler 8:    TCCR2B |= (1<<CS21);
// Prescaler 32:   TCCR2B |= (1<<CS22) | (1<<CS20);
// Prescaler 64:   TCCR2B |= (1<<CS22) | (1<<CS21);
// Prescaler 128:  TCCR2B |= (1<<CS22) | (1<<CS21) | (1<<CS20);
// Prescaler 256:  TCCR2B |= (1<<CS22);
// Prescaler 1024: TCCR2B |= (1<<CS22) | (1<<CS21);Timer1 (TCCR1B)Prescaler 1:    TCCR1B |= (1<<CS10);
//Timer1 (TCCR1B) 8bits
// Prescaler 8:    TCCR1B |= (1<<CS11);
// Prescaler 64:   TCCR1B |= (1<<CS11) | (1<<CS10);
// Prescaler 256:  TCCR1B |= (1<<CS12);
// Prescaler 1024: TCCR1B |= (1<<CS12) | (1<<CS10);Timer0 (TCCR0B)Prescaler 1:    TCCR0B |= (1<<CS00);
//Timer0 (TCCR0B) 8bits
// Prescaler 8:    TCCR0B |= (1<<CS01);
// Prescaler 64:   TCCR0B |= (1<<CS01) | (1<<CS00);
// Prescaler 256:  TCCR0B |= (1<<CS02);
// Prescaler 1024: TCCR0B |= (1<<CS02) | (1<<CS00);ADC (ADCSRA)División 2:   ADCSRA |= (1<<ADPS0);
//ADC (ADCSRA)  10bis
// División 4:   ADCSRA |= (1<<ADPS1);
// División 8:   ADCSRA |= (1<<ADPS1) | (1<<ADPS0);
// División 16:  ADCSRA |= (1<<ADPS2);
// División 32:  ADCSRA |= (1<<ADPS2) | (1<<ADPS0);
// División 64:  ADCSRA |= (1<<ADPS2) | (1<<ADPS1);
// División 128: ADCSRA |= (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);

