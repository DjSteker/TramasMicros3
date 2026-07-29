

# TramasMicros3

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: AVR](https://img.shields.io/badge/platform-AVR-blue)](https://www.arduino.cc/)

**Librería de temporización y planificación de tareas para microcontroladores AVR (Arduino Uno, Mega, Nano...)**  
Proporciona un temporizador de alta precisión por interrupción (basado en Timer1) y un sistema de tareas programadas con intervalos configurables, ideal para aplicaciones con múltiples procesos concurrentes sin usar `delay()`.

---

## ✨ Características

- **`SystemTimer`** – Temporizador hardware de 1 kHz (resolución de 1 µs) que no interfiere con `millis()` ni `micros()` de Arduino.
- **`TramasMicros3`** – Clase para definir tareas con intervalo en microsegundos. Soporta control de ejecución (`enable`/`disable`), reinicio y verificación de vencimiento.
- **`TaskScheduler`** – Planificador cooperativo que registra y ejecuta automáticamente todas las tareas registradas.
- **Eficiente** – Sin uso de `delay()`, solo comprueba los tiempos cuando se llama a `checkAll()`.
- **Fácil de integrar** – Solo necesitas incluir la librería y colocar `TaskScheduler::checkAll()` en el `loop()`.

---

## 📦 Instalación

### Desde el IDE de Arduino
1. Descarga este repositorio como ZIP.
2. En el IDE, ve a **Sketch → Include Library → Add .ZIP Library** y selecciona el archivo descargado.
3. Reinicia el IDE.

### Manual
Copia la carpeta `TramasMicros3` dentro de `~/Arduino/libraries/` (o la ruta correspondiente).

---

## 🚀 Uso rápido

```cpp
#include <TramasMicros3.h>

TramasMicros3 tarea1(2000000, []() {
  Serial.println("Tarea cada 2 segundos");
}); // intervalo en microsegundos

TramasMicros3 tarea2(500000, []() {
  Serial.println("Tarea cada 0.5 segundos");
});

void setup() {
  Serial.begin(115200);
  SystemTimer::init();
  TaskScheduler::registerTask(&tarea1);
  TaskScheduler::registerTask(&tarea2);
}

void loop() {
  TaskScheduler::checkAll();   // Ejecuta las tareas que correspondan
}
```

---

## 📚 API

### `SystemTimer`
| Método | Descripción |
|--------|-------------|
| `static void init()` | Inicializa el Timer1 en modo CTC. Debe llamarse una sola vez al inicio. |
| `static unsigned long getMicros()` | Devuelve el tiempo en microsegundos desde `init()`. |
| `static uint32_t getMillis()` | Devuelve el tiempo en milisegundos. |

### `TramasMicros3`
| Método | Descripción |
|--------|-------------|
| `TramasMicros3(intervalo, funcion)` | Crea una tarea activa con el intervalo (en µs) y la función a ejecutar. |
| `TramasMicros3(previo, intervalo, funcion, activo)` | Constructor avanzado con tiempo previo y estado inicial. |
| `void reset()` | Reinicia el contador de tiempo. |
| `void enable()` / `void disable()` | Activa/desactiva la tarea. |
| `void check()` | Ejecuta la función si el intervalo ha vencido. |
| `void setInterval(µs)` | Cambia el intervalo. |
| `bool isDue()` | Devuelve `true` si la tarea debe ejecutarse y actualiza el tiempo interno. |

### `TaskScheduler`
| Método | Descripción |
|--------|-------------|
| `static void registerTask(TramasMicros3*)` | Registra una tarea (máximo `MAX_TIMED_TASKS`, por defecto 4). |
| `static void checkAll()` | Verifica y ejecuta todas las tareas registradas. |

---

## ⚙️ Compatibilidad

- Placas basadas en **AVR**: Arduino Uno, Nano, Mega, Leonardo…
- No compatible con ESP32, ESP8266 o ARM (usa registros específicos de AVR).

---

## 📄 Licencia

Este proyecto está bajo la licencia **MIT** – consulta el archivo `LICENSE` para más detalles.

---

## 🤝 Contribuciones

Pull requests, reportes de bugs y sugerencias son bienvenidos. Si quieres añadir soporte para otras arquitecturas o nuevas funcionalidades, no dudes en colaborar.

---

**Autor:** DjSteker  
**Repositorio:** [github.com/DjSteker/TramasMicros3](https://github.com/DjSteker/TramasMicros3) (ejemplo)

---

