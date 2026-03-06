import re
import os

def transformar_modulo(input_file, output_file):
    # Validamos que el archivo de entrada exista
    if not os.path.exists(input_file):
        print(f"Error: No se encontró el archivo de entrada '{input_file}'.")
        return

    # Leemos el archivo de Autodesk (.mod)
    with open(input_file, 'r', encoding='utf-8') as f:
        contenido = f.read()

    # 1. Extraer las velocidades y forzar el uso de corchetes
    patron_velocidades = r'(?:VAR|PERS|CONST)\s+speeddata\s+([a-zA-Z0-9_]+)\s*:=\s*[\{\[]\s*(.*?)\s*[\}\]]\s*;'
    velocidades = re.findall(patron_velocidades, contenido, re.IGNORECASE)
    
    declaraciones_velocidad = ""
    for nombre, valores in velocidades:
        declaraciones_velocidad += f"  PERS speeddata {nombre}:=[{valores}];\n"

    # 2. Extraer los procedimientos de trayectoria (omitiendo el 'main' original)
    patron_procedimientos = r'PROC\s+([a-zA-Z0-9_]+)\s*\(\s*\).*?(?:\n.*?)*?ENDPROC'
    procedimientos = re.finditer(patron_procedimientos, contenido, re.IGNORECASE|re.DOTALL)
    
    codigo_trayectorias = ""
    llamada_main = ""
    
    for match in procedimientos:
        nombre_proc = match.group(1)
        if nombre_proc.lower() != 'main':
            codigo_trayectorias += match.group(0) + "\n\n"
            if not llamada_main:
                llamada_main = nombre_proc

    # Si la trayectoria no estaba definida explícitamente como un PROC aparte, 
    # la buscamos dentro del bloque main de Autodesk (ej. p1001_1;)
    if not llamada_main:
        patron_llamada = r'(p\d+_\d+);'
        llamada_match = re.search(patron_llamada, contenido)
        if llamada_match:
            llamada_main = llamada_match.group(1)
        else:
            llamada_main = "p1004_1" # Valor de respaldo si no detecta nada

    # 3. Construir la plantilla completa
    plantilla = f"""MODULE Module1
! Trayectoria segura
! IRB2600 - Orientación estable
! -----------------------
{declaraciones_velocidad}
!-----------------Variables------------
  PERS bool is_printing:=FALSE;
  PERS bool estado:=TRUE;
  PERS string comando:="";
  PERS string comn:="0";
  PERS string comn1:="0";
  PERS bool bucle;
  PERS bool Paro:=TRUE;
  PERS num temp_actual := 0; 
  PERS num set_point := 0;
  PERS num Vel_HMI := 0;
!----------------------------------------------
    ! Declaración de la identidad de la interrupción
    VAR intnum int_ParoEmergencia;
    ! Variable que actuará como "Botón" en el FlexPendant
    PERS bool btn_reset := FALSE;
    PERS tooldata Extruder3:=[TRUE,[[17.4,-104.032,90.52],[0.5,0.5,0.5,-0.5]],[1,[0,0,1],[1,0,0,0],0,0,0]];
TASK PERS wobjdata mesa1:=[FALSE,TRUE,"",[[1152.238466449,-151.368483103,237],[1,0,0,0]],[[0,0,0],[1,0,0,0]]];
    !-------------------------------------
    PROC main()
        btn_reset := FALSE;
        is_printing:=FALSE;
        comando:="";
        comn1:="0";
        comn:="0";
        IDelete int_ParoEmergencia;
        CONNECT int_ParoEmergencia WITH trap_Reiniciar;
        IPers btn_reset, int_ParoEmergencia;
        Paro:=TRUE;
        ConfigurarTriggers;
        WHILE Paro DO
            WaitUntil comn1="1";
            !WaitUntil comn="1";
            ! Acceleration and jerk/ramp (percentage)
            AccSet 20,20;
            ! Configurations
            ConfJ\\On;
            ConfL\\Off;
            {llamada_main};
            ConfL\\On;
            ConfJ\\On;
!-----------REINICIO DE VARIABLES--------------------
            comando:="ENFRIAR";
            WaitTime 0.1;
            comando:="";
            comn:="0";
            comn1:="0";
            bucle:=FALSE;
            Paro:=FALSE;
            Stop;
            ENDWHILE
        ENDPROC
    TRAP trap_Reiniciar
        comando:="";
        comn:="0";
        comn1:="0";
        Paro:=FALSE;
        btn_reset:=FALSE;
        ! Detiene el movimiento del robot inmediatamente
        StopMove;
        ! Borra la trayectoria restante que tenía calculada
        ClearPath;
        ! Mensaje para el usuario
        TPWrite "!!! PARO SOLICITADO - REINICIANDO !!!";
        WaitTime 1;
        ! Restablecemos el botón a falso para que no se active de nuevo solo
        ! Restablece el estado de movimiento (necesario tras StopMove)
        StartMove;
        ! Mueve el puntero de programa (PP) a la primera línea de Main
        ExitCycle;
    ENDTRAP

{codigo_trayectorias}
ENDMODULE
"""

    # 4. Generar el nuevo archivo con formato .mod
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(plantilla)
    
    print(f"Conversión exitosa. Archivo generado: '{output_file}'")

if __name__ == "__main__":
    # Define aquí los nombres exactos de tus archivos .mod
    archivo_entrada = "mAutodesk.mod" 
    archivo_salida = "Module3.mod"
    
    transformar_modulo(archivo_entrada, archivo_salida)