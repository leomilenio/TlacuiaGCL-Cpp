# Roadmap — Gestor de Concesiones Tlacuia

## ✅ Sprint 2 — Inventario por Concesión y Corte con PDF [COMPLETADO 2026-03-18]

### Contexto

Actualmente el sistema registra productos calculados con precio neto y precio final, pero no
rastrea **cuántas piezas** se recibieron ni cuántas se vendieron. Esto impide generar un corte
preciso al cierre de una concesión, ya que el corte debe reflejar solamente las unidades vendidas
y calcular el reintegro al distribuidor por las unidades devueltas.

---

### Objetivo del Sprint

Al terminar este sprint, el flujo completo de una concesión debe ser:

1. **Recepción**: el librero registra un producto con `cantidad_recibida = N`.
2. **Venta parcial**: al hacer el corte declara `cantidad_vendida = K` (donde `K ≤ N`).
3. **Corte**: el sistema calcula el pago al distribuidor (`K × costo`) y la devolución
   (`(N - K)` piezas), genera un PDF con el resumen.

---

### Historias de Usuario

#### US-1: Campo `cantidad` al agregar un producto a una concesión

**Como** encargado de librería,
**quiero** poder indicar cuántas piezas de un producto recibí en una concesión,
**para** tener el inventario correcto al momento de hacer el corte.

**Criterios de aceptación:**
- `AgregarProductoDialog` incluye un campo `QSpinBox` "Cantidad recibida" (mínimo 1, sin máximo razonable).
- El valor se guarda como columna `cantidad_recibida INTEGER NOT NULL DEFAULT 1` en `productos_calculados`.
- El panel de detalle de `ConcesionesWidget` muestra la columna "Qty" en la mini-tabla de productos.
- Migración de DB: V4 → V5 agrega `cantidad_recibida` y `cantidad_vendida` a `productos_calculados`.

---

#### US-2: Declarar unidades vendidas al generar el corte

**Como** encargado de librería,
**quiero** poder indicar cuántas piezas de cada producto vendí antes de imprimir el corte,
**para** que el sistema calcule los totales exactos (vendido vs. a devolver).

**Criterios de aceptación:**
- El diálogo de corte ("Ver Corte") muestra una tabla editable con columnas:
  `Producto | Qty Recibida | Qty Vendida (editable) | Qty Devuelta | Subtotal`.
- El campo `Qty Vendida` es un `QSpinBox` con rango `[0, cantidad_recibida]`.
- Los totales se recalculan en tiempo real al editar las cantidades.
- Al confirmar, se actualiza `cantidad_vendida` en `productos_calculados` para cada fila.
- El `CorteResult` en `ProductoRepository` se extiende con:
  - `totalUnidadesRecibidas`, `totalUnidadesVendidas`, `totalUnidadesDevueltas`
  - `totalPagoAlDistribuidor` = `SUM(costo × cantidad_vendida)`
  - `totalDevolucion` = `SUM(costo × cantidad_devuelta)`

---

#### US-3: Exportar corte a PDF

**Como** encargado de librería,
**quiero** generar un PDF con el resumen del corte,
**para** entregárselo al distribuidor como comprobante del cierre de la concesión.

**Criterios de aceptación:**
- Botón "Exportar PDF" en el diálogo de corte.
- El PDF incluye:
  - Encabezado: nombre de la aplicación, fecha, nombre del distribuidor, folio de la concesión.
  - Tabla de productos: Descripción, ISBN (si aplica), Qty Recibida, Qty Vendida, Precio Neto,
    Subtotal Vendido, Qty Devuelta.
  - Sección de totales: Total vendido (MXN), Total a pagar al distribuidor, Total devoluciones.
  - Sección fiscal: IVA Trasladado, IVA Acreditable (si aplica), IVA Neto a SAT.
  - Pie: firma del librero y del distribuidor (espacio en blanco para firma física).
- El usuario elige la ruta de guardado con `QFileDialog`.
- Implementación: `QPrinter` + `QPainter` o `QTextDocument::print()` (sin dependencias externas).
- El nombre del archivo sugerido: `Corte_<Distribuidor>_<Folio>_<Fecha>.pdf`.

---

### Cambios Técnicos Detallados

#### DB — Migración V4 → V5

