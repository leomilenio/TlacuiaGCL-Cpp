# Backlog — Auditoría de Compatibilidad macOS/Windows
> Generado: 2026-03-22 | Sprint de origen: PDF + Folios
> Los puntos 1 (fuente PDF) y 2 (WAL/crash safety) fueron resueltos en ese sprint.
> Los puntos 3-6 (Alta prioridad) fueron resueltos el 2026-03-22.

---

## ✅ ALTA PRIORIDAD — COMPLETADO

### ~~[COMPAT-03] Validación explícita de certificados SSL en actualizaciones~~
> **Resuelto 2026-03-22** — `QSslConfiguration::defaultConfiguration()` con `setPeerVerifyMode(QSslSocket::VerifyPeer)` aplicado explícitamente al request. Se agregó `#include <QSslConfiguration>`.

### ~~[COMPAT-04] Mensajes de error de red dependientes del SO~~
> **Resuelto 2026-03-22** — Función `networkErrorToSpanish(QNetworkReply*)` mapea los `QNetworkReply::NetworkError` más comunes a mensajes en español. `reply->errorString()` ya no se muestra al usuario.

### ~~[COMPAT-05] Construcción de rutas con `/` hardcodeado~~
> **Resuelto 2026-03-22** — `dataDir + "/data.db"` reemplazado por `QDir(dataDir).filePath("data.db")` en `AppConfig.cpp`.

### ~~[COMPAT-06] Manejo de extensiones de archivo en diálogos de backup~~
> **Resuelto 2026-03-22** — Validación post-selección con `QFileInfo::suffix().toLower()` para `.db/.sqlite` y `.sha256`. `QTextStream` en `guardarHashFile` y `leerHashDesdeArchivo` usa `setEncoding(QStringConverter::Utf8)` explícito.

---

## 🟡 PRIORIDAD MEDIA — COMPLETADO

### ~~[COMPAT-07] Flags de compilación C++20 incompletos en MSVC~~
> **Resuelto 2026-03-22** — Agregados `/Zc:__cplusplus` (reporta `__cplusplus = 202002L` correctamente en MSVC) y `/utf-8` (fuente y ejecución en UTF-8, evita corrupción de tildes/ñ con locales Windows como cp1252). Ambos flags están dentro del bloque `if(MSVC)`, sin efecto en Clang/GCC.

### ~~[COMPAT-08] Seguridad de hilos en QSqlDatabase no documentada~~
> **Resuelto 2026-03-22** — Comentario "NOTA DE THREAD SAFETY" completo en `DatabaseManager.h`. En `.cpp`: `m_ownerThread = QThread::currentThread()` en el constructor; `Q_ASSERT_X(QThread::currentThread() == m_ownerThread, ...)` en `database()`. El assert dispara en builds Debug si se accede desde otro hilo, sin overhead en Release.

### ~~[COMPAT-09] High-DPI / Retina sin reconciliar en exportadores PDF~~
> **Resuelto 2026-03-22** — `doc.documentLayout()->setPaintDevice(&printer)` se llama ANTES de `setHtml()` y `setPageSize()` en ambos exportadores. Esto fuerza a `QTextDocument` a usar el DPI del printer (1200 dpi) para todo el layout, haciendo el `pageCount()` y las coordenadas independientes del `devicePixelRatio` de la pantalla. `Q_ASSERT(paintDevice() != nullptr)` añadido como guardia de regresión.

### ~~[COMPAT-10] Descubrimiento de Qt6 en Windows con múltiples instalaciones~~
> **Resuelto 2026-03-22** — `find_package(Qt6 6.4 REQUIRED ...)` en `cmake/FindDependencies.cmake` especifica versión mínima. Comentario en el mismo archivo documenta el uso de `-DQt6_DIR` para override en Windows con múltiples instalaciones.

---

## 🟢 BAJA PRIORIDAD / DEUDA TÉCNICA

### [COMPAT-11] Firma de código macOS solo ad-hoc — no apta para distribución
**Archivo:** `app/CMakeLists.txt`
**Descripción:** La firma post-build usa `--sign "-"` (ad-hoc). Esto es correcto para desarrollo
pero **Gatekeeper rechaza apps ad-hoc en macOS 12+** cuando provienen de fuentes externas.
Para distribución pública se requiere Developer ID ($99/año en Apple Developer Program).
**Propuesta:** Documentar que el build actual es solo para desarrollo/testing local.
Para distribución: usar Developer ID + `codesign --sign "Developer ID Application: ..."` +
notarización con `notarytool`.

---

### [COMPAT-12] Strings de UI hardcodeados en español — sin soporte de localización
**Archivos:** Todos los `.cpp` del directorio `app/src/`
**Descripción:** No existe ningún archivo `.ts` (Qt Linguist) ni llamadas a `tr()` en la mayoría
de los strings. Esto no afecta la compatibilidad actual pero bloquea cualquier futura
localización a inglés u otro idioma.
**Propuesta (largo plazo):** Envolver strings visibles en `tr()`. Crear un archivo
`tlacuia_es_MX.ts` como base. No es urgente si el mercado objetivo es solo hispanohablante.

---

### [COMPAT-13] Lifecycle de QNetworkAccessManager
**Archivo:** `app/src/UpdateDialog.cpp`
**Descripción:** `m_nam` es hijo del diálogo (`new QNetworkAccessManager(this)`), por lo que su
destrucción está atada al diálogo. Correcto en la arquitectura actual, pero si el diálogo se
destruye con una request en vuelo, puede generar un use-after-free en builds de Debug.
**Propuesta:** Cancelar requests pendientes en el destructor del diálogo:
```cpp
UpdateDialog::~UpdateDialog() {
    if (m_reply) m_reply->abort();
}
```

---

## Resuelto en sprint actual

| ID | Descripción | Sprint |
|----|-------------|--------|
| COMPAT-01 | Fuente Arial hardcodeada en footer PDF | Sprint PDF+Folios |
| COMPAT-02 | Arquitectura WAL SQLite para crash safety local | Sprint PDF+Folios |
