import os

def particionar_documento(archivo_entrada, lineas_por_particion):
    if not os.path.exists(archivo_entrada):
        print(f"Error: No se encontró el archivo '{archivo_entrada}'.")
        return

    with open(archivo_entrada, 'r', encoding='utf-8') as f:
        lineas_originales = f.readlines()

    # 1. Limpiar las cabeceras y pies de página originales
    # Esto evita que la primera partición tenga duplicados de MODULE y PROC
    lineas_limpias = []
    for linea in lineas_originales:
        texto_limpio = linea.strip().upper()
        # Si la línea es una declaración de módulo, procedimiento o cierres, la ignoramos
        if texto_limpio.startswith("MODULE ") or texto_limpio.startswith("PROC ") or texto_limpio == "ENDPROC" or texto_limpio == "ENDMODULE":
            continue
        lineas_limpias.append(linea)

    total_lineas = len(lineas_limpias)
    if total_lineas == 0:
        print("El archivo está vacío o solo contenía cabeceras.")
        return

    numero_particion = 1
    
    # 2. Dividir las líneas de trayectoria puras y armar los nuevos archivos
    for i in range(0, total_lineas, lineas_por_particion):
        bloque_lineas = lineas_limpias[i : i + lineas_por_particion]
        
        # Nombre del archivo de salida
        archivo_salida = f"particion_{numero_particion}.mod"
        
        with open(archivo_salida, 'w', encoding='utf-8') as f_out:
            # Cabecera inyectada de forma segura
            f_out.write("MODULE m1001_1\n")
            f_out.write(f"PROC p1001_{numero_particion}()\n")
            
            # Trayectorias
            for linea in bloque_lineas:
                # Escribimos la línea asegurando que no queden saltos de línea irregulares
                f_out.write(f"{linea.rstrip()}\n")
            
            # Cierre inyectado de forma segura
            f_out.write("ENDPROC\n")
            f_out.write("ENDMODULE\n")
            
        print(f"Generado: '{archivo_salida}' -> PROC p1001_{numero_particion}() con {len(bloque_lineas)} líneas.")
        
        numero_particion += 1

if __name__ == "__main__":
    # Define aquí el nombre de tu archivo general
    archivo_a_dividir = "m1001_1.mod" 
    
    # Define el límite de líneas de trayectoria por partición
    lineas_maximas = 5000 
    
    particionar_documento(archivo_a_dividir, lineas_maximas)