```sql
-- productos_calculados: añadir columnas de inventario
ALTER TABLE productos_calculados ADD COLUMN cantidad_recibida INTEGER NOT NULL DEFAULT 1;
ALTER TABLE productos_calculados ADD COLUMN cantidad_vendida  INTEGER NOT NULL DEFAULT 0;

-- Índice para consultas de corte
CREATE INDEX IF NOT EXISTS idx_productos_concesion
    ON productos_calculados(concesion_id, cantidad_vendida);
```

Archivo: `core/src/DatabaseManager.cpp` — añadir `migrateV4toV5()` y actualizar
`SCHEMA_VERSION_CURRENT` a 5.

---

#### `ProductoRecord` — nuevos campos

```cpp
// core/include/core/ProductoRepository.h
struct ProductoRecord {
    // ... campos existentes ...
    int cantidadRecibida = 1;   // piezas recibidas en la concesión
    int cantidadVendida  = 0;   // piezas declaradas vendidas al hacer el corte
    // cantidadDevuelta es calculado: cantidadRecibida - cantidadVendida
};
```

---

#### `CorteResult` — campos extendidos

```cpp
struct CorteResult {
    // ... campos existentes ...
    int    totalUnidadesRecibidas    = 0;
    int    totalUnidadesVendidas     = 0;
    int    totalUnidadesDevueltas    = 0;
    double totalPagoAlDistribuidor   = 0.0;  // SUM(costo × cantidad_vendida)
    double totalDevolucion           = 0.0;  // SUM(costo × cantidad_devuelta)
};
```

`calcularCorte()` en `ProductoRepository.cpp` extiende la query:
```sql
SELECT
    SUM(cantidad_recibida)                          AS sum_qty_rec,
    SUM(cantidad_vendida)                           AS sum_qty_vend,
    SUM(cantidad_recibida - cantidad_vendida)        AS sum_qty_dev,
    SUM(costo * cantidad_vendida)                   AS sum_pago_dist,
    SUM(costo * (cantidad_recibida - cantidad_vendida)) AS sum_devolucion,
    SUM(precio_final * cantidad_vendida)            AS sum_precio_final,
    -- ... resto igual ...
FROM productos_calculados WHERE concesion_id = :cid
```

---

#### `AgregarProductoDialog` — campo cantidad

Añadir un `QSpinBox` "Cantidad recibida" con rango `[1, 9999]` entre el selector de CFDI y el
precio neto. El valor se pasa al `ProductoRecord` al aceptar.

---

#### `CorteDialog` — nueva clase

Reemplaza el `QDialog` inline en `ConcesionesWidget::onVerCorteClicked()`.

```
app/include/app/CorteDialog.h
app/src/CorteDialog.cpp
```

Layout:
```
┌──────────────────────────────────────────────────────────────────────┐
│  Corte de Concesión — ACME Editorial  F-001  (2026-03-18)           │
│  ──────────────────────────────────────────────────────────────────  │
│  Producto         │ Rec │ Vend (editar) │ Dev │ Precio Neto │ Total  │
│  ──────────────── │ ─── │ ──────────── │ ─── │ ─────────── │ ─────  │
│  El principito    │  5  │ [3 ▲▼]       │  2  │  $54.00     │ $162   │
│  Don Quijote      │ 10  │ [8 ▲▼]       │  2  │  $80.00     │ $640   │
│  ──────────────────────────────────────────────────────────────────  │
│  Total a pagar al distribuidor:  $802.00                             │
│  Total devoluciones (piezas):    4                                   │
│  ──────────────────────────────────────────────────────────────────  │
│  IVA Trasladado:   $XXX  │  IVA Acreditable: $XXX  │  SAT: $XXX     │
│                                                                      │
│                              [Exportar PDF]  [Confirmar]  [Cancelar] │
└──────────────────────────────────────────────────────────────────────┘
```

Al "Confirmar":
1. Actualizar `cantidad_vendida` en cada fila de `productos_calculados`.
2. Llamar `concesionRepo.finalizar(id)` para cerrar la concesión.

Al "Exportar PDF":
1. Abrir `QFileDialog` para elegir destino.
2. Generar PDF con `QTextDocument` y `QPrinter`.

---

#### Generador de PDF — `CortePdfExporter`

```
app/include/app/CortePdfExporter.h
app/src/CortePdfExporter.cpp
```

Función principal:
```cpp
namespace App {
class CortePdfExporter {
public:
    static bool exportar(const Calculadora::ConcesionRecord& concesion,
                         const QList<Calculadora::ProductoRecord>& productos,
                         const Calculadora::CorteResult& corte,
                         const QString& filePath);
};
}
```

