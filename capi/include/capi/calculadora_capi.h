/*
 * calculadora_capi.h
 * C API plano sobre core_lib. Seguro para P/Invoke desde .NET.
 * Todos los tipos son POD. Ningun tipo de C++ cruza este limite.
 *
 * Ejemplo de uso desde C#:
 *   [DllImport("calculadora_shared")]
 *   static extern CApiResult calc_producto_propio(double precioFinal);
 */

#ifndef CALCULADORA_CAPI_H
#define CALCULADORA_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
  #ifdef CAPI_EXPORTS
    #define CAPI_API __declspec(dllexport)
  #else
    #define CAPI_API __declspec(dllimport)
  #endif
#else
  #define CAPI_API __attribute__((visibility("default")))
#endif

typedef struct {
    double precio_final;
    double costo;
    double comision;
    double iva_trasladado;
    double iva_acreditable;
    double iva_neto_sat;
    int    escenario;      /* 0 = propio, 1 = concesion */
    int    tiene_cfdi;     /* 0 = sin CFDI, 1 = con CFDI */
    int    is_valid;       /* 1 = exito, 0 = error */
    char   error_message[256];
} CApiResult;

/* Escenario 1a: Producto Propio sin CFDI de compra (ivaAcreditable = 0) */
CAPI_API CApiResult calc_producto_propio(double precio_final);

/* Escenario 1b: Producto Propio con CFDI de compra al proveedor (LIVA Art. 4-5)
 * ivaAcreditable = costo_implicito (54%) * 16% */
CAPI_API CApiResult calc_producto_propio_con_cfdi(double precio_final);

/* Escenario 2a: Concesion con CFDI (IVA acreditable) */
CAPI_API CApiResult calc_concesion_con_cfdi(double precio_neto_proveedor);

/* Escenario 2b: Concesion sin CFDI (IVA no recuperable) */
CAPI_API CApiResult calc_concesion_sin_cfdi(double precio_neto_proveedor);

/* Ciclo de vida de la libreria */
CAPI_API void calculadora_init(void);
CAPI_API void calculadora_shutdown(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CALCULADORA_CAPI_H */
