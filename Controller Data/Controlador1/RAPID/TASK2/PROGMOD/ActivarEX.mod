MODULE ActivarEX
    ! Variable global del socket
    VAR socketdev sd1;
    ! Variable local para controlar el estado de pausa
    VAR bool robot_detenido := FALSE;
    
    PROC abrircomuni()
        VAR rawbytes buffer_basura;
        ! Creamos una bandera para controlar el bucle
        VAR bool limpieza_terminada := FALSE;
        
        SocketClose sd1;
        SocketCreate sd1;
        SocketConnect sd1, "200.126.19.237",4012;
        
        ! --- LIMPIEZA DE BUFFER (FLUSH) ---
        ! El bucle se ejecutará mientras NO hayamos terminado la limpieza
        WHILE limpieza_terminada = FALSE DO
            ! Intentamos leer. 
            ! Si hay datos: Pasa a la siguiente línea y repite el bucle.
            ! Si NO hay datos (TimeOut): Salta al ERROR.
            SocketReceive sd1, \RawData:=buffer_basura, \Time:=0.1;
        ENDWHILE
        
        TPWrite "Conexión establecida y buffer limpio.";
        
    ERROR
        IF ERRNO = ERR_SOCK_TIMEOUT THEN
             ! El Timeout es BUENA SEÑAL: significa que ya no hay datos basura.
             ! 1. Cambiamos la bandera para salir del While
             limpieza_terminada := TRUE;
             
             ! 2. TRYNEXT salta a la instrucción después de SocketReceive (ENDWHILE)
             ! Al evaluar el WHILE, como limpieza_terminada es TRUE, sale del bucle.
             TRYNEXT; 
        ELSE
             ! Si es otro error real, cerramos
             SocketClose sd1;
             RAISE;
        ENDIF
    ENDPROC

    PROC cerrarcomuni()
        TPWrite "Cerrando conexión con Arduino...";
        SocketClose sd1;
    ERROR
        TRYNEXT;
    ENDPROC

    PROC leer_datos()
        VAR num n_len;
        s_mensaje_completo:="";

        ! 1. Limpiar buffer de variable
        ClearRawBytes receive_string;
        
        ! 2. Recibir datos con TimeOut corto
        SocketReceive sd1, \RawData:=receive_string, \Time:=0.5;
        
        ! 3. Calcular longitud REAL recibida
        n_len := RawBytesLen(receive_string);
        
        ! Si no hay datos, salimos (Evita procesar vacíos)
        IF n_len = 0 RETURN;

        ! 4. Convertir a string para visualizar
        UnpackRawBytes receive_string, 1, s_mensaje_completo, \ASCII:=n_len;
        TPWrite " Msg: " + s_mensaje_completo;
        ! (Opcional) Debug: Ver qué llega realmente y su tamaño
        !TPWrite "Len: " + ValToStr(n_len) + " Msg: " + s_mensaje_completo;

        !------------ ANÁLISIS DE MENSAJES (ORDENADO POR TAMAÑO) ---------
        
        ! PRIORIDAD 1: TELEMETRÍA (Mensaje largo >= 24 bytes)
        ! Corregido: Debe ser MAYOR O IGUAL (>=), no menor.
        IF n_len >= 24 THEN
            UnpackRawBytes receive_string, 1, s_estado, \ASCII:=8;
            UnpackRawBytes receive_string, 9, s_temp, \ASCII:=8;
            UnpackRawBytes receive_string, 17, s_setpoint, \ASCII:=8;
            
            ok_val := StrToVal(s_temp, n_temp);
            ok_val := StrToVal(s_setpoint, n_setpoint);
            
            ! Asignar a variables PERS
            
            temp_actual := n_temp;
            !set_point := n_setpoint;
            !TPWrite "Datos: " + ValToStr(temp_actual) + " / " + VAlToStr(set_point);

        ! PRIORIDAD 2: COMANDOS DE ESTADO (Mensajes cortos)
        ! Usamos StrFind > 0 para encontrar la palabra clave
        ELSE 
            IF n_len >= 8 THEN
               IF StrPart(s_mensaje_completo, 1, 8) = "CALIENTE" THEN
                   comn1 := "1";
                   TPWrite ">> ARDUINO LISTO (RDY)";
                   IF robot_detenido = TRUE THEN
                       TPWrite ">> RECUPERADO. Reanudando...";
                       StartMove;
                       robot_detenido := FALSE;
                   ENDIF
               ENDIF
            ENDIF
            ! 2. Comprobamos si tiene al menos 6 letras para "PAUSED"
            IF n_len >= 6 THEN
               IF StrPart(s_mensaje_completo, 1, 6) = "PAUSED" THEN
                   IF robot_detenido = FALSE THEN
                       TPWrite "!!! ALERTA: PAUSA TERMICA !!!";
                       StopMove;
                       robot_detenido := TRUE;
                   ENDIF
               ENDIF
            ENDIF
            ! 3. Comprobamos si tiene al menos 5 letras para "ERROR" o "FATAL"
            IF n_len >= 5 THEN
               IF StrPart(s_mensaje_completo, 1, 5) = "ERROR" OR StrPart(s_mensaje_completo, 1, 5) = "FATAL" THEN
                   TPWrite "!!! ERROR FATAL !!!";
                   StopMove;
                   Paro := TRUE;
               ENDIF
            ENDIF
        ENDIF
    ERROR
        IF ERRNO = ERR_SOCK_TIMEOUT THEN
            TRYNEXT;
        ELSE
            TPWrite "Error Socket: " + ValToStr(ERRNO);
            TRYNEXT;
        ENDIF
    ENDPROC
ENDMODULE