Implementación con `QTextDocument`:
- Genera HTML interno con tablas CSS simples.
- Imprime a archivo con `QPrinter::PdfFormat`.
- Qt 6 ya incluye `Qt6::PrintSupport` — añadir al `target_link_libraries` en `app/CMakeLists.txt`.

---

### CMakeLists.txt — cambios requeridos

```cmake
# app/CMakeLists.txt — añadir al add_executable:
include/app/CorteDialog.h
include/app/CortePdfExporter.h
src/CorteDialog.cpp
src/CortePdfExporter.cpp

# Añadir dependencia Qt6::PrintSupport:
target_link_libraries(CalculadoraPapeleria
    PRIVATE core_lib Qt6::Widgets Qt6::PrintSupport
)
```

---

### Orden de implementación

1. Migración V4→V5 (`DatabaseManager`)
2. Extender `ProductoRecord` y `ProductoRepository` (campos + query extendida)
3. Extender `CorteResult`
4. Actualizar `AgregarProductoDialog` (spinner de cantidad)
5. Crear `CorteDialog` (tabla editable + totales en tiempo real)
6. Crear `CortePdfExporter`
7. Sustituir el `QDialog` inline de `onVerCorteClicked` por `CorteDialog`
8. Añadir `Qt6::PrintSupport` al CMakeLists
9. Tests unitarios para la query extendida de `calcularCorte`

---

### Fuera de scope de este sprint

- Generación de CFDI digital (requiere integración con PAC — sprint independiente)
- Notificaciones por email o WhatsApp al distribuidor
- Historial de cortes pasados (los registros quedan en DB pero no hay vista dedicada aún)
- Inventario en tiempo real durante la concesión (alerta de stock bajo)

---

## ✅ Sprint 3 — Panel de Notificaciones al Inicio (Port de TlacuiaGCL) [COMPLETADO 2026-03-18]

### Contexto

En el TlacuiaGCL original (Python/PyQt5) existía una **vista de arranque dedicada** cuya única
función era presentarle al usuario el estado de todas sus concesiones activas antes de entrar al
programa principal. Esta vista funcionaba como un tablero de control de alertas: de un vistazo el
usuario sabía qué concesiones requerían atención inmediata, cuáles estaban en orden y cuáles
estaban próximas a vencer.

El comportamiento actual (un `AlertDialog` modal que solo muestra las concesiones problemáticas)
es una implementación parcial. Este sprint porta el sistema completo.

---

### Objetivo del Sprint

Reemplazar el `AlertDialog` modal de arranque por una **ventana de inicio dedicada**
(`SplashConcesionesWindow`) que:

1. Muestra **todas** las concesiones activas, agrupadas por estado.
2. Permite al usuario revisar la situación antes de entrar al programa principal.
3. Ofrece acceso directo a la concesión seleccionada al entrar.
4. Cierra y abre `MainWindow` al presionar "Continuar".

---

### Historias de Usuario

#### US-4: Vista de arranque con estado de concesiones

**Como** encargado de librería,
**quiero** ver al abrir la aplicación un resumen visual del estado de todas mis concesiones activas,
**para** saber de inmediato cuáles requieren atención sin tener que navegar por el programa.

**Criterios de aceptación:**
- La `SplashConcesionesWindow` se muestra **antes** de `MainWindow`.
- Las concesiones se presentan en tres secciones ordenadas por urgencia:
  1. **Vencidas** (rojo) — concesiones activas cuya fecha de vencimiento ya pasó.
  2. **Vence pronto** (amarillo) — vencen en los próximos 14 días.
  3. **Vigentes** (verde) — sin urgencia.
- Cada tarjeta de concesión muestra: nombre del distribuidor, folio, fecha de vencimiento,
  días restantes (o "hace N días" si ya venció).
- Si no hay concesiones activas, la vista muestra un mensaje de bienvenida y el botón
  "Continuar" directamente.
- Botón "Continuar al programa" cierra la splash y abre `MainWindow`.
- Botón "Ir a esta concesión" en cada tarjeta: abre `MainWindow` y navega directamente a la
  pestaña Concesiones con esa concesión seleccionada.

---

#### US-5: Indicador de alertas en tiempo real dentro de MainWindow

**Como** encargado de librería,
**quiero** un indicador visual persistente en la barra de estado que me recuerde si tengo
concesiones con alertas activas,
**para** no perder de vista situaciones urgentes mientras trabajo en otras pestañas.

