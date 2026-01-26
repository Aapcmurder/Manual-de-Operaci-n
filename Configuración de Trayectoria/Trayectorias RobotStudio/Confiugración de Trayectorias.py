import re
import os

# ================= CONFIGURACIÓN =================
ARCHIVO_ENTRADA = "m1006_1.mod"
ARCHIVO_SALIDA = "m1001_1.mod"

# TEXTO DE INICIO
TRIGGER_START = "! rapid-leaky"
# =================================================

HEADER_RAPID = """
    ! --- CONFIGURACIÓN DE TRIGGERS ---
    VAR intnum int_on;
    VAR intnum int_off;
    VAR triggdata trigg_on;
    VAR triggdata trigg_off;
    
    PROC ConfigurarTriggers()
        IDelete int_on;
        IDelete int_off;
        CONNECT int_on WITH TrapEncender;
        CONNECT int_off WITH TrapApagar;
        
        ! trigg_on: Dispara al INICIO del movimiento (\Start)
        TriggInt trigg_on, 0, \Start, int_on;
        ! trigg_off: Dispara al FINAL del movimiento (Distance=0)
        TriggInt trigg_off, 0, int_off;
    ENDPROC
"""

FOOTER_RAPID = """
    ! --- TRAPS ---
    TRAP TrapEncender
        !is_printing := TRUE;
        comando:="EXTRUYE";
    ENDTRAP
    TRAP TrapApagar
        !is_printing := FALSE;
        comando:="STOP";
    ENDTRAP
"""

def procesar_leaky_final(ruta_in, ruta_out):
    if not os.path.exists(ruta_in):
        print(f"Error: No encuentro {ruta_in}")
        return

    print(f"Procesando: '{TRIGGER_START}' activa el primer disparo...")
    with open(ruta_in, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    lineas_procesadas = []
    
    # Banderas
    zona_leaky_activa = False   
    pendiente_encender = False  
    
    header_inyectado = False

    for i, line in enumerate(lines):
        texto = line.strip()
        
        # 1. Inyectar Header/Footer
        if "MODULE" in texto and not header_inyectado:
            lineas_procesadas.append(line)
            lineas_procesadas.append(HEADER_RAPID + "\n")
            header_inyectado = True
            continue
        if "ENDMODULE" in texto:
            lineas_procesadas.append(FOOTER_RAPID + "\n")
            lineas_procesadas.append(line)
            continue

        # 2. DETECTAR LA LÍNEA "! rapid-leaky"
        if TRIGGER_START in texto:
            zona_leaky_activa = True
            # ¡AQUÍ ESTÁ LA CLAVE! 
            # Al ver esta línea, activamos INMEDIATAMENTE la orden de encender el siguiente MoveL.
            pendiente_encender = True 
            lineas_procesadas.append(line)
            continue

        # 3. LÓGICA DENTRO DE LA ZONA
        if zona_leaky_activa:
            
            # A) Si es un COMENTARIO que empieza con "!"
            if texto.startswith("!"):
                pendiente_encender = True # Preparamos el gatillo
                lineas_procesadas.append(line)
                continue

            # B) Si es un MOVIMIENTO (MoveL)
            if texto.startswith("MoveL"):
                if pendiente_encender:
                    # --- CONVERTIR A TRIGG_ON ---
                    nueva = line.replace("MoveL", "TriggL", 1)
                    
                    # Insertar trigg_on
                    match = re.search(r',\s*(z\d+|fine)', nueva, re.IGNORECASE)
                    if match:
                        zona = match.group(0)
                        reemplazo = f", trigg_on{zona}"
                        nueva = nueva.replace(zona, reemplazo, 1)
                    
                    lineas_procesadas.append(nueva)
                    
                    # Ya usamos el gatillo, lo apagamos.
                    pendiente_encender = False 
                else:
                    # Si no hay "!" antes, se queda como MoveL normal
                    lineas_procesadas.append(line)
            else:
                # Otras líneas (ConfJ, MoveAbsJ...)
                lineas_procesadas.append(line)
        
        else:
            # Fuera de zona, todo normal
            lineas_procesadas.append(line)

    # 4. PASO FINAL: APAGAR EL ÚLTIMO MOVIMIENTO
    # Buscamos el último movimiento válido y lo convertimos a APAGADO
    idx = len(lineas_procesadas) - 1
    
    while idx >= 0:
        linea_rev = lineas_procesadas[idx]
        
        if "MoveL" in linea_rev or "TriggL" in linea_rev:
            # Convertir a TriggL Off + Fine
            nueva_final = linea_rev.replace("MoveL", "TriggL", 1)
            
            # Si por error ya tenía trigg_on (porque había un ! antes), lo cambiamos a trigg_off
            if "trigg_on" in nueva_final:
                nueva_final = nueva_final.replace("trigg_on", "trigg_off")
            else:
                # Si no, insertamos trigg_off
                match = re.search(r',\s*(z\d+|fine)', nueva_final, re.IGNORECASE)
                if match:
                    nueva_final = nueva_final.replace(match.group(0), ", trigg_off, fine")
            
            # Asegurar FINE (crítico para terminar bien)
            if "fine" not in nueva_final:
                # Si hay alguna zona z..., la cambiamos a fine
                nueva_final = re.sub(r',\s*z\d+', ", fine", nueva_final)

            lineas_procesadas[idx] = nueva_final
            print("-> Último movimiento configurado como trigg_off + fine.")
            break
        
        idx -= 1

    with open(ruta_out, 'w', encoding='utf-8') as f:
        f.writelines(lineas_procesadas)

    print("-" * 30)
    print(f"¡HECHO! Archivo: {ruta_out}")
    print("Comportamiento:")
    print("1. '! rapid-leaky' -> Activa el PRIMER TriggL.")
    print("2. '!' cualquiera  -> Activa el SIGUIENTE TriggL.")
    print("3. Fin del archivo -> Apaga (TriggL Off + Fine).")
    print("-" * 30)

if __name__ == "__main__":
    procesar_leaky_final(ARCHIVO_ENTRADA, ARCHIVO_SALIDA)