**Criterios de aceptación:**
- La `QStatusBar` de `MainWindow` muestra un label persistente con el conteo:
  - `"⚠ 2 concesiones vencidas · 1 próxima a vencer"` (con ícono y color semántico).
  - `"✓ Todas las concesiones al día"` cuando no hay alertas.
- El label se actualiza cada vez que se modifica o finaliza una concesión.
- Al hacer clic en el label, navega a la pestaña Concesiones.

---

### Cambios Técnicos Detallados

#### Nueva clase `SplashConcesionesWindow`

```
app/include/app/SplashConcesionesWindow.h
app/src/SplashConcesionesWindow.cpp
```

- Hereda de `QWidget` (ventana independiente, no `QDialog`, para poder tener su propio ciclo
  de vida antes de que `MainWindow` exista).
- Recibe `ConcesionRepository&` en el constructor.
- Emite `continuar(int64_t concesionSeleccionadaId)` al cerrar; `id = 0` si no se seleccionó
  ninguna.

```cpp
class SplashConcesionesWindow : public QWidget {
    Q_OBJECT
public:
    explicit SplashConcesionesWindow(Calculadora::ConcesionRepository& repo,
                                     QWidget* parent = nullptr);
signals:
    void continuar(int64_t concesionSeleccionadaId);
};
```

**Layout propuesto:**
```
┌───────────────────────────────────────────────────────────┐
│  Gestor de Concesiones Tlacuia          [logo / ícono]    │
│  ─────────────────────────────────────────────────────    │
│  ● VENCIDAS (2)                                           │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ ACME Editorial — F-001  │  Venció hace 3 días  [→] │  │
│  │ Fondo de Cultura — F-012│  Venció hace 1 día   [→] │  │
│  └─────────────────────────────────────────────────────┘  │
│  ● VENCE PRONTO (1)                                       │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ Santillana — F-007      │  Vence en 5 días     [→] │  │
│  └─────────────────────────────────────────────────────┘  │
│  ● VIGENTES (4)                                           │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ Penguin — F-003         │  42 días restantes   [→] │  │
│  │ ...                                                 │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                           │
│                            [Continuar al programa  →]     │
└───────────────────────────────────────────────────────────┘
```

#### Flujo en `main.cpp`

```cpp
// Antes de crear MainWindow:
auto dbManager     = std::make_unique<Calculadora::DatabaseManager>(config.dbPath);
dbManager->initialize();
auto concesionRepo = std::make_unique<Calculadora::ConcesionRepository>(*dbManager);

App::SplashConcesionesWindow splash(*concesionRepo);
int64_t concesionInicial = 0;
QObject::connect(&splash, &App::SplashConcesionesWindow::continuar,
    [&](int64_t id) { concesionInicial = id; });
splash.show();
app.exec();   // loop hasta que splash emita continuar y se cierre

// Lanzar MainWindow pasando el dbManager ya inicializado y el id de concesión inicial
App::MainWindow mainWindow(std::move(dbManager), std::move(concesionRepo), concesionInicial);
mainWindow.show();
return app.exec();
```

> **Nota para el LLM ejecutor**: Este cambio requiere refactorizar `MainWindow` para aceptar
> un `DatabaseManager` ya inicializado (en lugar de crearlo internamente), o bien usar un
> patrón de dos fases donde la splash pasa el `dbPath` y `MainWindow` re-abre la misma DB
> usando el mismo `connectionName`. Evaluar qué opción genera menos cambio de superficie.

#### Indicador en `QStatusBar`

En `MainWindow`:
- Nuevo `QLabel* m_alertasLabel` en la `statusBar()`.
- Método privado `refreshAlertaStatus()` que consulta `findVencenPronto(14)` y
  `findAll()` para contar vencidas.
- Llamar `refreshAlertaStatus()` desde `setupCentralWidget()` y cada vez que
  `ConcesionesWidget` modifique datos (nueva señal `concesionesModificadas()`).
- El label es clickeable vía `QLabel::linkActivated` o `mousePressEvent` para navegar a la
  pestaña Concesiones.

---

### Referencia en TlacuiaGCL

El sistema original se encuentra en:
- `app/views/splash_window.py` — `SplashWindow(QWidget)` con la lógica de agrupación
- `app/views/components/concession_item.py` — `ConcesionItem` (tarjeta reutilizable)
- `app/controllers/splash_controller.py` — lógica de carga y navegación

Repo: `https://github.com/leomilenio/TlacuiaGCL`

---

### Fuera de scope de este sprint

- Notificaciones push del sistema operativo (macOS `NSUserNotification` / Windows toast)
- Sonido o vibración de alerta
- Configuración del umbral de "vence pronto" (actualmente hardcodeado a 14 días)

---

## ✅ Sprint 4 — Hoja de Estilos y Tema Visual [COMPLETADO 2026-03-18]

### Contexto

La interfaz actual usa los controles nativos de Qt sin personalización, lo que resulta funcional
pero poco atractivo. Este sprint define y aplica una hoja de estilos coherente con la identidad
de **Somos Voces / Tlacuia**, manteniendo compatibilidad con macOS y Windows y respetando el
modo oscuro del sistema.

---

### Objetivo del Sprint

Aplicar una hoja de estilos QSS que:

1. Da identidad visual propia al producto sin abandonar los controles nativos Qt.
2. Funciona correctamente en modo claro y oscuro en macOS y Windows.
3. Es mantenible: un solo archivo `.qss` cargado desde recursos Qt (`.qrc`).

---

### Historias de Usuario

#### US-6: Hoja de estilos base

**Como** encargado de librería,
**quiero** que la aplicación tenga una apariencia visual coherente y profesional,
**para** que sea agradable de usar en el día a día.

**Criterios de aceptación:**
- Archivo `app/resources/styles/tlacuia.qss` cargado en `main.cpp` vía `QFile` + `app.setStyleSheet(...)`.
- Los siguientes elementos tienen estilo personalizado:
  - `QTabWidget::tab-bar` — pestañas con tipografía clara, indicador de pestaña activa.
  - `QPushButton` — radio de borde 4px, padding consistente; variante primaria (accent color).
  - `QGroupBox` — borde y título con tipografía ligeramente menor.
  - `QListWidget` / `QTableView` — filas alternadas, sin grid visible, highlight coherente.
  - Badges de status (`QLabel` con objectName) — verde/amarillo/rojo con texto blanco/negro.
  - `SplashConcesionesWindow` — fondo levemente diferenciado por sección de urgencia.
- No se hardcodean colores absolutos excepto los tres semánticos de badge (ya establecidos).
- Se usa `palette(...)` para todos los tonos de fondo, texto y borde, asegurando compatibilidad
  con dark mode.

---

#### US-7: Variante de color para el acento de marca

**Como** producto de Somos Voces,
**quiero** que el color de acento (highlight de pestañas, botón primario, borde del callout)
refleje la identidad de Tlacuia,
**para** distinguir visualmente la app de otras herramientas genéricas de Qt.

**Criterios de aceptación:**
- Se define una variable de color de acento (en comentario del QSS, no en C++) que se aplica a:
  - Borde izquierdo del callout (`#desgloseFrame`).
  - Indicador de pestaña activa.
  - Botón "primario" (Agregar, Crear, Continuar).
- El color cumple contraste WCAG AA sobre fondo claro y oscuro.
- Se documenta en el QSS cómo cambiar el color de acento para futuros rebrandings.

---

### Cambios Técnicos Detallados

#### Estructura de archivos

```
app/resources/
├── icons/
│   ├── icon.icns
│   └── icon.ico
├── styles/
│   └── tlacuia.qss       ← NUEVO
└── icons.qrc             ← actualizar para incluir el QSS
```

Actualizar `icons.qrc` (o crear `resources.qrc`) para incluir:
```xml
<qresource prefix="/styles">
    <file>styles/tlacuia.qss</file>
</qresource>
```

#### Carga en `main.cpp`

```cpp
QFile styleFile(":/styles/tlacuia.qss");
if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
    app.setStyleSheet(QTextStream(&styleFile).readAll());
}
```

#### Detección de dark mode

```cpp
// En main.cpp, antes de cargar el QSS:
bool darkMode = (app.palette().color(QPalette::Window).lightness() < 128);
// Pasar como variable de entorno o propiedad dinámica al QSS
// si se requieren overrides específicos para dark mode.
```

> Qt 6.5+ soporta `@media (prefers-color-scheme: dark)` en QSS de forma experimental.
> Para Qt 6.10 (versión actual) se puede usar `QApplication::setProperty("darkMode", true)`
> y condicionales en QSS via `[darkMode="true"] QWidget { ... }`.

---

### Referencia de diseño

- Paleta de Somos Voces: pendiente definir con el equipo (color primario, secundario, acento).
- Referencia de componentes: TlacuiaGCL usa PyQt5 con una hoja de estilos básica en
  `app/resources/styles/style.qss` en el mismo repositorio.
- Guía general: Material Design color tokens como referencia de contraste y jerarquía,
  adaptados a Qt Widgets (no Material Components).

---

### Fuera de scope de este sprint

- Temas intercambiables en tiempo de ejecución (modo claro / oscuro manual desde menú)
- Animaciones o transiciones CSS
- Internacionalización del QSS (fuentes variables por locale)

---

## ✅ Sprint 5 — Comisión Configurable por Concesión [COMPLETADO 2026-03-18]

### Contexto

Actualmente el sistema aplica siempre un 30% de comisión (hardcodeado en `PriceRatios::COMISION`).
En la práctica, distintos distribuidores negocian porcentajes diferentes, por lo que el 30% debe
ser el valor por defecto pero el encargado debe poder cambiarlo concesión por concesión.

---

### Objetivo del Sprint

Permitir que al crear (o editar) una concesión se elija si se trabaja con la comisión estándar
(30%) o con un porcentaje personalizado ingresado por el usuario. Ese porcentaje queda vinculado
a la concesión y se usa automáticamente al calcular cualquier producto que se agregue a ella.

---

### Historias de Usuario

#### US-N: Comisión configurable al crear/editar una concesión

**Como** encargado de librería,
**quiero** poder definir el porcentaje de comisión al registrar una concesión,
**para** reflejar los acuerdos reales con cada distribuidor sin estar limitado al 30%.

**Criterios de aceptación:**
- `NuevaConcesionDialog` incluye un control para la comisión:
  - Radio "Comisión estándar (30%)" seleccionado por defecto.
  - Radio "Otro porcentaje:" + `QDoubleSpinBox` rango `[1.0, 99.0]`, 1 decimal, sufijo `%`.
  - El spinner se habilita solo si se elige "Otro porcentaje".
- El valor se guarda como columna `comision_pct REAL NOT NULL DEFAULT 30.0` en `concesiones`.
- Migración de DB: Vn → Vn+1 agrega `comision_pct` a `concesiones`.
- `ConcesionRecord` añade campo `comisionPct = 30.0`.
- `AgregarProductoDialog` recibe el `comisionPct` de la concesión y lo pasa al `PriceCalculator`.
- `PriceCalculator`: los métodos `calcularConcesionConCFDI` y `calcularConcesionSinCFDI` aceptan
  un parámetro opcional `double comisionPct = PriceRatios::COMISION * 100` (valor en %).
- El panel de detalle de `ConcesionesWidget` muestra la comisión acordada de la concesión.
- El `CorteDialog` muestra la comisión real usada en el encabezado del corte.

---

### Cambios Técnicos (resumen)

- **DB Vn→Vn+1**: `ALTER TABLE concesiones ADD COLUMN comision_pct REAL NOT NULL DEFAULT 30.0`
- **`ConcesionRecord`**: campo `double comisionPct = 30.0`
- **`ConcesionRepository`**: `save()`, `update()`, `mapRow()` incluyen `comision_pct`
- **`NuevaConcesionDialog`**: radio estándar/personalizado + spinner
- **`PriceCalculator`**: firma extendida con `comisionPct` opcional
- **`AgregarProductoDialog`**: recibe `comisionPct` en constructor, lo usa en el cálculo
- **`ConcesionesWidget`**: muestra `Comisión: X%` en el panel de detalle

---

### Fuera de scope de este sprint

- Comisión configurable para productos de tipo "Producto Propio" (solo aplica a concesiones)
- Historial de cambios de comisión por concesión

---

## Sprint 6 — Funcionalidades Pendientes de TlacuiaGCL (Port de Características Core)
### ✅ US-A: Documentos adjuntos a concesiones [COMPLETADO 2026-03-18]
### ✅ US-B: Buscador y filtro en la lista de concesiones [COMPLETADO 2026-03-18]

### Contexto

Comparando el C++ actual con el repositorio original de TlacuiaGCL (Python/PyQt5) en
`https://github.com/leomilenio/TlacuiaGCL`, se identifican las siguientes funcionalidades core
**no portadas aún** que aportan valor operativo real al flujo de trabajo diario:

1. **Documentos adjuntos por concesión** — el original permitía adjuntar PDFs y archivos Excel
   a cada concesión (factura, nota de crédito física, comprobante del distribuidor).
2. **Buscador y filtro en la lista de concesiones** — buscar por emisor, folio o status sin
   tener que desplazarse por toda la lista.
3. **Ganancia estimada en el corte** — el `FinConcesionDialog` original calculaba la ganancia
   por producto: `(qty_vendida × precio_final) − (qty_vendida × costo)`.
4. **Verificador de actualizaciones** — diálogo que consulta un JSON remoto y compara versiones
   para alertar al usuario cuando hay una versión más reciente disponible.

Las herramientas de la pestaña **Tools** del original (`congruence_analisis.py`,
`gslibCut_analisis.py`, `table_extractor.py`) **quedan fuera de scope** por decisión de producto.

---

### Objetivo del Sprint

Cerrar la brecha funcional con TlacuiaGCL portando las cuatro funcionalidades listadas y
elevando la versión del proyecto a `0.5.0`.

---

### Historias de Usuario

#### US-A: Documentos adjuntos a concesiones

**Como** encargado de librería,
**quiero** poder adjuntar archivos PDF o Excel a cada concesión al momento de crearla o editarla,
**para** tener la factura o nota de crédito física del distribuidor vinculada directamente al
registro sin depender de una carpeta física externa.

**Criterios de aceptación:**
- `NuevaConcesionDialog` incluye una sección "Documentos Adjuntos" con:
  - Botón "Seleccionar archivos…" que abre `QFileDialog` (filtro: PDF, XLSX, XLS).
  - `QListWidget` mostrando los archivos seleccionados.
  - Botón "Quitar" para remover un archivo de la lista.
- Los archivos se guardan como blobs en tabla `documentos_concesion` (nueva, migración V5→V6):
  ```sql
  CREATE TABLE IF NOT EXISTS documentos_concesion (
      id           INTEGER PRIMARY KEY AUTOINCREMENT,
      concesion_id INTEGER NOT NULL REFERENCES concesiones(id) ON DELETE CASCADE,
      nombre       TEXT    NOT NULL,
      tipo         TEXT    NOT NULL,   -- 'PDF' | 'Excel'
      contenido    BLOB    NOT NULL,
      fecha_adjunto TEXT   NOT NULL
  );
  ```
- El panel de detalle de `ConcesionesWidget` muestra un `QListWidget` con los documentos
  adjuntos de la concesión seleccionada.
- Doble clic en un documento → escribe el blob a un archivo temporal y lo abre con
  `QDesktopServices::openUrl(QUrl::fromLocalFile(...))`.

**Nuevos archivos:**
- `core/include/core/DocumentoRepository.h`
- `core/src/DocumentoRepository.cpp`

**Archivos a modificar:**
- `core/src/DatabaseManager.cpp` → migración V5→V6
- `app/include/app/NuevaConcesionDialog.h` / `.cpp` → sección de adjuntos
- `app/include/app/ConcesionesWidget.h` / `.cpp` → lista de documentos + doble clic

---

#### US-B: Buscador y filtro en la lista de concesiones

**Como** encargado de librería,
**quiero** poder filtrar la lista de concesiones por nombre del distribuidor, folio o status,
**para** localizar rápidamente una concesión específica sin desplazarme por toda la lista.

**Criterios de aceptación:**
- `ConcesionesWidget` agrega un `QLineEdit` de búsqueda en la parte superior del panel
  izquierdo (placeholder: "Buscar por emisor o folio…").
- El filtro actúa en tiempo real (conectado a `QLineEdit::textChanged`).
- La búsqueda es insensible a mayúsculas/diacríticos (normalización básica).
- Un `QComboBox` al lado permite filtrar por status: "Todos", "Activas", "Vencidas",
  "Finalizadas".
- Al limpiar el buscador se restaura la lista completa.
- El filtro es puramente en memoria (no requiere query adicional a la DB).

**Archivos a modificar:**
- `app/include/app/ConcesionesWidget.h` / `.cpp`

---

#### US-C: Ganancia estimada en el corte

**Como** encargado de librería,
**quiero** ver en el diálogo de corte cuánto gané en esta concesión,
**para** tener una visión rápida de la rentabilidad sin hacer cálculos manuales.

**Criterios de aceptación:**
- `CorteDialog` muestra una fila adicional en la sección de totales:
  `Ganancia estimada (comisión): $XXX.XX`
  calculada como `SUM((precio_final − costo) × cantidad_vendida)` en tiempo real.
- El `CortePdfExporter` incluye la ganancia estimada en la sección de totales del PDF.
- `CorteResult` añade campo `double gananciaEstimada = 0.0`.
- `calcularCorte()` agrega `SUM((precio_final - costo) * cantidad_vendida)` a la query.

**Archivos a modificar:**
- `core/include/core/ProductoRepository.h` → campo `gananciaEstimada` en `CorteResult`
- `core/src/ProductoRepository.cpp` → query extendida
- `app/src/CorteDialog.cpp` → fila ganancia en totales + recalc en tiempo real
- `app/src/CortePdfExporter.cpp` → incluir ganancia en PDF

---

#### US-D: Verificador de actualizaciones

**Como** usuario del sistema,
**quiero** poder verificar si hay una versión más reciente de Tlacuia disponible,
**para** mantener el sistema actualizado con las últimas correcciones y funcionalidades.

**Criterios de aceptación:**
- Opción "Buscar actualizaciones…" en el menú Ayuda de `MainWindow`.
- `UpdateDialog` (`QDialog`) muestra:
  - Versión actual instalada.
  - Notas de la versión actual (leídas de un `version_info.json` embebido en recursos Qt).
  - Botón "Verificar ahora" que hace GET a un JSON remoto en GitHub (URL configurable en
    `AppConfig`).
  - Si hay versión más reciente: muestra número y notas de la nueva versión + botón
    "Ver descargas" que abre el URL en el navegador.
  - Si está actualizado: mensaje "La aplicación está al día (v X.Y.Z)".
- La petición HTTP se realiza con `QNetworkAccessManager` (Qt6::Network).
- Si no hay conexión o falla la petición: error informativo, no crashea.

**Nuevos archivos:**
- `app/include/app/UpdateDialog.h`
- `app/src/UpdateDialog.cpp`
- `app/resources/version_info.json` (embebido en `.qrc`)

**Archivos a modificar:**
- `app/src/MainWindow.cpp` → opción de menú + slot
- `app/CMakeLists.txt` → añadir `Qt6::Network`
- `cmake/FindDependencies.cmake` → añadir `Network` a `find_package`

---

### Cambios Técnicos Resumen

| Área | Cambio |
|------|--------|
| `core/src/DatabaseManager.cpp` | Migración V5→V6: tabla `documentos_concesion` |
| `core/include/core/DocumentoRepository.h` | **NUEVO**: `save()`, `findByConcesion()`, `getContenido()` |
| `core/src/DocumentoRepository.cpp` | **NUEVO** |
| `core/include/core/ProductoRepository.h` | `CorteResult.gananciaEstimada` |
| `core/src/ProductoRepository.cpp` | Query extendida con ganancia |
| `app/include/app/NuevaConcesionDialog.h/.cpp` | Sección adjuntos |
| `app/include/app/ConcesionesWidget.h/.cpp` | Buscador + lista documentos + apertura |
| `app/src/CorteDialog.cpp` | Fila ganancia en totales |
| `app/src/CortePdfExporter.cpp` | Ganancia en PDF |
| `app/include/app/UpdateDialog.h` | **NUEVO** |
| `app/src/UpdateDialog.cpp` | **NUEVO** |
| `app/resources/version_info.json` | **NUEVO** (embebido en QRC) |
| `app/CMakeLists.txt` | `Qt6::Network` |
| `cmake/FindDependencies.cmake` | `Network` en `find_package` |

---

### Orden de implementación

1. Migración DB V5→V6 (`DocumentoRepository` + `DatabaseManager`)
2. `DocumentoRepository` (save, findByConcesion, getContenido)
3. `NuevaConcesionDialog` — sección adjuntos
4. `ConcesionesWidget` — lista de documentos + doble clic
5. `CorteResult.gananciaEstimada` + query extendida
6. `CorteDialog` — fila ganancia en tiempo real
7. `CortePdfExporter` — ganancia en PDF
8. Buscador + filtro de status en `ConcesionesWidget`
9. `UpdateDialog` + opción de menú
10. `version_info.json` + embeder en QRC
11. Elevar versión a `0.5.0` en `CMakeLists.txt` y `main.cpp`

---

### Fuera de scope de este sprint

- Actualización automática (auto-update / patching)
- Firma digital de documentos adjuntos
- Sincronización de concesiones entre dispositivos
- Herramientas de TlacuiaGCL (congruencia, GsLib, extractor de tablas)
